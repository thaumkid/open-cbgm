/*
 * textual_flow.cpp
 *
 *  Created on: Dec 24, 2019
 *      Author: jjmccollum
 */

#include <iostream>
#include <cstring>
#include <string>
#include <list>
#include <vector>
#include <set> //for small sets keyed by readings
#include <unordered_set> //for large sets keyed by witnesses
#include <map> //for small maps keyed by readings
#include <unordered_map> //for large maps keyed by witnesses

#include "pugixml.h"
#include "roaring.hh"
#include "textual_flow.h"
#include "witness.h"
#include "variation_unit.h"

using namespace std;

/**
 * Default constructor.
 */
textual_flow::textual_flow() {

}

/**
 * Constructs a textual flow instance from a variation unit
 * and a list of witnesses whose potential ancestors have been set.
 */
textual_flow::textual_flow(const variation_unit & vu, const list<witness> & witnesses) {
	//Copy the label and connectivity from the variation unit:
	label = vu.get_label();
	connectivity = vu.get_connectivity();
	//Initialize the textual flow graph as empty:
	graph.vertices = list<textual_flow_vertex>();
	graph.edges = list<textual_flow_edge>();
	//Get a copy of the variation unit's reading support map:
	unordered_map<string, list<string>> reading_support = vu.get_reading_support();
	//Add vertices and edges for each witness in the input list:
	for (witness wit : witnesses) {
		//Get the witness's ID and a list of any readings it has at this variation unit:
		string wit_id = wit.get_id();
		list<string> wit_rdgs = reading_support.find(wit_id) != reading_support.end() ? reading_support.at(wit_id) : list<string>();
		//Add a vertex for this witness to the graph:
		textual_flow_vertex v;
		v.id = wit_id;
		v.rdgs = wit_rdgs;
		graph.vertices.push_back(v);
		//If this witness has no potential ancestors (i.e., if it has equal priority to the Ausgangstext),
		//then there are no edges to add, and we can continue:
		list<string> potential_ancestor_ids = wit.get_potential_ancestor_ids();
		if (potential_ancestor_ids.empty()) {
			continue;
		}
		//Otherwise, identify this witness's textual flow ancestor for this variation unit:
		string textual_flow_ancestor_id;
		int con = 0;
		flow_type type = flow_type::NONE;
		//If the witness is extant, then attempt to find an ancestor within the connectivity limit that agrees with it:
		if (!wit_rdgs.empty()) {
			con = 0;
			for (string potential_ancestor_id : potential_ancestor_ids) {
				//If we reach the connectivity limit, then exit the loop early:
				if (con == connectivity) {
					break;
				}
				//If this potential ancestor agrees with the current witness here, then we're done:
				bool agree = false;
				if (reading_support.find(potential_ancestor_id) != reading_support.end()) {
					list<string> potential_ancestor_rdgs = reading_support.at(potential_ancestor_id);
					for (string wit_rdg : wit_rdgs) {
						for (string potential_ancestor_rdg : potential_ancestor_rdgs) {
							if (wit_rdg == potential_ancestor_rdg) {
								agree = true;
							}
						}
					}
				}
				if (agree) {
					textual_flow_ancestor_id = potential_ancestor_id;
					type = wit_rdgs.size() > 1 ? flow_type::AMBIGUOUS : flow_type::EQUAL;
					break;
				}
				//Otherwise, continue through the loop:
				con++;
			}
		}
		//If no textual flow ancestor has been found (either because the witness is lacunose or it does not have a close ancestor that agrees with it),
		//then its first extant potential ancestor is its textual flow ancestor:
		if (textual_flow_ancestor_id.empty()) {
			con = 0;
			for (string potential_ancestor_id : potential_ancestor_ids) {
				if (reading_support.find(potential_ancestor_id) != reading_support.end()) {
					textual_flow_ancestor_id = potential_ancestor_id;
					break;
				}
				con++;
			}
			//Set the type based on whether or not this witness is extant:
			type = !wit_rdgs.empty() ? flow_type::CHANGE : flow_type::LOSS;
		}
		//Add an edge to the graph connecting the current witness to its textual flow ancestor:
		textual_flow_edge e;
		e.descendant = wit_id;
		e.ancestor = textual_flow_ancestor_id;
		e.connectivity = con;
		e.type = type;
		graph.edges.push_back(e);
	}
}

/**
 * Default destructor.
 */
textual_flow::~textual_flow() {

}

/**
 * Returns the label of this textual_flow instance.
 */
string textual_flow::get_label() const {
	return label;
}

/**
 * Returns the connectivity of this textual_flow instance.
 */
int textual_flow::get_connectivity() const {
	return connectivity;
}

/**
 * Returns the textual flow diagram of this textual_flow instance.
 */
textual_flow_graph textual_flow::get_graph() const {
	return graph;
}

/**
 * Given an output stream, writes a complete textual flow diagram to output in .dot format.
 */
void textual_flow::textual_flow_to_dot(ostream & out) {
	//Add the graph first:
	out << "digraph textual_flow {\n";
	//Add a line indicating that nodes do not have any shape:
	out << "\tnode [shape=plaintext];\n";
	//Add a box node indicating the label of this variation_unit:
	out << "\tlabel [shape=box, label=\"" << label << "\\nCon=" << connectivity << "\"];\n";
	//Add all of the graph nodes, keeping track of their numerical indices:
	unordered_map<string, int> id_to_index = unordered_map<string, int>();
	for (textual_flow_vertex v : graph.vertices) {
		//Map the ID of this vertex to its numerical index:
		string wit_id = v.id;
		list<string> wit_rdgs = v.rdgs;
		id_to_index[wit_id] = id_to_index.size();
		int wit_index = id_to_index[wit_id];
		out << "\t" << wit_index;
		//Format the node based on its readings list:
		if (wit_rdgs.empty()) {
			//The witness is lacunose at this variation unit:
			out << " [label=\"" << wit_id << "\", color=gray, shape=ellipse, style=dashed]";
		}
		else if (wit_rdgs.size() == 1) {
			//The witness has exactly one reading at this variation unit:
			out << " [label=\"" << wit_id << "\"]";
		}
		else {
			//The witness's support is ambiguous at this variation unit:
			out << " [label=\"" << wit_id << "\", shape=ellipse, peripheries=2]";
		}
		out << ";\n";
	}
	//Add all of the graph edges:
	for (textual_flow_edge e : graph.edges) {
		//Get the indices corresponding to the endpoints' IDs:
		int ancestor_index = id_to_index[e.ancestor];
		int descendant_index = id_to_index[e.descendant];
		out << "\t";
		if (e.type == flow_type::AMBIGUOUS) {
			//Ambiguous changes are indicated by double-lined arrows:
			out << ancestor_index << " => " << descendant_index;
		}
		else {
			//All other changes are indicated by single-lined arrows:
			out << ancestor_index << " -> " << descendant_index;
		}
		//Conditionally format the edge:
		out << " [";
		if (e.connectivity > 0) {
			//If the connectivity index is not direct (i.e., 0), then print it in one-based format:
			out << "label=\"" << (e.connectivity + 1) << "\", fontsize=10, ";
		}
		if (e.type == flow_type::CHANGE) {
			//Highlight changes in readings in blue:
			out << "color=blue";
		}
		else if (e.type == flow_type::LOSS) {
			//Highlight losses with dashed gray arrows:
			out << "color=gray, style=dashed";
		}
		else {
			//Equal and ambiguous textual flows are indicated by solid black arrows:
			out << "color=black";
		}
		out << "];\n";
	}
	out << "}" << endl;
	return;
}

/**
 * Given a reading ID and an output stream,
 * writes a coherence in attestations diagram for that reading to output in .dot format.
 */
void textual_flow::coherence_in_attestations_to_dot(const string & rdg, ostream & out) {
	//Add the graph first:
	out << "digraph textual_flow_diagram {\n";
	//Add a line indicating that nodes do not have any shape:
	out << "\tnode [shape=plaintext];\n";
	//Add a box node indicating the label of this variation_unit and the selected reading:
	out << "\tlabel [shape=box, label=\"" << label << rdg << "\\nCon=" << connectivity << "\"];\n";
	//Maintain a map of node IDs to numerical indices
	//and a vector of vertex data structures:
	unordered_map<string, int> id_to_index = unordered_map<string, int>();
	vector<textual_flow_vertex> vertices = vector<textual_flow_vertex>();
	for (textual_flow_vertex v : graph.vertices) {
		//Map the ID of this vertex to its numerical index:
		string wit_id = v.id;
		list<string> wit_rdgs = v.rdgs;
		id_to_index[wit_id] = id_to_index.size();
		vertices.push_back(v);
	}
	//Now draw the primary set of vertices corresponding to witnesses with the input reading:
	unordered_set<string> primary_set = unordered_set<string>();
	for (textual_flow_vertex v : graph.vertices) {
		//If this witness does not have the specified reading, then skip it:
		string wit_id = v.id;
		list<string> wit_rdgs = v.rdgs;
		bool has_rdg = false;
		for (string wit_rdg : wit_rdgs) {
			if (wit_rdg == rdg) {
				has_rdg = true;
				break;
			}
		}
		if (!has_rdg) {
			continue;
		}
		//Otherwise, draw a vertex with its numerical index:
		int wit_ind = id_to_index.at(wit_id);
		out << "\t" << wit_ind;
		if (wit_rdgs.size() == 1) {
			//The witness has exactly one reading at this variation unit:
			out << " [label=\"" << wit_id << "\"]";
		}
		else {
			//The witness's support is ambiguous at this variation unit:
			out << " [label=\"" << wit_id << "\", shape=ellipse, peripheries=2]";
		}
		out << ";\n";
		primary_set.insert(wit_id);
	}
	//Then add a secondary set of vertices for ancestors of these witnesses with a different reading:
	unordered_set<string> secondary_set = unordered_set<string>();
	for (textual_flow_edge e : graph.edges) {
		//Get the endpoints' IDs:
		string ancestor_id = e.ancestor;
		string descendant_id = e.descendant;
		//If the descendant is not in the primary vertex set, or the ancestor is, then skip:
		if (primary_set.find(descendant_id) == primary_set.end() || primary_set.find(ancestor_id) != primary_set.end()) {
			continue;
		}
		//Otherwise, we have an ancestor with a reading other than the specified one;
		//skip it if we've added it already:
		if (secondary_set.find(ancestor_id) != secondary_set.end()) {
			continue;
		}
		//If it's new, then serialize its reading(s) for labeling purposes:
		int ancestor_ind = id_to_index.at(ancestor_id);
		textual_flow_vertex v = vertices[ancestor_ind];
		list<string> ancestor_rdgs = v.rdgs;
		string serialized = "";
		for (string ancestor_rdg : ancestor_rdgs) {
			if (ancestor_rdg != ancestor_rdgs.front()) {
				serialized += ", ";
			}
			serialized += ancestor_rdg;
		}
		//Then draw a vertex with its numerical index:
		out << "\t" << ancestor_ind;
		if (ancestor_rdgs.size() == 1) {
			//The witness has exactly one reading at this variation unit:
			out << " [label=\"" << serialized << ": " << ancestor_id << "\", color=blue, shape=ellipse, type=dashed]";
		}
		else {
			//The witness's support is ambiguous at this variation unit:
			out << " [label=\"" << serialized << "\", color=blue, shape=ellipse, peripheries=2, type=dashed]";
		}
		out << ";\n";
		secondary_set.insert(ancestor_id);
	}
	//Add all of the graph edges:
	for (textual_flow_edge e : graph.edges) {
		//Get the endpoints' IDs:
		string ancestor_id = e.ancestor;
		string descendant_id = e.descendant;
		//If the descendant is not in the primary vertex set, then skip this edge:
		if (primary_set.find(descendant_id) == primary_set.end()) {
			continue;
		}
		//Otherwise, get the indices of the endpoints:
		int ancestor_ind = id_to_index.at(ancestor_id);
		int descendant_ind = id_to_index.at(descendant_id);
		out << "\t";
		if (e.type == flow_type::AMBIGUOUS) {
			//Ambiguous changes are indicated by double-lined arrows:
			out << ancestor_ind << " => " << descendant_ind;
		}
		else {
			//All other changes are indicated by single-lined arrows:
			out << ancestor_ind << " -> " << descendant_ind;
		}
		//Conditionally format the edge:
		out << " [";
		if (e.connectivity > 0) {
			//If the connectivity index is not direct (i.e., 0), then print it in one-based format:
			out << "label=\"" << (e.connectivity + 1) << "\", fontsize=10, ";
		}
		if (e.type == flow_type::CHANGE) {
			//Highlight changes in readings in blue:
			out << "color=blue";
		}
		else if (e.type == flow_type::LOSS) {
			//Highlight losses with dashed gray arrows:
			out << "color=gray, style=dashed";
		} else {
			//Textual flow involving no change is indicated by a black arrow:
			out << "color=black";
		}
		out << "];\n";
	}
	out << "}" << endl;
	return;
}

/**
 * Given an output stream, writes a coherence in variant passages diagram to output in .dot format.
 */
void textual_flow::coherence_in_variant_passages_to_dot(ostream & out) {
	//Add the graph first:
	out << "digraph textual_flow_diagram {\n";
	//Add a line indicating that nodes do not have any shape:
	out << "\tnode [shape=plaintext];\n";
	//Add a box node indicating the label of this variation_unit:
	out << "\tlabel [shape=box, label=\"" << label << "\\nCon=" << connectivity << "\"];\n";
	//Maintain a map of node IDs to numerical indices
	//and a vector of vertex data structures:
	unordered_map<string, int> id_to_index = unordered_map<string, int>();
	vector<textual_flow_vertex> vertices = vector<textual_flow_vertex>();
	for (textual_flow_vertex v : graph.vertices) {
		//Map the ID of this vertex to its numerical index:
		string wit_id = v.id;
		list<string> wit_rdgs = v.rdgs;
		id_to_index[wit_id] = id_to_index.size();
		vertices.push_back(v);
	}
	//Maintain a map of support lists for each reading:
	map<string, list<string>> clusters = map<string, list<string>>();
	for (textual_flow_vertex v : graph.vertices) {
		string wit_id = v.id;
		list<string> wit_rdgs = v.rdgs;
		for (string wit_rdg : wit_rdgs) {
			//Add an empty list of witness IDs for this reading, if it hasn't been encountered yet:
			if (clusters.find(wit_rdg) == clusters.end()) {
				clusters[wit_rdg] = list<string>();
			}
			clusters[wit_rdg].push_back(wit_id);
		}
	}
	//Maintain a set of IDs for nodes between which there exists an edge of flow type CHANGE:
	unordered_set<string> change_wit_ids = unordered_set<string>();
	for (textual_flow_edge e : graph.edges) {
		if (e.type == flow_type::CHANGE) {
			change_wit_ids.insert(e.ancestor);
			change_wit_ids.insert(e.descendant);
		}
	}
	//Add a cluster for each reading, including all of the nodes it contains:
	for (pair<string, list<string>> kv : clusters) {
		string rdg = kv.first;
		list<string> cluster = kv.second;
		out << "\tsubgraph cluster_" << rdg << " {\n";
		out << "\t\tlabeljust=\"c\";\n";
		out << "\t\tlabel=\"" << rdg << "\";\n";
		for (string wit_id : cluster) {
			//If this witness is not at either end of a CHANGE flow edge, then skip it:
			if (change_wit_ids.find(wit_id) == change_wit_ids.end()) {
				continue;
			}
			//Otherwise, get the numerical index for this witness's vertex and the vertex itself:
			int wit_ind = id_to_index.at(wit_id);
			textual_flow_vertex v = vertices[wit_ind];
			out << "\t\t" << wit_ind;
			if (v.rdgs.size() == 1) {
				//The witness has exactly one reading at this variation unit:
				out << " [label=\"" << wit_id << "\"]";
			}
			else {
				//The witness's support is ambiguous at this variation unit:
				out << " [label=\"" << wit_id << "\", shape=ellipse, peripheries=2]";
			}
			out << ";\n";
		}
		out << "\t}\n";
	}
	//Finally, add the "CHANGE" edges:
	for (textual_flow_edge e : graph.edges) {
		//Only include vertices that are at endpoints of a "CHANGE" edge:
		if (e.type != flow_type::CHANGE) {
			continue;
		}
		//Get the indices corresponding to the endpoints' IDs:
		int ancestor_ind = id_to_index.at(e.ancestor);
		int descendant_ind = id_to_index.at(e.descendant);
		out << "\t";
		out << ancestor_ind << " -> " << descendant_ind;
		//Conditionally format the edge:
		out << " [";
		if (e.connectivity > 0) {
			//If the connectivity index is not direct (i.e., 0), then print it in one-based format:
			out << "label=\"" << (e.connectivity + 1) << "\", fontsize=10, ";
		}
		//Highlight changes in readings in blue:
		out << "color=blue";
		out << "];\n";
	}
	out << "}" << endl;
	return;
}

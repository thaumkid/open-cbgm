/*
 * variation_unit.cpp
 *
 *  Created on: Oct 24, 2019
 *      Author: jjmccollum
 */

#include <iostream>
#include <cstring>
#include <string>
#include <list>
#include <unordered_map>

#include "pugixml.h"
#include "roaring.hh"
#include "witness.h"
#include "variation_unit.h"
#include "local_stemma.h"

using namespace std;

/**
 * Default constructor.
 */
variation_unit::variation_unit() {

}

/**
 * Constructs a variation unit from an <app/> XML element and its numerical index.
 */
variation_unit::variation_unit(unsigned int variation_unit_index, const pugi::xml_node xml) {
	//Populate the index:
	index = variation_unit_index;
	//Populate the label:
	label = (xml.child("label") && xml.child("label").text()) ? xml.child("label").text().get() : "";
	//Populate the witness-to-readings map:
	reading_support = unordered_map<string, list<int>>();
	int reading_ind = -1;
	for (pugi::xml_node rdg : xml.children("rdg")) {
		//Each <rdg/> element contains the content of the variant reading itself,
		//a required attribute containing the supporting witnesses' IDs,
		//an optional "n" attribute containing the reading's ID,
		//and an optional attribute indicating the variant type (e.g., "substantive", "orthographic", or "defective"):
		string type = rdg.attribute("type") ? rdg.attribute("type").value() : "substantive";
		if (type != "orthographic" && type != "defective") {
			//If the reading is neither an orthographic nor a defective sub-variant,
			//then increment the index for the current reading
			//(sub-variants are assumed to follow their parent reading in the XML tree):
			reading_ind++;
		}
		//If the reading has no "n" attribute, then use its index as a default:
		string rdg_id = rdg.attribute("n") ? rdg.attribute("n").value() : to_string(reading_ind);
		//Now split the witness support attribute into a list of witness strings:
		list<string> wits = list<string>();
		const string wit_string = rdg.attribute("wit").value();
		char * wit_chars = new char[wit_string.length() + 1];
		strcpy(wit_chars, wit_string.c_str());
		const char delim[] = " ";
		char * token = strtok(wit_chars, delim);
		while (token) {
			//Strip each reference of the "#" character and add the resulting ID to the witnesses set:
			string wit = string(token);
			wit = wit.erase(0, 1);
			wits.push_back(wit);
			token = strtok(NULL, delim); //iterate to the next token
		}
		//Then process each witness with this reading:
		for (string wit : wits) {
			//Add an empty list for each reading we haven't encountered yet:
			if (reading_support.find(wit) == reading_support.end()) {
				reading_support[wit] = list<int>();
			}
			reading_support[wit].push_back(reading_ind);
		}
	}
	//Set the connectivity value:
	pugi::xpath_node numeric_path = xml.select_node("fs/f[@name=\"connectivity\"]/numeric");
	if (numeric_path) {
		pugi::xml_node numeric = numeric_path.node();
		connectivity = numeric.attribute("value") ? numeric.attribute("value").as_int() : connectivity;
	}
	//Initialize the local stemma graph:
	pugi::xml_node stemma_node = xml.child("graph");
	if (stemma_node) {
		//The <graph/> element should contain the local stemma for this variation unit:
		stemma = local_stemma(label, stemma_node);
	}
	//Initialize the textual flow graph as empty:
	graph.vertices = list<textual_flow_vertex>();
	graph.edges = list<textual_flow_edge>();
}

variation_unit::~variation_unit() {

}

/**
 * Returns the numerical index of this variation unit.
 */
unsigned int variation_unit::get_index() {
	return index;
}

/**
 * Returns the label of this variation_unit.
 */
string variation_unit::get_label() {
	return label;
}

/**
 * Returns the number of (substantive) readings in this variation_unit.
 */
int variation_unit::size() {
	return reading_support.size();
}

/**
 * Returns a vector of reading IDs.
 */
vector<string> variation_unit::get_readings() {
	return readings;
}

/**
 * Returns the reading support set of this variation_unit.
 */
unordered_map<string, list<int>> variation_unit::get_reading_support() {
	return reading_support;
}

/**
 * Returns the connectivity of this variation_unit.
 */
int variation_unit::get_connectivity() {
	return connectivity;
}

/**
 * Returns the local stemma of this variation_unit.
 */
local_stemma variation_unit::get_local_stemma() {
	return stemma;
}

/**
 * Returns the textual flow diagram of this variation unit.
 */
textual_flow_graph variation_unit::get_textual_flow_diagram() {
	return graph;
}

/**
 * Given a witness, adds a vertex representing it and an edge representing its relationship to its ancestor to the textual flow diagram graph
 * and adds the IDs of its textual flow parents to its set of textual flow ancestors.
 */
void variation_unit::calculate_textual_flow_for_witness(witness &w) {
	string wit_id = w.get_id();
	//Check if this witness is lacunose:
	bool extant = (reading_support.find(wit_id) != reading_support.end());
	//Add a vertex for this witness to the graph:
	textual_flow_vertex v;
	v.id = wit_id;
	v.extant = extant;
	graph.vertices.push_back(v);
	//If this witness has no potential ancestors (i.e., if it is the Ausgangstext), then we're done:
	if (w.get_potential_ancestor_ids().size() == 0) {
		return;
	}
	//Otherwise, identify this witness's textual flow ancestor for this variation unit:
	string textual_flow_ancestor_id;
	int con;
	flow_type type = flow_type::NONE;
	//If the witness is extant, then attempt to find an ancestor within the connectivity limit that agrees with it:
	if (extant) {
		con = 0;
		for (string potential_ancestor_id : w.get_potential_ancestor_ids()) {
			//If we reach the connectivity limit, then exit the loop early:
			if (con == connectivity) {
				break;
			}
			//If this potential ancestor agrees with the current witness here, then we're done:
			if (w.get_agreements_for_witness(potential_ancestor_id).contains(index)) {
				textual_flow_ancestor_id = potential_ancestor_id;
				type = flow_type::EQUAL;
				//Add the textual flow ancestor to this witness's set of textual flow ancestors:
				w.add_textual_flow_ancestor_id(textual_flow_ancestor_id);
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
		for (string potential_ancestor_id : w.get_potential_ancestor_ids()) {
			if (reading_support.find(textual_flow_ancestor_id) != reading_support.end()) {
				textual_flow_ancestor_id = potential_ancestor_id;
				break;
			}
			con++;
		}
		//Set the type based on whether or not this witness is extant:
		type = extant ? flow_type::CHANGE : flow_type::LOSS;
		//Add the textual flow ancestor to this witness's set of textual flow ancestors:
		w.add_textual_flow_ancestor_id(textual_flow_ancestor_id);
		//If this witness is extant, then find the closest ancestor with a reading that explains its reading here,
		//and add it to this witness's set of textual flow ancestors, as well:
		if (extant) {
			bool reading_explained = false;
			for (string potential_ancestor_id : w.get_potential_ancestor_ids()) {
				if (w.get_explained_readings_for_witness(potential_ancestor_id).contains(index)) {
					w.add_textual_flow_ancestor_id(potential_ancestor_id);
					reading_explained = true;
					break;
				}
			}
			if (!reading_explained) {
				//If no such ancestor can be found, then the global stemma will not be constructible;
				//report a warning to the user:
				cout << "Warning: witness " << wit_id << " has no potential ancestors that can explain its reading at variation unit " << label << "; consider modifying the local stemma for this variation unit." << endl;
			}
		}
	}
	//Add an edge to the graph connecting the current witness to its textual flow ancestor:
	textual_flow_edge e;
	e.descendant = wit_id;
	e.ancestor = textual_flow_ancestor_id;
	e.connectivity = con;
	e.type = type;
	graph.edges.push_back(e);
	return;
}

/**
 * Given a map of witness IDs to witnesses, constructs the textual flow diagram for this variation_unit
 * and modifies each witness's set of textual flow ancestors.
 */
void variation_unit::calculate_textual_flow(unordered_map<string, witness> &witnesses_by_id) {
	graph.vertices = list<textual_flow_vertex>();
	graph.edges = list<textual_flow_edge>();
	//Add a node for each witness:
	for (pair<string, witness> kv : witnesses_by_id) {
		witness w = kv.second;
		calculate_textual_flow_for_witness(w);
	}
	return;
}

/**
 * Given an output stream, writes this variation_unit's textual flow diagram graph to output in .dot format.
 */
void variation_unit::textual_flow_diagram_to_dot(ostream & out) {
	//Add the graph first:
	out << "digraph textual_flow_diagram {\n";
	//Add lines specifying the font and font size:
	out << "\tgraph [fontname = \"helvetica\", fontsize=15];\n";
	out << "\tnode [fontname = \"helvetica\", fontsize=15];\n";
	out << "\tedge [fontname = \"helvetica\", fontsize=15];\n";
	//Add a line indicating that nodes do not have any shape:
	out << "\tnode [shape=plaintext];\n";
	//Add a box node indicating the label of this variation_unit:
	out << "\tlabel [shape=box, label=\"" << label << "\\nCon=" << connectivity << "\"];\n";
	//Add all of the graph nodes, keeping track of their numerical indices:
	unordered_map<string, int> id_to_index = unordered_map<string, int>();
	for (textual_flow_vertex v : graph.vertices) {
		//Map the ID of this vertex to its numerical index:
		string wit_id = v.id;
		id_to_index[wit_id] = id_to_index.size();
		int wit_index = id_to_index[wit_id];
		out << "\t" << wit_index;
		//Format the node based on whether the witness is extant here:
		if (v.extant) {
			out << " [label=\"" << wit_id << "\"]";
		}
		else {
			out << " [label=\"" << wit_id << "\", color=gray, shape=circle, style=dashed]";
		}
		out << ";\n";
	}
	//Add all of the graph edges:
	for (textual_flow_edge e : graph.edges) {
		//Get the indices corresponding to the endpoints' IDs:
		int ancestor_index = id_to_index[e.ancestor];
		int descendant_index = id_to_index[e.descendant];
		out << "\t";
		out << ancestor_index << " -> " << descendant_index;
		//Conditionally format the edge:
		out << " [";
		if (e.connectivity > 0) {
			//If the connectivity index is not direct (i.e., 0), then print it in one-based format:
			out << "label=\"" << (e.connectivity + 1) << "\", ";
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
 * Given a reading index and an output stream, writes this variation_unit's textual flow diagram graph for that reading to output in .dot format.
 */
void variation_unit::textual_flow_diagram_for_reading_to_dot(int i, ostream & out) {
	//Get the ID of the reading:
	string reading = readings[i];
	//Add the graph first:
	out << "digraph textual_flow_diagram {\n";
	//Add lines specifying the font and font size:
	out << "\tgraph [fontname = \"helvetica\", fontsize=15];\n";
	out << "\tnode [fontname = \"helvetica\", fontsize=15];\n";
	out << "\tedge [fontname = \"helvetica\", fontsize=15];\n";
	//Add a line indicating that nodes do not have any shape:
	out << "\tnode [shape=plaintext];\n";
	//Add a box node indicating the label of this variation_unit and the selected reading:
	out << "\tlabel [shape=box, label=\"" << label << reading << "\\nCon=" << connectivity << "\"];\n";
	//Add all of the graph nodes for witnesses with the specified reading, keeping track of their numerical indices:
	unordered_map<string, int> primary_id_to_index = unordered_map<string, int>();
	for (textual_flow_vertex v : graph.vertices) {
		//If this witness does not have the specified reading, then skip it:
		string wit_id = v.id;
		if (reading_support.find(wit_id) == reading_support.end()) {
			continue;
		}
		bool wit_has_reading = false;
		list<int> readings_for_wit = reading_support[wit_id];
		for (int reading : readings_for_wit) {
			if (reading == i) {
				wit_has_reading = true;
				break;
			}
		}
		if (!wit_has_reading) {
			continue;
		}
		//Otherwise, map its ID to its numerical index:
		primary_id_to_index[wit_id] = primary_id_to_index.size();
		int wit_index = primary_id_to_index[wit_id];
		out << "\t" << wit_index;
		out << " [label=\"" << wit_id << "\"]";
		out << ";\n";
	}
	//Add all of the graph nodes for ancestors of these witnesses with a different reading, keeping track of their numerical indices:
	unordered_map<string, int> secondary_id_to_index = unordered_map<string, int>();
	for (textual_flow_edge e : graph.edges) {
		//Get the endpoints' IDs:
		string ancestor_id = e.ancestor;
		string descendant_id = e.descendant;
		//If the descendant is not in the primary vertex set, or the ancestor is, then ignore:
		if (primary_id_to_index.find(descendant_id) == primary_id_to_index.end() || primary_id_to_index.find(ancestor_id) != primary_id_to_index.end()) {
			continue;
		}
		//Otherwise, we have an ancestor with a reading other than the specified one; skip it if we've added its vertex already:
		if (secondary_id_to_index.find(ancestor_id) != secondary_id_to_index.end()) {
			continue;
		}
		//If it's new, then get its (first) reading:
		list<int> ancestor_readings = reading_support[ancestor_id];
		int ancestor_reading = ancestor_readings.front();
		string ancestor_reading_id = readings[ancestor_reading];
		//Then add a vertex for it to the secondary vertex set:
		secondary_id_to_index[ancestor_id] = primary_id_to_index.size() + secondary_id_to_index.size();
		int wit_index = secondary_id_to_index[ancestor_id];
		out << "\t" << wit_index;
		out << " [shape=circle, label=\"" << ancestor_reading_id << ": " << ancestor_id << "\"]";
		out << ";\n";
	}
	//Add all of the graph edges:
	for (textual_flow_edge e : graph.edges) {
		//Get the endpoints' IDs:
		string ancestor_id = e.ancestor;
		string descendant_id = e.descendant;
		//If the descendant is not in the primary vertex set, then skip this edge:
		if (primary_id_to_index.find(descendant_id) == primary_id_to_index.end()) {
			continue;
		}
		//Otherwise, get the indices of the endpoints:
		int ancestor_index = primary_id_to_index.find(ancestor_id) != primary_id_to_index.end() ? primary_id_to_index[ancestor_id] : secondary_id_to_index[ancestor_id];
		int descendant_index = primary_id_to_index[descendant_id];
		out << "\t";
		out << ancestor_index << " -> " << descendant_index;
		//Conditionally format the edge:
		out << " [";
		if (e.connectivity > 0) {
			//If the connectivity index is not direct (i.e., 0), then print it in one-based format:
			out << "label=\"" << (e.connectivity + 1) << "\", ";
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
 * Given an output stream, writes this variation_unit's textual flow diagram graph for only "CHANGE" edges to output in .dot format.
 */
void variation_unit::textual_flow_diagram_for_changes_to_dot(ostream & out) {
	//Add the graph first:
	out << "digraph textual_flow_diagram {\n";
	//Add lines specifying the font and font size:
	out << "\tgraph [fontname = \"helvetica\", fontsize=15];\n";
	out << "\tnode [fontname = \"helvetica\", fontsize=15];\n";
	out << "\tedge [fontname = \"helvetica\", fontsize=15];\n";
	//Add a line indicating that nodes do not have any shape:
	out << "\tnode [shape=plaintext];\n";
	//Add a box node indicating the label of this variation_unit:
	out << "\tlabel [shape=box, label=\"" << label << "\\nCon=" << connectivity << "\"];\n";
	//Maintain a vector of support lists for each reading:
	vector<list<string>> clusters = vector<list<string>>(readings.size());
	for (pair<string, list<int>> kv : reading_support) {
		string wit_id = kv.first;
		list<int> wit_readings = kv.second;
		for (int wit_reading : wit_readings) {
			clusters[wit_reading].push_back(wit_id);
		}
	}
	//Now map the IDs of the included vertices to numerical indices:
	unordered_map<string, int> id_to_index = unordered_map<string, int>();
	for (textual_flow_edge e : graph.edges) {
		//Only include vertices that are at endpoints of a "CHANGE" edge:
		if (e.type != flow_type::CHANGE) {
			continue;
		}
		string ancestor_id = e.ancestor;
		string descendant_id = e.descendant;
		if (id_to_index.find(ancestor_id) == id_to_index.end()) {
			id_to_index[ancestor_id] = id_to_index.size();
		}
		if (id_to_index.find(descendant_id) == id_to_index.end()) {
			id_to_index[descendant_id] = id_to_index.size();
		}
	}
	//Add a cluster for each reading, including all of the nodes it contains:
	for (unsigned int i = 0; i < clusters.size(); i++) {
		string reading_id = readings[i];
		list<string> cluster = clusters[i];
		out << "\tsubgraph cluster" << i << " {\n";
		out << "\t\tlabeljust=\"c\";\n";
		out << "\t\tlabel=\"" << reading_id << "\";\n";
		for (string wit_id : cluster) {
			//If this witness's ID is not found in the numerical index map, then skip it:
			if (id_to_index.find(wit_id) == id_to_index.end()) {
				continue;
			}
			//Otherwise, get the numerical index for this witness's vertex:
			int wit_index = id_to_index[wit_id];
			out << "\t\t" << wit_index;
			out << " [label=\"" << wit_id << "\"]";
			out << ";\n";
		}
	}
	//Finally, add the "CHANGE" edges:
	for (textual_flow_edge e : graph.edges) {
		//Only include vertices that are at endpoints of a "CHANGE" edge:
		if (e.type != flow_type::CHANGE) {
			continue;
		}
		//Get the indices corresponding to the endpoints' IDs:
		int ancestor_index = id_to_index[e.ancestor];
		int descendant_index = id_to_index[e.descendant];
		out << "\t";
		out << ancestor_index << " -> " << descendant_index;
		//Conditionally format the edge:
		out << " [";
		if (e.connectivity > 0) {
			//If the connectivity index is not direct (i.e., 0), then print it in one-based format:
			out << "label=\"" << (e.connectivity + 1) << "\", ";
		}
		//Highlight changes in readings in blue:
		out << "color=blue";
		out << "];\n";
	}
	return;
}
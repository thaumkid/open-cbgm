/*
 * print_graphs.cpp
 *
 *  Created on: Dec 13, 2019
 *      Author: jjmccollum
 */

#include <iostream>
#include <fstream>
#include <experimental/filesystem>
#include <string>
#include <list>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <getopt.h>

#include "pugixml.h"
#include "roaring.hh"
#include "global_stemma.h"
#include "witness.h"
#include "apparatus.h"
#include "variation_unit.h"
#include "local_stemma.h"

using namespace std;
namespace fs = std::experimental::filesystem;

/**
 * Prints the short usage message.
 */
void usage() {
	printf("usage: print_graphs [-h] [-t threshold] [--split] [--orth] [--def] [--local] [--flow] [--attestations] [--variants] [--global] input_xml\n\n");
	return;
}

/**
 * Prints the help message.
 */
void help() {
	usage();
	printf("Prints diagrams of CBGM graphs to .dot output files. The output files will be organized in separate directories based on diagram type.\n\n");
	printf("optional arguments:\n");
	printf("\t-h, --help: print usage manual\n");
	printf("\t-t, --threshold: minimum extant readings threshold\n");
	printf("\t--split: treat split attestations as distinct readings\n");
	printf("\t--orth: treat orthographic subvariants as distinct readings\n");
	printf("\t--def: treat defective forms as distinct readings\n");
	printf("\t--local: print local stemmata diagrams\n");
	printf("\t--flow: print complete textual flow diagrams for all passages\n");
	printf("\t--attestations: print coherence in attestation textual flow diagrams for all readings at all passages\n");
	printf("\t--variants: print coherence at variant passages diagrams (i.e., textual flow diagrams restricted to flow between different readings) at all passages\n");
	printf("\t--global: print global stemma diagram (this may take several minutes)\n\n");
	printf("positional arguments:\n");
	printf("\tinput_xml: collation file in TEI XML format\n");
	return;
}

/**
 * Entry point to the script.
 */
int main(int argc, char* argv[]) {
	//Parse the command-line options:
	int split = 0;
	int orth = 0;
	int def = 0;
	unsigned int threshold = 0;
	int local = 0;
	int flow = 0;
	int attestations = 0;
	int variants = 0;
	int global = 0;
	const char* const short_opts = "ht:";
	const option long_opts[] = {
		{"split", no_argument, & split, 1},
		{"orth", no_argument, & orth, 1},
		{"def", no_argument, & def, 1},
		{"threshold", required_argument, nullptr, 't'},
		{"local", no_argument, & local, 1},
		{"flow", no_argument, & flow, 1},
		{"attestations", no_argument, & attestations, 1},
		{"variants", no_argument, & variants, 1},
		{"global", no_argument, & global, 1},
		{"help", no_argument, nullptr, 'h'},
		{nullptr, no_argument, nullptr, 0}
	};
	int opt;
	while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (opt) {
			case 'h':
				help();
				return 0;
			case 't':
				threshold = atoi(optarg);
				break;
			case 0:
				//This will happen if a long option is being parsed; just move on:
				break;
			default:
				printf("Error: invalid argument.\n");
				usage();
				exit(1);
		}
	}
	//Parse the positional arguments:
	int index = optind;
	if (argc <= index) {
		printf("Error: 1 positional argument (input_xml) is required.\n");
		exit(1);
	}
	//The first positional argument is the XML file:
	char * input_xml = argv[index];
	index++;
	//Using the input flags, populate a set of reading types to be treated as distinct:
	unordered_set<string> distinct_reading_types = unordered_set<string>();
	if (split) {
		//Treat split readings as distinct:
		distinct_reading_types.insert("split");
	}
	if (orth) {
		//Treat orthographic variants as distinct:
		distinct_reading_types.insert("orthographic");
	}
	if (def) {
		//Treat defective variants as distinct:
		distinct_reading_types.insert("defective");
	}
	//Attempt to parse the input XML file as an apparatus:
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(input_xml);
	if (!result) {
		printf("Error: An error occurred while loading XML file %s: %s\n", input_xml, result.description());
		exit(1);
	}
	pugi::xml_node tei_node = doc.child("TEI");
	if (!tei_node) {
		printf("Error: The XML file %s does not have a <TEI> element as its root element.\n", input_xml);
		exit(1);
	}
	apparatus app = apparatus(tei_node, distinct_reading_types);
	//Get the current working directory's path:
	fs::path cwd = fs::current_path();
	//If specified, print all local stemmata:
	if (local) {
		cout << "Printing all local stemmata..." << endl;
		//Create the directory to write files to:
		fs::path local_dir = cwd.append("local");
		fs::create_directory(local_dir);
		for (variation_unit vu : app.get_variation_units()) {
			//The filename will be the label, with hyphens in place of spaces and the suffix .dot:
			string filename = vu.get_label();
			while (filename.find(" ") != string::npos) {
				filename.replace(filename.find(" "), 1, "-");
			}
			filename += "-local-stemma.dot";
			//Append the path:
			filename = local_dir.append(filename);
			//Open a filestream:
			fstream dot_file;
			dot_file.open(filename, ios::out);
			local_stemma ls = vu.get_local_stemma();
			ls.to_dot(dot_file);
			dot_file.close();
		}
	}
	//If no other flags are set, then we're done:
	if (flow + attestations + variants + global == 0) {
		exit(0);
	}
	//Otherwise, initialize all witnesses:
	cout << "Comparing all witnesses (this may take a while)... " << endl;
	unordered_map<string, witness> witnesses_by_id = unordered_map<string, witness>();
	for (string wit_id : app.get_list_wit()) {
		cout << "Initializing witness " << wit_id << "... " << endl;
		witness wit = witness(wit_id, app);
		//Do not add the witnesses that are too fragmentary:
		if (wit.get_explained_readings_for_witness(wit_id).cardinality() < threshold) {
			continue;
		}
		witnesses_by_id[wit_id] = wit;
	}
	//Then populate each witness's list of potential ancestors:
	for (auto & kv : witnesses_by_id) {
		witness wit = kv.second;
		wit.set_potential_ancestor_ids(witnesses_by_id);
	}
	//If any type of textual flow diagrams are requested, then construct the textual flow diagrams for all variation units:
	if (flow + attestations + variants > 0) {
		cout << "Calculating textual flow for all variation units... " << endl;
		for (variation_unit & vu : app.get_variation_units()) {
			vu.calculate_textual_flow(witnesses_by_id);
		}
	}
	//If specified, print all complete textual flow diagrams:
	if (flow) {
		cout << "Printing all complete textual flow diagrams..." << endl;
		//Create the directory to write files to:
		fs::path flow_dir = cwd.append("flow");
		fs::create_directory(flow_dir);
		for (variation_unit vu : app.get_variation_units()) {
			//The filename will be the label, with hyphens in place of spaces and the suffix .dot:
			string filename = vu.get_label();
			while (filename.find(" ") != string::npos) {
				filename.replace(filename.find(" "), 1, "-");
			}
			filename += "-textual-flow.dot";
			//Append the path:
			filename = flow_dir.append(filename);
			//Open a filestream:
			fstream dot_file;
			dot_file.open(filename, ios::out);
			vu.textual_flow_diagram_to_dot(dot_file);
			dot_file.close();
		}
	}
	//If specified, print all coherence in attestations textual flow diagrams:
	if (attestations) {
		cout << "Printing all coherence in attestations textual flow diagrams..." << endl;
		//Create the directory to write files to:
		fs::path attestations_dir = cwd.append("attestations");
		fs::create_directory(attestations_dir);
		for (variation_unit vu : app.get_variation_units()) {
			for (string rdg : vu.get_readings()) {
				//The filename will be the label, with hyphens in place of spaces and the suffix .dot:
				string filename = vu.get_label();
				while (filename.find(" ") != string::npos) {
					filename.replace(filename.find(" "), 1, "-");
				}
				filename += rdg;
				filename += "-coherence-attestations.dot";
				//Append the path:
				filename = attestations_dir.append(filename);
				//Open a filestream:
				fstream dot_file;
				dot_file.open(filename, ios::out);
				vu.textual_flow_diagram_for_reading_to_dot(rdg, dot_file);
				dot_file.close();
			}
		}
	}
	//If specified, print all coherence in variant passages flow diagrams:
	if (variants) {
		cout << "Printing all coherence in variant passages textual flow diagrams..." << endl;
		//Create the directory to write files to:
		fs::path variants_dir = cwd.append("variants");
		fs::create_directory(variants_dir);
		for (variation_unit vu : app.get_variation_units()) {
			//The filename will be the label, with hyphens in place of spaces and the suffix .dot:
			string filename = vu.get_label();
			while (filename.find(" ") != string::npos) {
				filename.replace(filename.find(" "), 1, "-");
			}
			filename += "-coherence-variants.dot";
			//Append the path:
			filename = variants_dir.append(filename);
			//Open a filestream:
			fstream dot_file;
			dot_file.open(filename, ios::out);
			vu.textual_flow_diagram_to_dot(dot_file);
			dot_file.close();
		}
	}
	//If no other flags are set, then we're done:
	if (global == 0) {
		exit(0);
	}
	//Otherwise, optimize the substemmata for all witnesses:
	cout << "Optimizing substemmata for all witnesses (this may take a while)... " << endl;
	for (auto & kv: witnesses_by_id) {
		string wit_id = kv.first;
		cout << "Optimizing substemmata for witness " << wit_id << "... " << endl;
		witness wit = kv.second;
		wit.set_global_stemma_ancestors();
	}
	//Then initialize the global stemma:
	global_stemma gs = global_stemma(witnesses_by_id);
	//If specified, print the global stemma:
	if (global) {
		cout << "Printing global stemma..." << endl;
		//Create the directory to write files to:
		fs::path global_dir = cwd.append("global");
		fs::create_directory(global_dir);
		string filename = "global-stemma.dot";
		//Append the path:
		filename = global_dir.append(filename);
		//Open a filestream:
		fstream dot_file;
		dot_file.open(filename, ios::out);
		gs.to_dot(dot_file);
		dot_file.close();
	}
	exit(0);
}
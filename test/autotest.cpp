/*
 * autotest.cpp
 *
 *  Created on: Dec 23, 2019
 *      Author: jjmccollum
 */

#include <getopt.h>
#include <iostream>
#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>

#include "autotest.h"
#include "roaring.hh"
#include "pugixml.h"
#include "global_stemma.h"
#include "textual_flow.h"
#include "witness.h"
#include "set_cover_solver.h"
#include "apparatus.h"
#include "variation_unit.h"
#include "local_stemma.h"

using namespace std;

/**
 * Prints the short usage message.
 */
void usage() {
	printf("usage: test_library [-h] [--list-modules] [--list-tests] [-m module] [-t test]\n");
	return;
}

/**
 * Prints the help message.
 */
void help() {
	usage();
	printf("Runs unit tests for the library. If specified, runs specific tests or tests for specific modules.\n\n");
	printf("optional arguments:\n");
	printf("\t-h, --help: print usage manual\n");
	printf("\t--list-modules: lists all modules to be tested\n");
	printf("\t--list-tests: lists all unit tests\n");
	printf("\t-m, --module: name of specific module to test\n");
	printf("\t-t, --test: name of specific test to run\n");
	return;
}

/**
 * Default constructor.
 */
autotest::autotest() {

}

/**
 * Constructs an autotest instance from a list of module names and a map of unit tests, keyed by parent module name.
 */
autotest::autotest(const list<string> & _modules, const map<string, list<string>> & _tests_by_module) {
	//Copy the list of modules and the map of tests for each module:
	modules = list<string>(_modules);
	tests_by_module = map<string, list<string>>(_tests_by_module);
	//Populate a map from each test to its parent module:
	parent_module_by_test = map<string, string>();
	for (pair<string, list<string>> kv : tests_by_module) {
		string module = kv.first;
		list<string> tests = kv.second;
		for (string test : tests) {
			parent_module_by_test[test] = module;
		}
	}
}

/**
 * Default destructor.
 */
autotest::~autotest() {

}

/**
 * Prints a list of all modules in this autotest instance.
 */
void autotest::print_modules() const {
	cout << "Test modules:\n";
	for (string module : modules) {
		cout << module << "\n";
	}
	cout << endl;
	return;
}

/**
 * Prints a list of all unit tests in this autotest instance.
 */
void autotest::print_tests() const {
	cout << "Unit tests:\n";
	for (string module : modules) {
		cout << module << "\n";
		list<string> tests_in_module = tests_by_module.at(module);
		for (string test : tests_in_module) {
			cout << test << "\n";
		}
	}
	cout << endl;
	return;
}

/**
 * Set this autotest instance's target module to the module with the given name.
 * A boolean value is returned indicating whether the autotest instance has a module with the specified name.
 */
bool autotest::set_target_module(const string & _target_module) {
	//Check if there is a module with the given name:
	if (tests_by_module.find(_target_module) != tests_by_module.end()) {
		//If there is, then set the target module:
		target_module = _target_module;
		return true;
	}
	else {
		return false;
	}
}

/**
 * Set this autotest instance's target test to the test with the given name.
 * A boolean value is returned indicating whether the autotest instance has a unit test with the specified name.
 */
bool autotest::set_target_test(const string & _target_test) {
	//Check if there is a unit test with the given name:
	if (parent_module_by_test.find(_target_test) != parent_module_by_test.end()) {
		//If there is, then set the target test and its parent module:
		string _target_module = parent_module_by_test.at(_target_test);
		target_module = _target_module;
		target_test = _target_test;
		return true;
	}
	else {
		return false;
	}
}

/**
 * Runs all (specified) tests for the library, populating the results in the appropriate unit_test, module_test, and library_test data structures.
 */
void autotest::run() {
	//Initialize the container for library-wide test results:
	lib_test.name = "open-cbgm";
	lib_test.modules = list<module_test>();
	//Then proceed for each module:
	string current_module;
	/**
	 * Module common
	 */
	current_module = "common";
	if (target_module.empty() || target_module == current_module) {
		//Initialize a container for module-wide test results:
		module_test mod_test;
		mod_test.name = current_module;
		mod_test.units = list<unit_test>();
		//Then proceed for each unit test:
		string current_unit;
		/**
		 * Unit common_read_xml
		 */
		current_unit = "common_read_xml";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Parse the test XML file:
				pugi::xml_document doc;
				pugi::xml_parse_result result = doc.load_file("examples/test.xml");
				if (!result) {
					u_test.msg += string(result.description()) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		lib_test.modules.push_back(mod_test);
	}
	/**
	 * Module local_stemma
	 */
	current_module = "local_stemma";
	if (target_module.empty() || target_module == current_module) {
		//Initialize a container for module-wide test results:
		module_test mod_test;
		mod_test.name = current_module;
		mod_test.units = list<unit_test>();
		//Do pre-test work:
		pugi::xml_document doc;
		doc.load_file("examples/test.xml");
		pugi::xml_node app_node = doc.select_node("descendant::app[@n=\"B00K0V0U6\"]").node();
		string label_text = app_node.child("label").text().get();
		pugi::xml_node graph_node = app_node.child("graph");
		//Then proceed for each unit test:
		string current_unit;
		/**
		 * Unit test local_stemma_constructor
		 */
		current_unit = "local_stemma_constructor";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct a local stemma with no collapsed nodes or edges:
				local_stemma ls = local_stemma(label_text, graph_node, map<string, string>());
				//Check that the label is the expected value:
				string expected_label = "Test 0:0/6";
				string label = ls.get_label();
				if (label != "Test 0:0/6") {
					u_test.msg += "Expected label " + expected_label + ", got label " + label + "\n";
				}
				//Check that the graph is the size we expect:
				unsigned int expected_n_vertices = 5;
				unsigned int expected_n_edges = 4;
				local_stemma_graph graph = ls.get_graph();
				unsigned int n_vertices = graph.vertices.size();
				unsigned int n_edges = graph.edges.size();
				if (n_vertices != expected_n_vertices) {
					u_test.msg += "Expected graph.vertices.size() == " + to_string(expected_n_vertices) + ", got " + to_string(n_vertices) + "\n";
				}
				if (n_edges != expected_n_edges) {
					u_test.msg += "Expected graph.edges.size() == " + to_string(expected_n_edges) + ", got " + to_string(n_edges) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		/**
		 * Unit test local_stemma_constructor_collapse
		 */
		current_unit = "local_stemma_constructor_collapse";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct a local stemma with collapsed nodes and edges:
				local_stemma ls = local_stemma(label_text, graph_node, map<string, string>({{"bf", "b"}, {"co", "c"}}));
				//Check that the label is the expected value:
				string expected_label = "Test 0:0/6";
				string label = ls.get_label();
				if (label != expected_label) {
					u_test.msg += "Expected label " + expected_label + ", got " + label + "\n";
				}
				//Check that the graph is the size we expect:
				unsigned int expected_n_vertices = 3;
				unsigned int expected_n_edges = 2;
				local_stemma_graph graph = ls.get_graph();
				unsigned int n_vertices = graph.vertices.size();
				unsigned int n_edges = graph.edges.size();
				if (n_vertices != expected_n_vertices) {
					u_test.msg += "Expected graph.vertices.size() == " + to_string(expected_n_vertices) + ", got " + to_string(n_vertices) + "\n";
				}
				if (n_edges != expected_n_edges) {
					u_test.msg += "Expected graph.edges.size() == " + to_string(expected_n_edges) + ", got " + to_string(n_edges) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		//Do more pre-test work:
		app_node = doc.select_node("descendant::app[@n=\"B00K0V0U4\"]").node();
		label_text = app_node.child("label").text().get();
		graph_node = app_node.child("graph");
		local_stemma ls = local_stemma(label_text, graph_node, map<string, string>());
		/**
		 * Unit test local_stemma_is_equal_or_prior
		 */
		current_unit = "local_stemma_is_equal_or_prior";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Test equality of readings:
				if (!ls.is_equal_or_prior("a", "a")) {
					u_test.msg += "For variation unit B00K0V0U4, is_equal_or_prior(\"a\", \"a\") == true is expected, but got false.";
				}
				//Test direct priority of readings:
				if (!ls.is_equal_or_prior("a", "b")) {
					u_test.msg += "For variation unit B00K0V0U4, is_equal_or_prior(\"a\", \"b\") == true is expected, but got false.";
				}
				//Test transitive priority of readings:
				if (!ls.is_equal_or_prior("a", "d")) {
					u_test.msg += "For variation unit B00K0V0U4, is_equal_or_prior(\"a\", \"d\") == true is expected, but got false.";
				}
				//Test posteriority of readings:
				if (ls.is_equal_or_prior("c", "a")) {
					u_test.msg += "For variation unit B00K0V0U4, is_equal_or_prior(\"c\", \"a\") == false is expected, but got true.";
				}
				//Test direct priority of readings:
				if (ls.is_equal_or_prior("b", "c")) {
					u_test.msg += "For variation unit B00K0V0U4, is_equal_or_prior(\"b\", \"c\") == false is expected, but got true.";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		/**
		 * Unit test local_stemma_to_dot
		 */
		current_unit = "local_stemma_to_dot";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Test .dot serialization:
				stringstream ss;
				ls.to_dot(ss);
				string out = ss.str();
				if (out.empty()) {
					u_test.msg += "The .dot serialization was empty.\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		lib_test.modules.push_back(mod_test);
	}
	/**
	 * Module variation_unit
	 */
	current_module = "variation_unit";
	if (target_module.empty() || target_module == current_module) {
		//Initialize a container for module-wide test results:
		module_test mod_test;
		mod_test.name = current_module;
		mod_test.units = list<unit_test>();
		//Do pre-test work:
		pugi::xml_document doc;
		doc.load_file("examples/test.xml");
		pugi::xml_node app_node = doc.select_node("descendant::app[@n=\"B00K0V0U8\"]").node();
		//Then proceed for each unit test:
		string current_unit;
		/**
		 * Unit test variation_unit_constructor
		 */
		current_unit = "variation_unit_constructor";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct a variation unit where only substantive readings are treated as distinct:
				set<string> distinct_reading_types = set<string>();
				variation_unit vu = variation_unit(app_node, distinct_reading_types);
				//Check that the ID is the expected value:
				string expected_id = "B00K0V0U8";
				string id = vu.get_id();
				if (id != expected_id) {
					u_test.msg += "For variation unit B00K0V0U8, expected ID " + expected_id + ", got " + id + "\n";
				}
				//Check that the label is the expected value:
				string expected_label = "Test 0:0/8";
				string label = vu.get_label();
				if (label != expected_label) {
					u_test.msg += "For variation unit B00K0V0U8, expected label " + expected_label + ", got " + label + "\n";
				}
				//Check that the readings list is the correct size:
				list<string> readings = vu.get_readings();
				unsigned int expected_readings_size = 3;
				unsigned int readings_size = readings.size();
				if (readings_size != expected_readings_size) {
					u_test.msg += "For variation unit B00K0V0U8 with only substantive readings treated as distinct, expected readings.size() == " + to_string(expected_readings_size) + ", got " + to_string(readings_size) + "\n";
				}
				//Check that the reading support map is the correct size:
				unordered_map<string, list<string>> reading_support = vu.get_reading_support();
				unsigned int expected_reading_support_size = 4;
				unsigned int reading_support_size = reading_support.size();
				if (reading_support_size != expected_reading_support_size) {
					u_test.msg += "For variation unit B00K0V0U8, expected reading_support.size() == " + to_string(expected_reading_support_size) + ", got " + to_string(reading_support_size) + "\n";
				}
				//The reading support map's entry for witness A should have two entries:
				list<string> a_rdgs = reading_support.at("A");
				unsigned int expected_a_rdgs_size = 2;
				unsigned int a_rdgs_size = a_rdgs.size();
				if (a_rdgs_size != expected_a_rdgs_size) {
					u_test.msg += "For variation unit B00K0V0U8, expected reading_support[\"A\"].size() == " + to_string(expected_a_rdgs_size) + ", got " + to_string(a_rdgs_size) + "\n";
				}
				//The reading support map's entry for witness B should have one entry:
				list<string> b_rdgs = reading_support.at("B");
				unsigned int expected_b_rdgs_size = 1;
				unsigned int b_rdgs_size = b_rdgs.size();
				if (b_rdgs_size != expected_b_rdgs_size) {
					u_test.msg += "For variation unit B00K0V0U8, expected reading_support[\"B\"].size() == " + to_string(expected_b_rdgs_size) + ", got " + to_string(b_rdgs_size) + "\n";
				}
				//The reading support map's entry for witness C should have an entry for reading c:
				list<string> c_rdgs = reading_support.at("C");
				string expected_c_rdg = "c";
				string c_rdg = c_rdgs.front();
				if (c_rdg != expected_c_rdg) {
					u_test.msg += "For variation unit B00K0V0U8 with only substantive readings treated as distinct, expected reading_support[\"C\"].front() == " + expected_c_rdg + ", got " + c_rdg + "\n";
				}
				//Check that the connectivity is correct:
				int expected_connectivity = 5;
				int connectivity = vu.get_connectivity();
				if (connectivity != expected_connectivity) {
					u_test.msg += "For variation unit B00K0V0U8, expected connectivity == " + to_string(expected_connectivity) + ", got " + to_string(connectivity) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		/**
		 * Unit test variation_unit_constructor_split_distinct
		 */
		current_unit = "variation_unit_constructor_split_distinct";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct a variation unit where substantive and split readings are treated as distinct:
				set<string> distinct_reading_types = set<string>({"split"});
				variation_unit vu = variation_unit(app_node, distinct_reading_types);
				//Check that the readings list is the correct size:
				list<string> readings = vu.get_readings();
				unsigned int expected_readings_size = 4;
				unsigned int readings_size = readings.size();
				if (readings_size != expected_readings_size) {
					u_test.msg += "For variation unit B00K0V0U8 with substantive and split readings treated as distinct, expected readings.size() == " + to_string(expected_readings_size) + ", got " + to_string(readings_size) + "\n";
				}
				//The reading support map's entry for witness C should have an entry for reading c2:
				unordered_map<string, list<string>> reading_support = vu.get_reading_support();
				list<string> c_rdgs = reading_support.at("C");
				string expected_c_rdg = "c2";
				string c_rdg = c_rdgs.front();
				if (c_rdg != expected_c_rdg) {
					u_test.msg += "For variation unit B00K0V0U8 with substantive and split readings treated as distinct, expected reading_support[\"C\"].front() == " + expected_c_rdg + ", got " + c_rdg + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		//Do more pre-test work:
		pugi::xml_node tei_node = doc.child("TEI");
		set<string> distinct_reading_types = set<string>({"split"});
		apparatus app = apparatus(tei_node, distinct_reading_types);
		variation_unit vu = app.get_variation_units()[3];
		list<witness> witnesses = list<witness>();
		for (const string & wit_id : app.get_list_wit()) {
			witness wit = witness(wit_id, app);
			witnesses.push_back(wit);
		}
		for (witness & wit : witnesses) {
			wit.set_potential_ancestor_ids(witnesses);
		}
		lib_test.modules.push_back(mod_test);
	}
	/**
	 * Module apparatus
	 */
	current_module = "apparatus";
	if (target_module.empty() || target_module == current_module) {
		//Initialize a container for module-wide test results:
		module_test mod_test;
		mod_test.name = current_module;
		mod_test.units = list<unit_test>();
		//Do pre-test work:
		pugi::xml_document doc;
		doc.load_file("examples/test.xml");
		pugi::xml_node tei_node = doc.child("TEI");
		set<string> distinct_reading_types = set<string>({"split"});
		//Then proceed for each unit test:
		string current_unit;
		/**
		 * Unit apparatus_constructor
		 */
		current_unit = "apparatus_constructor";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct an apparatus:
				apparatus app = apparatus(tei_node, distinct_reading_types);
				//Check if its number of witnesses is correct:
				unsigned int expected_n_witnesses = 5;
				unsigned int n_witnesses = app.get_list_wit().size();
				if (n_witnesses != expected_n_witnesses) {
					u_test.msg += "Expected list_wit.size() == " + to_string(expected_n_witnesses) + ", got " + to_string(n_witnesses) + "\n";
				}
				//Check if its number of variation units is correct:
				unsigned int expected_n_variation_units = 4;
				unsigned int n_variation_units = app.get_variation_units().size();
				if (n_variation_units != expected_n_variation_units) {
					u_test.msg += "Expected variation_units.size() == " + to_string(expected_n_variation_units) + ", got " + to_string(n_variation_units) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		//Do more pre-test work:
		apparatus app = apparatus(tei_node, distinct_reading_types);
		/**
		 * Unit apparatus_get_extant_passages_for_witness
		 */
		current_unit = "apparatus_get_extant_passages_for_witness";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Test if the apparatus correctly evaluates the number of passages at which a witness is extant:
				unsigned int expected_extant = 3;
				unsigned int extant = app.get_extant_passages_for_witness("E");
				if (extant != expected_extant) {
					u_test.msg += "Expected number of extant passages for witness E to be " + to_string(expected_extant) + ", got " + to_string(extant) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		lib_test.modules.push_back(mod_test);
	}
	/**
	 * Module set_cover_solver
	 */
	current_module = "set_cover_solver";
	if (target_module.empty() || target_module == current_module) {
		//Initialize a container for module-wide test results:
		module_test mod_test;
		mod_test.name = current_module;
		mod_test.units = list<unit_test>();
		//Do pre-test work:
		Roaring target = Roaring::bitmapOf(4, 0, 1, 2, 3);
		vector<set_cover_row> rows = vector<set_cover_row>();
		set_cover_row row_a;
		row_a.id = "A";
		row_a.bits = Roaring::bitmapOf(3, 0, 2, 3);
		row_a.cost = 3;
		rows.push_back(row_a);
		set_cover_row row_b;
		row_b.id = "B";
		row_b.bits = Roaring::bitmapOf(2, 0, 3);
		row_b.cost = 2;
		rows.push_back(row_b);
		set_cover_row row_c;
		row_c.id = "C";
		row_c.bits = Roaring::bitmapOf(4, 0, 1, 2, 3);
		row_c.cost = 4;
		rows.push_back(row_c);
		//Then proceed for each unit test:
		string current_unit;
		/**
		 * Unit set_cover_solver_constructor
		 */
		current_unit = "set_cover_solver_constructor";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct a new set cover solver:
				set_cover_solver scs = set_cover_solver(rows, target);
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		//Do more pre-test work:
		set_cover_solver scs = set_cover_solver(rows, target);
		/**
		 * Unit set_cover_solver_get_unique_rows
		 */
		current_unit = "set_cover_solver_get_unique_rows";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Make sure that unique coverage rows are correctly identified:
				list<set_cover_row> unique_rows = scs.get_unique_rows();
				unsigned int expected_unique_rows_size = 1;
				unsigned int unique_rows_size = unique_rows.size();
				if (unique_rows_size != expected_unique_rows_size) {
					u_test.msg += "Expected unique_rows.size() == " + to_string(expected_unique_rows_size) + ", but got " + to_string(unique_rows_size) + "\n";
				}
				else {
					set_cover_row unique_row = unique_rows.front();
					string expected_id = "C";
					string id = unique_row.id;
					if (id != expected_id) {
						u_test.msg += "Expected unique row ID to be " + expected_id + ", but got " + id + "\n";
					}
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		/**
		 * Unit set_cover_solver_get_trivial_solution
		 */
		current_unit = "set_cover_solver_get_trivial_solution";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Make sure that the trivial solution is correct:
				set_cover_solution trivial_solution = scs.get_trivial_solution();
				//Trivial solution should consist of one row:
				list<set_cover_row> solution_rows = trivial_solution.rows;
				unsigned int expected_solution_rows_size = 1;
				unsigned int solution_rows_size = solution_rows.size();
				if (solution_rows_size != expected_solution_rows_size) {
					u_test.msg += "Expected trivial_solution.rows.size() == " + to_string(expected_solution_rows_size) + ", but got " + to_string(solution_rows_size) + "\n";
				}
				else {
					//That row should be row "C":
					set_cover_row solution_row = solution_rows.front();
					string expected_id = "C";
					string id = solution_row.id;
					if (id != expected_id) {
						u_test.msg += "Expected trivial solution ID to be " + expected_id + ", but got " + id + "\n";
					}
				}
				//Make sure the trivial solution has the correct cost:
				int expected_cost = 4;
				int cost = trivial_solution.cost;
				if (cost != expected_cost) {
					u_test.msg += "Expected trivial_solution.cost == " + to_string(expected_cost) + ", but got " + to_string(cost) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		//Do more pre-test work:
		rows.pop_back(); //remove row C, which is a trivial solution
		set_cover_row row_d;
		row_d.id = "D";
		row_d.bits = Roaring::bitmapOf(3, 1, 2, 3);
		row_d.cost = 1;
		rows.push_back(row_d);
		scs = set_cover_solver(rows, target);
		/**
		 * Unit set_cover_solver_get_greedy_solution
		 */
		current_unit = "set_cover_solver_get_greedy_solution";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Make sure that the greedy solution is correct:
				set_cover_solution greedy_solution = scs.get_greedy_solution();
				//Greedy solution should consist of two rows:
				list<set_cover_row> solution_rows = greedy_solution.rows;
				unsigned int expected_solution_rows_size = 2;
				unsigned int solution_rows_size = solution_rows.size();
				if (solution_rows_size != expected_solution_rows_size) {
					u_test.msg += "Expected greedy_solution.rows.size() == " + to_string(expected_solution_rows_size) + ", but got " + to_string(solution_rows_size) + "\n";
				}
				//Make sure the greedy solution has the correct cost:
				int expected_cost = 3;
				int cost = greedy_solution.cost;
				if (cost != expected_cost) {
					u_test.msg += "Expected greedy_solution.cost == " + to_string(expected_cost) + ", but got " + to_string(cost) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		lib_test.modules.push_back(mod_test);
	}
	/**
	 * Module witness
	 */
	current_module = "witness";
	if (target_module.empty() || target_module == current_module) {
		//Initialize a container for module-wide test results:
		module_test mod_test;
		mod_test.name = current_module;
		mod_test.units = list<unit_test>();
		//Then proceed for each unit test:
		string current_unit;
		//Do pre-test work:
		pugi::xml_document doc;
		doc.load_file("examples/test.xml");
		pugi::xml_node tei_node = doc.child("TEI");
		set<string> distinct_reading_types = set<string>({"split"});
		apparatus app = apparatus(tei_node, distinct_reading_types);
		/**
		 * Unit witness_constructor
		 */
		current_unit = "witness_constructor";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct a new witness with genealogical relationships to all other witnesses:
				witness wit = witness("A", app);
				//Check that the ID is correct:
				string expected_id = "A";
				string id = wit.get_id();
				if (id != expected_id) {
					u_test.msg += "Expected witness ID to be " + expected_id + ", but got " + id + "\n";
				}
				//Check that the size of the agreements map is correct:
				unsigned int expected_agreements_by_witness_size = 5;
				unsigned int agreements_by_witness_size = wit.get_agreements_by_witness().size();
				if (expected_agreements_by_witness_size != agreements_by_witness_size) {
					u_test.msg += "Expected agreements_by_witness.size() == " + to_string(expected_agreements_by_witness_size) + ", but got " + to_string(agreements_by_witness_size) + "\n";
				}
				//Check that the size of the genealogical relationship map is correct:
				unsigned int expected_explained_readings_by_witness_size = 5;
				unsigned int explained_readings_by_witness_size = wit.get_explained_readings_by_witness().size();
				if (expected_explained_readings_by_witness_size != explained_readings_by_witness_size) {
					u_test.msg += "Expected explained_readings_by_witness.size() == " + to_string(expected_explained_readings_by_witness_size) + ", but got " + to_string(explained_readings_by_witness_size) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		/**
		 * Unit witness_constructor_relative
		 */
		current_unit = "witness_constructor_relative";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct a new witness with genealogical relationships only to itself:
				witness wit = witness("A", list<string>({"A"}), app);
				//Check that the size of the agreements map is correct:
				unsigned int expected_agreements_by_witness_size = 1;
				unsigned int agreements_by_witness_size = wit.get_agreements_by_witness().size();
				if (expected_agreements_by_witness_size != agreements_by_witness_size) {
					u_test.msg += "Expected agreements_by_witness.size() == " + to_string(expected_agreements_by_witness_size) + ", but got " + to_string(agreements_by_witness_size) + "\n";
				}
				//Check that the size of the genealogical relationship map is correct:
				unsigned int expected_explained_readings_by_witness_size = 1;
				unsigned int explained_readings_by_witness_size = wit.get_explained_readings_by_witness().size();
				if (expected_explained_readings_by_witness_size != explained_readings_by_witness_size) {
					u_test.msg += "Expected explained_readings_by_witness.size() == " + to_string(expected_explained_readings_by_witness_size) + ", but got " + to_string(explained_readings_by_witness_size) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		//Do more pre-test work:
		witness wit = witness("B", app);
		/**
		 * Unit witness_get_agreements_for_witness
		 */
		current_unit = "witness_get_agreements_for_witness";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Check that the witness's agreements with A are correct:
				Roaring expected_agreements = Roaring::bitmapOf(3, 0, 1, 3);
				Roaring agreements = wit.get_agreements_for_witness("A");
				if ((agreements ^ expected_agreements).cardinality() != 0) {
					u_test.msg += "Expected agreements_for_witness(\"A\") == " + expected_agreements.toString() + ", but got " + agreements.toString() + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		/**
		 * Unit witness_get_explained_readings_for_witness
		 */
		current_unit = "witness_get_explained_readings_for_witness";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Check that the witness's explained readings by A are correct:
				Roaring expected_explained_readings = Roaring::bitmapOf(4, 0, 1, 2, 3);
				Roaring explained_readings = wit.get_explained_readings_for_witness("A");
				if ((explained_readings ^ expected_explained_readings).cardinality() != 0) {
					u_test.msg += "Expected explained_readings_for_witness(\"A\") == " + expected_explained_readings.toString() + ", but got " + explained_readings.toString() + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		//Do more pre-test work:
		wit = witness("C", app);
		list<witness> witnesses = list<witness>();
		for (string wit_id : app.get_list_wit()) {
			witness other_wit = witness(wit_id, app);
			witnesses.push_back(other_wit);
		}
		/**
		 * Unit witness_set_potential_ancestor_ids
		 */
		current_unit = "witness_set_potential_ancestor_ids";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Check if a witness's potential ancestor list is correctly sorted and filtered:
				wit.set_potential_ancestor_ids(witnesses);
				vector<string> expected_potential_ancestor_ids = vector<string>({"B", "A"});
				vector<string> potential_ancestor_ids = vector<string>();
				for (string potential_ancestor_id : wit.get_potential_ancestor_ids()) {
					potential_ancestor_ids.push_back(potential_ancestor_id);
				}
				unsigned int expected_potential_ancestor_ids_size = expected_potential_ancestor_ids.size();
				unsigned int potential_ancestor_ids_size = potential_ancestor_ids.size();
				if (potential_ancestor_ids_size != expected_potential_ancestor_ids_size) {
					u_test.msg += "Expected potential_ancestor_ids.size() == " + to_string(expected_potential_ancestor_ids_size) + ", but got " + to_string(potential_ancestor_ids_size) + "\n";
				}
				else {
					bool lists_equal = true;
					for (unsigned int i = 0; i < expected_potential_ancestor_ids.size(); i++) {
						if (potential_ancestor_ids[i] != expected_potential_ancestor_ids[i]) {
							lists_equal = false;
							break;
						}
					}
					if (!lists_equal) {
						u_test.msg += "Expected potential_ancestor_ids == [\"B\", \"A\"], but got ";
						u_test.msg += "[";
						for (string potential_ancestor_id : potential_ancestor_ids) {
							if (potential_ancestor_id == potential_ancestor_ids.front()) {
								u_test.msg += ", ";
							}
							u_test.msg += potential_ancestor_id;
						}
						u_test.msg += "]\n";
					}
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		/**
		 * Unit witness_set_global_stemma_ancestor_ids
		 */
		current_unit = "witness_set_global_stemma_ancestor_ids";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Check if a witness's global stemma ancestor list is correct:
				wit.set_global_stemma_ancestor_ids();
				vector<string> expected_global_stemma_ancestor_ids = vector<string>({"B"});
				vector<string> global_stemma_ancestor_ids = vector<string>();
				for (string global_stemma_ancestor_id : wit.get_global_stemma_ancestor_ids()) {
					global_stemma_ancestor_ids.push_back(global_stemma_ancestor_id);
				}
				unsigned int expected_global_stemma_ancestor_ids_size = expected_global_stemma_ancestor_ids.size();
				unsigned int global_stemma_ancestor_ids_size = global_stemma_ancestor_ids.size();
				if (global_stemma_ancestor_ids_size != expected_global_stemma_ancestor_ids_size) {
					u_test.msg += "Expected global_stemma_ancestor_ids.size() == " + to_string(expected_global_stemma_ancestor_ids_size) + ", but got " + to_string(global_stemma_ancestor_ids_size) + "\n";
				}
				else {
					bool lists_equal = true;
					for (unsigned int i = 0; i < expected_global_stemma_ancestor_ids.size(); i++) {
						if (global_stemma_ancestor_ids[i] != expected_global_stemma_ancestor_ids[i]) {
							lists_equal = false;
							break;
						}
					}
					if (!lists_equal) {
						u_test.msg += "Expected global_stemma_ancestor_ids == [\"B\"], but got ";
						u_test.msg += "[";
						for (string global_stemma_ancestor_id : global_stemma_ancestor_ids) {
							if (global_stemma_ancestor_id == global_stemma_ancestor_ids.front()) {
								u_test.msg += ", ";
							}
							u_test.msg += global_stemma_ancestor_id;
						}
						u_test.msg += "]\n";
					}
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		lib_test.modules.push_back(mod_test);
	}
	/**
	 * Module textual_flow
	 */
	current_module = "textual_flow";
	if (target_module.empty() || target_module == current_module) {
		//Initialize a container for module-wide test results:
		module_test mod_test;
		mod_test.name = current_module;
		mod_test.units = list<unit_test>();
		//Then proceed for each unit test:
		string current_unit;
		//Do pre-test work:
		pugi::xml_document doc;
		doc.load_file("examples/test.xml");
		pugi::xml_node tei_node = doc.child("TEI");
		set<string> distinct_reading_types = set<string>({"split"});
		apparatus app = apparatus(tei_node, distinct_reading_types);
		variation_unit vu = app.get_variation_units()[3];
		list<witness> witnesses = list<witness>();
		for (string wit_id : app.get_list_wit()) {
			witness wit = witness(wit_id, app);
			witnesses.push_back(wit);
		}
		for (witness & wit : witnesses) {
			wit.set_potential_ancestor_ids(witnesses);
		}
		/**
		 * Unit textual_flow_constructor
		 */
		current_unit = "textual_flow_constructor";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct a textual flow instance:
				textual_flow tf = textual_flow(vu, witnesses);
				//Check that the diagram has the correct number of vertices:
				unsigned int expected_n_vertices = 5;
				unsigned int expected_n_edges = 4;
				textual_flow_graph graph = tf.get_graph();
				unsigned int n_vertices = graph.vertices.size();
				unsigned int n_edges = graph.edges.size();
				if (n_vertices != expected_n_vertices) {
					u_test.msg += "Expected graph.vertices.size() == " + to_string(expected_n_vertices) + ", got " + to_string(n_vertices) + "\n";
				}
				if (n_edges != expected_n_edges) {
					u_test.msg += "Expected graph.edges.size() == " + to_string(expected_n_edges) + ", got " + to_string(n_edges) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		//Do more pre-test work:
		textual_flow tf = textual_flow(vu, witnesses);
		/**
		 * Unit test textual_flow_textual_flow_diagram_to_dot
		 */
		current_unit = "textual_flow_textual_flow_diagram_to_dot";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Test .dot serialization of complete textual flow graph:
				stringstream ss;
				tf.textual_flow_to_dot(ss);
				string out = ss.str();
				if (out.empty()) {
					u_test.msg += "The .dot serialization was empty.\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		/**
		 * Unit test textual_flow_coherence_in_attestations_to_dot
		 */
		current_unit = "textual_flow_coherence_in_attestations_to_dot";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Test .dot serialization of coherence in attestations graph:
				stringstream ss;
				tf.coherence_in_attestations_to_dot("b", ss);
				string out = ss.str();
				if (out.empty()) {
					u_test.msg += "The .dot serialization was empty.\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		/**
		 * Unit test textual_flow_coherence_in_variant_passages_to_dot
		 */
		current_unit = "textual_flow_coherence_in_variant_passages_to_dot";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Test .dot serialization of coherence in variant passages graph:
				stringstream ss;
				tf.coherence_in_variant_passages_to_dot(ss);
				string out = ss.str();
				if (out.empty()) {
					u_test.msg += "The .dot serialization was empty.\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		lib_test.modules.push_back(mod_test);
	}
	/**
	 * Module global_stemma
	 */
	current_module = "global_stemma";
	if (target_module.empty() || target_module == current_module) {
		//Initialize a container for module-wide test results:
		module_test mod_test;
		mod_test.name = current_module;
		mod_test.units = list<unit_test>();
		//Then proceed for each unit test:
		string current_unit;
		//Do pre-test work:
		pugi::xml_document doc;
		doc.load_file("examples/test.xml");
		pugi::xml_node tei_node = doc.child("TEI");
		set<string> distinct_reading_types = set<string>({"split"});
		apparatus app = apparatus(tei_node, distinct_reading_types);
		list<witness> witnesses = list<witness>();
		for (string wit_id : app.get_list_wit()) {
			witness wit = witness(wit_id, app);
			witnesses.push_back(wit);
		}
		for (witness & wit : witnesses) {
			wit.set_potential_ancestor_ids(witnesses);
			wit.set_global_stemma_ancestor_ids();
		}
		/**
		 * Unit global_stemma_constructor
		 */
		current_unit = "global_stemma_constructor";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Construct a global stemma from a list of witnesses with optimized substemmata:
				global_stemma gs = global_stemma(witnesses);
				//Check that the graph is the size we expect:
				unsigned int expected_n_vertices = 5;
				unsigned int expected_n_edges = 4;
				global_stemma_graph graph = gs.get_graph();
				unsigned int n_vertices = graph.vertices.size();
				unsigned int n_edges = graph.edges.size();
				if (n_vertices != expected_n_vertices) {
					u_test.msg += "Expected graph.vertices.size() == " + to_string(expected_n_vertices) + ", got " + to_string(n_vertices) + "\n";
				}
				if (n_edges != expected_n_edges) {
					u_test.msg += "Expected graph.edges.size() == " + to_string(expected_n_edges) + ", got " + to_string(n_edges) + "\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		//Do more pre-test work:
		global_stemma gs = global_stemma(witnesses);
		/**
		 * Unit global_stemma_to_dot
		 */
		current_unit = "global_stemma_to_dot";
		if (target_test.empty() || target_test == current_unit) {
			//Initialize a container for module-wide test results:
			unit_test u_test;
			u_test.name = current_unit;
			u_test.passed = false;
			u_test.msg = "";
			//Run the test:
			try {
				//Test .dot serialization:
				stringstream ss;
				gs.to_dot(ss);
				string out = ss.str();
				if (out.empty()) {
					u_test.msg += "The .dot serialization was empty.\n";
				}
				if (u_test.msg.empty()) {
					u_test.passed = true;
				}
			}
			catch (const exception & e) {
				u_test.msg += string(e.what()) + "\n";
			}
			mod_test.units.push_back(u_test);
		}
		lib_test.modules.push_back(mod_test);
	}
}

/**
 * Returns this autotest instance's library_test data structure.
 */
const library_test autotest::get_results() const {
	return lib_test;
}

/**
 * Entry point to the script. Runs all tests or just the specified tests.
 */
int main(int argc, char* argv[]) {
	//Parse the command-line options:
	int list_modules = 0;
	int list_tests = 0;
	string target_module = "";
	string target_test = "";
	const char* const short_opts = "hm:t:";
	const option long_opts[] = {
		{"list-modules", no_argument, & list_modules, 1},
		{"list-tests", no_argument, & list_tests, 1},
		{"module", required_argument, nullptr, 'm'},
		{"test", required_argument, nullptr, 't'},
		{"help", no_argument, nullptr, 'h'},
		{nullptr, no_argument, nullptr, 0}
	};
	int opt;
	while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (opt) {
			case 'h':
				help();
				return 0;
			case 'm':
				target_module = optarg;
				break;
			case 't':
				target_test = optarg;
				break;
			case 0:
				//This will happen if a long option is being parsed; just move on:
				break;
			default:
				cerr << "Error: invalid argument." << endl;
				usage();
				exit(1);
		}
	}
	//Initialize the list of test modules:
	list<string> modules = list<string>({
		"common",
		"local_stemma",
		"variation_unit",
		"apparatus",
		"set_cover_solver",
		"witness",
		"textual_flow",
		"global_stemma"
	});
	//Initialize the map of unit tests, keyed by parent module name:
	map<string, list<string>> tests_by_module = map<string, list<string>>({
		{"common", {"common_read_xml"}},
		{"local_stemma", {"local_stemma_constructor", "local_stemma_constructor_collapse", "local_stemma_is_equal_or_prior", "local_stemma_to_dot"}},
		{"variation_unit", {"variation_unit_constructor", "variation_unit_constructor_split_distinct"}},
		{"apparatus", {"apparatus_constructor", "apparatus_get_extant_passages_for_witness"}},
		{"set_cover_solver", {"set_cover_solver_constructor", "set_cover_solver_get_unique_rows", "set_cover_solver_get_trivial_solution", "set_cover_solver_get_greedy_solution"}},
		{"witness", {"witness_constructor", "witness_constructor_relative", "witness_get_agreements_for_witness", "witness_get_explained_readings_for_witness", "witness_set_potential_ancestor_ids", "witness_set_global_stemma_ancestor_ids"}},
		{"textual_flow", {"textual_flow_constructor", "textual_flow_textual_flow_to_dot", "textual_flow_coherence_in_attestations_to_dot", "textual_flow_coherence_in_variant_passages_to_dot"}},
		{"global_stemma", {"global_stemma_constructor", "global_stemma_to_dot"}}
	});
	//Initialize an autotest instance with these containers:
	autotest at = autotest(modules, tests_by_module);
	//If the list-modules or list-tests flags are set,
	//then print out whichever list is more specific:
	if (list_tests) {
		at.print_tests();
	}
	else if (list_modules) {
		at.print_modules();
	}
	//If a target test was specified, then take note of it:
	if (!target_test.empty()) {
		bool success = at.set_target_test(target_test);
		if (!success) {
			cerr << "Error: the specified test " << target_test << " is not the name of a unit test." << endl;
			exit(1);
		}
	}
	//Otherwise, if a target module was specified, then take note of it:
	else if (!target_module.empty()) {
		bool success = at.set_target_module(target_module);
		if (!success) {
			cerr << "Error: the specified module " << target_module << " is not the name of a module." << endl;
			exit(1);
		}
	}
	at.run();
	//Get the results:
	library_test lib_test = at.get_results();
	//Keep tallies of all tests performed and all tests passed in the library:
	unsigned int lib_tests_performed = 0;
	unsigned int lib_tests_passed = 0;
	cout << "LIBRARY " << lib_test.name << "\n\n";
	//Proceed for each module tested:
	for (module_test mod_test : lib_test.modules) {
		//Keep tallies of all tests performed and all tests passed in the module:
		unsigned int mod_tests_performed = 0;
		unsigned int mod_tests_passed = 0;
		cout << "\tMODULE " << mod_test.name << "\n\n";
		//Proceed for each unit test:
		for (unit_test u_test : mod_test.units) {
			cout << "\t\tTEST " << u_test.name << " ";
			mod_tests_performed++;
			if (u_test.passed) {
				cout << "PASS\n";
				mod_tests_passed++;
			}
			else {
				cout << "<<FAIL>>: " << u_test.msg;
			}
		}
		//Add this module's totals to the totals for the library:
		lib_tests_performed += mod_tests_performed;
		lib_tests_passed += mod_tests_passed;
		//Report the totals for this module:
		cout << "\t" << "==================================" << "\n";
		cout << "\t" << mod_tests_passed << " of " << mod_tests_performed << " tests passed.\n\n";
	}
	//Report the totals for the library:
	cout << "==================================\n";
	cout << lib_tests_passed << " of " << lib_tests_performed << " tests passed total.\n\n";
	//Return a successful exit code only if all tests have been passed:
	return lib_tests_passed == lib_tests_performed ? 0 : 1;
}

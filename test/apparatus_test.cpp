/*
 * apparatus_test.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: jjmccollum
 */

#include <iostream>
#include <string>

#include "pugixml.h"
#include "apparatus.h"

using namespace std;

void test_apparatus() {
	cout << "Running test_apparatus..." << endl;
	//Parse the test XML file:
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file("examples/3_john_collation.xml");
	cout << "XML file load result: " << result.description() << endl;
	pugi::xml_node tei_node = doc.child("TEI");
	unordered_set<string> distinct_reading_types = unordered_set<string>({"substantive", "split"});
	apparatus app = apparatus(tei_node, distinct_reading_types);
	cout << "list_wit: " << endl;
	for (string wit : app.get_list_wit()) {
		cout << wit << " ";
	}
	cout << endl;
	cout << "Done." << endl;
	return;
}

int main() {
	/**
	 * Entry point to the script. Calls all test methods.
	 */
	test_apparatus();
}

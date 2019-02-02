/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <fstream>
#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
	if (argc < 2) {
		cerr << "Usage: {} input...\n";
		return 1;
	}

	ostream &out = cout;

	out << "// This is an automatically generated file, DO NOT EDIT!\n\n"
			"static struct {\n"
			"\tconst char *filename;\n"
			"\tconst char *data;\n"
			"} templates[] = {\n";

	for (int i = 1; i < argc; i++) {
		out << "\t{\"" << argv[i] << "\", R\"__TEMPLATE__(";
		ifstream in(argv[i]);
		out << in.rdbuf();
		out << ")__TEMPLATE__\"},\n";
	}

	out << "};\n";
}

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017 Guus Sliepen <guus@lightbts.info>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fstream>
#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
	if (argc < 2) {
		cerr << "Usage: {} input...\n";
		return 1;
	}

	ofstream out("templates.inl");

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

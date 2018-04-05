/* LightBTS -- a lightweight issue tracking system
   Copyright © 2018 Guus Sliepen <guus@lightbts.info>

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

#include <fmt/ostream.h>
#include <iostream>

#include "import.hpp"

#include "cli.hpp"
#include "lightbts.hpp"

using namespace std;
using namespace fmt;

static int import_one_file(LightBTS::Instance &bts, istream &file) {
	LightBTS::Message msg;
	msg.load(file);
	if (!bts.import(msg))
		return 1;
	return 0;
}

int do_import(const char *argv0, const vector<string> &args) {
	LightBTS::Instance bts(data_dir);

	if (args.empty()) {
		import_one_file(bts, cin);
	} else {
		for (auto &&filename: args) {
			ifstream file(filename);
			import_one_file(bts, file);
		}
	}

	return 0;
}

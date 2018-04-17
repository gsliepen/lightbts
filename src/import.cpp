/* LightBTS -- a lightweight issue tracking system
   Copyright © 2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
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

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

#include <set>
#include <iostream>
#include <fmt/ostream.h>

#include "show.hpp"

#include "cli.hpp"
#include "lightbts.hpp"

using namespace std;
using namespace fmt;

static int do_show_message(const string &id) {
	print(cerr, "Not implemented.\n");
	return 1;
}

static int do_show_bug(const string &id) {
	LightBTS::Instance bts(data_dir);

	auto ticket = bts.get_ticket(id);

	print("Bug#{}: {}\n", ticket.get_id(), ticket.get_title());
	print("Status: {}\n", ticket.get_status_name());
	auto tags = bts.get_tags(ticket);
	if (!tags.empty()) {
		print("Tags:");
		for (auto &&tag: tags)
			print(" {}", tag);
		print("\n");
	}
	print("Severity: {}\n", ticket.get_severity_name());
	auto milestone = bts.get_milestone(ticket);
	if (!milestone.empty())
		print("Milestone: {}\n", milestone);

	return 0;
}

int do_show(const char *argv0, const vector<string> &args) {
	if (args.size() != 1) {
		print(cerr, "Exactly one argument required.\n");
		return 1;
	}

	auto id = args[0];
	if (id.find('@') != id.npos)
		return do_show_message(id);
	else
		return do_show_bug(id);
}

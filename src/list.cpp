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
#include <fmt/ostream.h>

#include "list.hpp"

#include "cli.hpp"
#include "lightbts.hpp"

using namespace std;
using namespace fmt;

int do_list(const char *argv0, const vector<string> &args) {
	LightBTS::Instance bts(data_dir);

	bool do_tags = false;
	bool do_milestones = false;
	set<string> tags;
	set<string> milestones;

	if (!args.empty()) {
		auto len = args.size();
		auto &&last_arg = args[len - 1];

		if (last_arg == "tags") {
			do_tags = true;
			len--;
		} else if(last_arg == "milestones") {
			do_milestones = true;
			len--;
		} else if(last_arg == "bugs") {
			len--;
		}
	}

	for(auto &&ticket: bts.list()) {
		if (do_tags) {
			for(auto &&tag: bts.get_tags(ticket))
				tags.insert(tag);
		} else if (do_milestones) {
			milestones.insert(bts.get_milestone(ticket));
		} else {
			print("{:>6} {:6} {:9}  {}\n", ticket.get_id(), ticket.get_status_name(), ticket.get_severity_name(), ticket.get_title());
		}
	}

	if (do_tags) {
		for (auto &&tag: tags)
			print("{}\n", tag);
	} else if (do_milestones) {
		for (auto &&milestone: milestones)
			print("{}\n", milestone);
	}

	return 0;
}	

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <set>
#include <fmt/ostream.h>

#include "list.hpp"

#include "cli.hpp"
#include "lightbts.hpp"
#include "pager.hpp"

using namespace std;
using namespace fmt;

int do_list(const char *argv0, const vector<string> &args) {
	LightBTS::Instance bts(data_dir);

	bool do_tags = false;
	bool do_milestones = false;
	set<string> tags;
	set<string> milestones;
	auto len = args.size();

	if (!args.empty()) {
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

	Pager pager(bts.get_config("core", "pager"));

	for(auto &&ticket: bts.list(args, len)) {
		if (do_tags) {
			for(auto &&tag: bts.get_tags(ticket))
				tags.insert(tag);
		} else if (do_milestones) {
			milestones.insert(bts.get_milestone(ticket));
		} else {
			print(pager, "{:>6} {:6} {:9}  {}\n", ticket.get_id(), ticket.get_status_name(), ticket.get_severity_name(), ticket.get_title());
		}
	}

	if (do_tags) {
		for (auto &&tag: tags)
			print(pager, "{}\n", tag);
	} else if (do_milestones) {
		for (auto &&milestone: milestones)
			print(pager, "{}\n", milestone);
	}

	return 0;
}	

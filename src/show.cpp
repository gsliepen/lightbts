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
#include "pager.hpp"

using namespace std;
using namespace fmt;

static int do_show_message(const string &id) {
	LightBTS::Instance bts(data_dir);

	auto message = bts.get_message(id);

	Pager pager(bts.get_config("core", "pager"));

	print(pager, "{}", message.to_string());

	return 0;
}

static int do_show_bug(const string &id) {
	LightBTS::Instance bts(data_dir);

	auto ticket = bts.get_ticket(id);

	Pager pager(bts.get_config("core", "pager"));

	print(pager, "Bug#{}: {}\n", ticket.get_id(), ticket.get_title());
	print(pager, "Status: {}\n", ticket.get_status_name());
	auto tags = bts.get_tags(ticket);
	if (!tags.empty()) {
		print(pager, "Tags:");
		for (auto &&tag: tags)
			print(pager, " {}", tag);
		print(pager, "\n");
	}
	print(pager, "Severity: {}\n", ticket.get_severity_name());
	auto milestone = bts.get_milestone(ticket);
	if (!milestone.empty())
		print(pager, "Milestone: {}\n", milestone);

	print(pager, "\n");

	bool first = true;
	for (auto &&message_id: bts.get_message_ids(ticket)) {
		if (verbose) {
			if (!first)
				print(pager, "\n");
			auto message = bts.get_message(message_id);
			print(pager, "From: {}\n", message["From"]);
			print(pager, "To: {}\n", message["To"]);
			print(pager, "Subject: {}\n", message["Subject"]);
			print(pager, "Date: {}\n", message["Date"]);
			print(pager, "Message-ID: {}\n", message["Message-ID"]);
			print(pager, "\n");
			print(pager, "{}", message.get_text());
		} else {
			if (first) {
				auto message = bts.get_message(message_id);
				string text = message.get_text();
				string::size_type pos = 0;
				for(int i = 0; i < 10 && pos != text.npos; i++) {
					pos = text.find('\n', pos);
					if (pos != text.npos)
						pos++;
				}
				print(pager, "{}", text.substr(0, pos));
				if (pos != text.npos)
					print(pager, "[...]\n");
				print(pager, "\n");
			}

			print(pager, "{}\n", message_id);
		}

		first = false;
	}

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

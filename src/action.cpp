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

#include <boost/algorithm/string/join.hpp>
#include <fmt/ostream.h>
#include <iostream>
#include <string>

#include "reply.hpp"

#include "cli.hpp"
#include "edit.hpp"
#include "lightbts.hpp"

using namespace std;
using namespace fmt;

static int do_action(const string &id, const string &action) {
	LightBTS::Instance bts(data_dir);

	auto ticket = bts.get_ticket(id);
	auto first_message_id = bts.get_first_message_id(ticket);

	if (first_message_id.empty()) {
		print(cerr, "No message ID found for ticket {}!\n", ticket.get_id());
		return 1;
	}

	LightBTS::Message msg;
	msg.set_crlf(false);
	msg["From"] = bts.get_local_email_address();
	msg["Subject"] = ticket.get_title();

	string body = action;

	if (!cl_message.empty()) {
		body.push_back('\n');
		body.append(cl_message);
		body.push_back('\n');
		msg.set_body(body);
	} else if (!batch) {
		body.push_back('\n');
		msg.set_body(body);
		if (!edit_interactive(bts, msg, "action")) {
			print(cerr, "Aborting action.\n");
			return 1;
		}
	} else {
		msg.set_body(body);
	}

	msg["To"] = "LightBTS";
	msg["User-Agent"] = "LightBTS/" + lightbts_version;
	msg["In-Reply-To"] = "<" + bts.get_first_message_id(ticket) + ">";

	if (bts.import(msg)) {
		print(cerr, "Import of action created a new ticket!\n");
		return 1;
	}

	print(cerr, "Action recorded for bug number {}\n", ticket.get_id());

	return 0;
}

static void add_version(string &action, const string &prefix) {
	for (auto &&version: versions)
		action.append(format("{}: {}\n", prefix, version));
}

static void add_tags(string &action) {
	for (auto &&tag: tags)
		action.append(format("Tag: +{}\n", tag));
}

int do_close(const char *argv0, const vector<string> &args) {
	if (args.size() != 1) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	string action = "Status: closed\n";
	add_version(action, "Fixed");
	add_tags(action);

	return do_action(args[0], action);
}

int do_reopen(const char *argv0, const vector<string> &args) {
	if (args.size() != 1) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	string action = "Status: open\n";
	add_version(action, "Found");
	add_tags(action);

	return do_action(args[0], action);
}

int do_retitle(const char *argv0, const vector<string> &args) {
	if (args.size() < 2) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	string action = "Title: " + args[1];
	for (size_t i = 2; i < args.size(); i++)
		action += " " + args[i];

	return do_action(args[0], action);
}

int do_single_action(const char *argv0, const vector<string> &args, const string &prefix) {
	if (args.size() != 2) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	string action = format("{}: {}\n", prefix, args[1]);
	return do_action(args[0], action);
}

int do_multiple_action(const char *argv0, const vector<string> &args, const string &prefix) {
	if (args.size() < 2) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	string action;
	for (size_t i = 1; i < args.size(); i++)
		action.append(format("{}: {}\n", prefix, args[i]));

	return do_action(args[0], action);
}

int do_found(const char *argv0, const vector<string> &args) {
	return do_multiple_action(argv0, args, "Found");
}

int do_notfound(const char *argv0, const vector<string> &args) {
	return do_multiple_action(argv0, args, "Notfound");
}

int do_fixed(const char *argv0, const vector<string> &args) {
	return do_multiple_action(argv0, args, "Fixed");
}

int do_notfixed(const char *argv0, const vector<string> &args) {
	return do_multiple_action(argv0, args, "Notfixed");
}

int do_severity(const char *argv0, const vector<string> &args) {
	return do_single_action(argv0, args, "Severity");
}

int do_link(const char *argv0, const vector<string> &args) {
	if (args.size() != 3) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	if (!LightBTS::is_valid_link_type(args[1])) {
		print(cerr, "Invalid link type\n");
		return 1;
	}

	string action = format("{}: {}\n", args[1], args[2]);

	return do_action(args[0], action);
}

int do_unlink(const char *argv0, const vector<string> &args) {
	if (args.size() != 3) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	if (!LightBTS::is_valid_link_type(args[1])) {
		print(cerr, "Invalid link type\n");
		return 1;
	}

	string action = format("Un{}: {}\n", args[1], args[2]);

	return do_action(args[0], action);
}

int do_tags(const char *argv0, const vector<string> &args) {
	if (args.size() < 2) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	string action = "Tags:";
	for (size_t i = 1; i < args.size(); i++)
		action += " " + args[i];

	return do_action(args[0], action);
}

int do_owner(const char *argv0, const vector<string> &args) {
	if (args.size() < 2) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	string action = "Owner:";
	for (size_t i = 1; i < args.size(); i++)
		action += " " + args[i];

	return do_action(args[0], action);
}

int do_noowner(const char *argv0, const vector<string> &args) {
	if (args.size() != 1) {
		print(cerr, "Invalid number of arguments\n");
		return 1;
	}

	return do_action(args[0], "Owner: -\n");
}

int do_deadline(const char *argv0, const vector<string> &args) {
	return do_single_action(argv0, args, "Deadline");
}

int do_milestone(const char *argv0, const vector<string> &args) {
	return do_single_action(argv0, args, "Milestone");
}

int do_progress(const char *argv0, const vector<string> &args) {
	return do_single_action(argv0, args, "Progress");
}


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

static string quote_text(const string &in) {
	string out;
	string::size_type line_start = 0;

	do {
		auto line_end = in.find('\n', line_start);

		out.append("> ");
		if (line_end == string::npos)
			out.append(in.substr(line_start));
		else
			out.append(in.substr(line_start, line_end - line_start));
		out.append("\n");

		if (line_end == string::npos)
			break;

		line_start = line_end + 1;
	} while (line_start < in.size());

	return out;
}

int do_reply(const char *argv0, const vector<string> &args) {
	if (args.empty()) {
		print(cerr, "Missing bug or message id.\n");
		return 1;
	}

	LightBTS::Instance bts(data_dir);

	auto id = args[0];
	if (id.find('@') == id.npos) {
		auto ticket = bts.get_ticket(id);
		id = bts.get_first_message_id(ticket);
	}

	auto parent = bts.get_message(id);

	LightBTS::Message msg;
	msg.set_crlf(false);
	msg["From"] = bts.get_local_email_address();
	msg["Subject"] = parent["Subject"];

	string body;
	if (!severity.empty())
		body.append(format("Severity: {}\n", severity));
	for (auto &&version: versions)
		body.append(format("Version: {}\n", version));
	for (auto &&tag: tags)
		body.append(format("Tags: {}\n", tag));
	if (!body.empty())
		body.append("\n");

	if (!cl_message.empty()) {
		body.append(cl_message);
		body.push_back('\n');
		msg.set_body(body);
	} else if (!batch) {
		// TODO: add "On <date>, <parent's From> wrote:\n\n"
		body.append(quote_text(parent.get_text()));
		msg.set_body(body);
		if (!edit_interactive(bts, msg, "reply")) {
			print(cerr, "Aborting creating new issue.\n");
			return 1;
		}
	} else {
		char buffer[4096];
		while(cin.read(buffer, sizeof(buffer)))
			body.append(buffer, sizeof(buffer));
		if (cin.bad())
			throw runtime_error("Error while reading input");
		body.append(buffer, cin.gcount());
		msg.set_body(body);
	}

	msg["To"] = "LightBTS";
	msg["User-Agent"] = "LightBTS/" + lightbts_version;
	msg["In-Reply-To"] = parent["Message-ID"];

	if (bts.import(msg)) {
		print(cerr, "Import of reply created a new ticket!\n");
		return 1;
	}

	auto ticket = bts.get_ticket_from_message_id(msg["Message-ID"]);
	print(cerr, "Thank you for reporting additional information for bug number {}\n", ticket.get_id());

	return 0;
}

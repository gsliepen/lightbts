/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <boost/algorithm/string/join.hpp>
#include <fmt/ostream.h>
#include <iostream>

#include "create.hpp"

#include "cli.hpp"
#include "edit.hpp"
#include "lightbts.hpp"

using namespace std;
using namespace fmt;

int do_create(const char *argv0, const vector<string> &args) {
	if (args.empty()) {
		print(cerr, "Missing title.\n");
		return 1;
	}

	LightBTS::Instance bts(data_dir);

	LightBTS::Message msg;
	msg.set_crlf(false);
	msg["From"] = bts.get_local_email_address();
	msg["Subject"] = boost::algorithm::join(args, " ");

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
		msg.set_body(body);
		if (!edit_interactive(bts, msg, "create")) {
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

	if (!bts.import(msg)) {
		print(cerr, "Message import failed\n");
		return 1;
	}

	auto ticket = bts.get_ticket_from_message_id(msg["Message-ID"]);
	print(cerr, "Thank you for reporting a bug, which has been assigned number {}\n", ticket.get_id());

	return 0;
}

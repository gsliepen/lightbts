/* LightBTS -- a lightweight issue tracking system
   Copyright © 2018-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <print>

#include "commands.hpp"
#include "compose.hpp"

static std::string quote_text(std::string_view text) {
	std::string quoted;
	while (!text.empty()) {
		auto end = text.find('\n');
		quoted += "> ";
		quoted += text.substr(0, end);
		quoted += '\n';
		text = end == text.npos ? std::string_view{} : text.substr(end + 1);
	}
	return quoted;
}

int do_reply(const Options &options, const Arguments &args) {
	if (args.size() != 1) {
		std::println(std::cerr, "Expected one bug or message id.");
		return 1;
	}

	auto bts = options.open_instance();

	auto parent_id = LightBTS::unquote_message_id(args[0]);
	if (!parent_id.contains('@')) {
		auto ticket = bts.get_ticket(parent_id);
		parent_id = bts.get_first_message_id(ticket);
		if (parent_id.empty()) {
			std::println(std::cerr, "No message ID found for ticket {}!", ticket.id);
			return 1;
		}
	}

	auto parent = bts.get_message(parent_id);

	auto message = compose_message(bts, options, {
		.subject = parent.get_header_value("Subject"),
		.pseudo_headers = option_headers(bts, options),
		.in_reply_to = parent_id,
		.editor_text = std::format("On {}, {} wrote:\n{}", parent.get_header_value("Date"), parent.get_header_value("From"), quote_text(parent.get_text())),
		.editor_hint = "reply",
	});

	if (!message) {
		std::println(std::cerr, "Aborting reply.");
		return 1;
	}

	auto result = import_composed(bts, *message);
	if (result.kind != LightBTS::ImportResult::Kind::appended) {
		std::println(std::cerr, "Reply could not be added to an existing bug.");
		return 1;
	}

	auto ticket = bts.get_ticket(result.ticket);
	std::println(std::cerr, "Thank you for reporting additional information for bug number {} ({})", ticket.id, bts.short_id(ticket));
	return 0;
}

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <print>

#include "commands.hpp"
#include "compose.hpp"
#include "pseudoheaders.hpp"
#include "strings.hpp"

int do_describe(const Options &options, const Arguments &args) {
	if (args.size() != 1) {
		std::println(std::cerr, "Expected one ticket.");
		return 1;
	}

	auto bts = options.open_instance();
	auto ticket = bts.get_ticket(args[0]);
	auto first_message_id = bts.get_first_message_id(ticket);
	auto description = bts.get_description(ticket);

	auto message = compose_message(bts, options, {
		.subject = ticket.title,
		.pseudo_headers = pseudo_header("Description", "replace"),
		.in_reply_to = first_message_id,
		.editor_text = description,
		.editor_hint = "describe",
	});

	if (!message || LightBTS::trim(LightBTS::strip_pseudo_headers(message->get_text())) == LightBTS::trim(description)) {
		std::println(std::cerr, "The description is unchanged.");
		return 1;
	}

	auto result = import_composed(bts, *message);
	if (result.kind != LightBTS::ImportResult::Kind::appended) {
		std::println(std::cerr, "The description of ticket {} was not changed.", ticket.id);
		return 1;
	}

	std::println(std::cerr, "Changed the description of bug number {} ({})", ticket.id, bts.short_id(ticket));
	return 0;
}

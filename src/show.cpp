/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <print>
#include <ranges>

#include "commands.hpp"
#include "pager.hpp"
#include "ticket-json.hpp"
#include "strings.hpp"

namespace {

std::string join(const auto &range) {
	return range | std::views::join_with(' ') | std::ranges::to<std::string>();
}

void show_ticket_header(LightBTS::Instance &bts, FILE *out, const LightBTS::Ticket &ticket) {
	auto details = bts.get_details(ticket);

	std::print(out, "Bug#{} ({}): {}\n", ticket.id, bts.short_id(ticket), ticket.title);
	for (auto &field: bts.get_fields().fields) {
		auto values = ticket.fields.find(field.name);
		if (values == ticket.fields.end())
			continue;
		auto formatted = values->second | std::views::transform([&](auto &value) { return field.format(value); });
		std::print(out, "{}: {}\n", LightBTS::capitalize(field.label), join(formatted));
	}
	if (!details.found_versions.empty())
		std::print(out, "Found in: {}\n", join(details.found_versions));
	if (!details.fixed_versions.empty())
		std::print(out, "Fixed in: {}\n", join(details.fixed_versions));
	for (auto &link: details.links)
		std::print(out, "This bug {} bug#{}: {}\n", LightBTS::verbose_link_names.at(std::to_underlying(link.type)), link.ticket, link.title);
	for (auto &link: details.reverse_links)
		std::print(out, "This bug is {} bug#{}: {}\n", LightBTS::reverse_link_names.at(std::to_underlying(link.type)), link.ticket, link.title);

	std::print(out, "\n");
}

/// The first lines of a text, followed by "[...]" if it is longer.
std::string first_lines(const std::string &text, int count) {
	std::string::size_type end = 0;
	for (int i = 0; i < count && end != text.npos; i++) {
		end = text.find('\n', end);
		if (end != text.npos)
			end++;
	}
	if (end == text.npos || end == text.size())
		return text;
	return text.substr(0, end) + "[...]\n";
}

int show_message(const Options &options, const std::string &id) {
	auto bts = options.open_instance();
	auto message = bts.get_message(id);
	Pager pager(options, bts.get_config("cli", "pager"));

	if (options.json) {
		auto json = LightBTS::message_to_json(message, LightBTS::unquote_message_id(id), true);
		json.set("ticket", LightBTS::ticket_to_json(bts, bts.get_ticket(id)));
		std::print(pager, "{}\n", json.dump());
		return 0;
	}

	if (options.verbose)
		show_ticket_header(bts, pager, bts.get_ticket(id));

	std::print(pager, "{}", message.to_string());
	return 0;
}

int show_ticket(const Options &options, const std::string &id) {
	auto bts = options.open_instance();
	auto ticket = bts.get_ticket(id);
	Pager pager(options, bts.get_config("cli", "pager"));

	if (options.json) {
		std::print(pager, "{}\n", LightBTS::details_to_json(bts, bts.get_details(ticket), options.verbose).dump());
		return 0;
	}

	show_ticket_header(bts, pager, ticket);

	// Without -v, show the current description, then the Message-IDs of all messages.
	if (!options.verbose)
		std::print(pager, "{}\n", first_lines(bts.get_description(ticket), 10));

	bool first = true;
	for (auto &message_id: bts.get_message_ids(ticket)) {
		if (options.verbose) {
			if (!first)
				std::print(pager, "\n");
			auto message = bts.get_message(message_id);
			for (auto field: {"From", "To", "Subject", "Date", "Message-ID"})
				std::print(pager, "{}: {}\n", field, message.get_header_value(field));
			std::print(pager, "\n{}", message.get_text());
		} else {
			std::print(pager, "{}\n", message_id);
		}

		first = false;
	}

	return 0;
}

}

int do_show(const Options &options, const Arguments &args) {
	if (args.size() != 1) {
		std::println(std::cerr, "Exactly one argument required.");
		return 1;
	}

	if (args[0].contains('@'))
		return show_message(options, args[0]);
	else
		return show_ticket(options, args[0]);
}

int do_get(const Options &options, const Arguments &args) {
	if (args.size() != 2) {
		std::println(std::cerr, "Expected a ticket and the name of a field.");
		return 1;
	}

	auto bts = options.open_instance();
	auto ticket = bts.get_ticket(args[0]);
	auto &name = args[1];

	if (name == "title") {
		std::println("{}", ticket.title);
	} else if (name == "id") {
		std::println("{}", ticket.hash);
	} else if (name == "number") {
		std::println("{}", ticket.id);
	} else {
		auto &field = bts.get_fields().get(name);
		auto values = ticket.fields.find(field.name);
		if (values == ticket.fields.end())
			return 1;
		for (auto &value: values->second)
			std::println("{}", value);
	}

	return 0;
}

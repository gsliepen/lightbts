/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <map>
#include <print>
#include <span>

#include "commands.hpp"
#include "pager.hpp"
#include "ticket-json.hpp"

int do_search(const Options &options, const Arguments &args) {
	// Words before "--" are searched for, the arguments after it are selectors.
	std::span<const std::string> words = args;
	std::span<const std::string> selectors;
	if (options.separator) {
		words = words.first(*options.separator);
		selectors = std::span(args).subspan(*options.separator);
	}

	if (words.empty()) {
		std::println(std::cerr, "Nothing to search for.");
		return 1;
	}

	auto bts = options.open_instance();
	auto &definitions = bts.get_fields();

	// Like lbts list, only open tickets are searched by default.
	auto results = bts.search(LightBTS::make_search_query(words), LightBTS::parse_filter(definitions, selectors));

	// Results are sorted by relevance, unless --sort is given.
	if (!options.sort.empty()) {
		std::vector<LightBTS::Ticket> tickets;
		std::map<std::int64_t, LightBTS::SearchResult> by_number;
		for (auto &result: results) {
			tickets.push_back(result.ticket);
			by_number.emplace(result.ticket.id, std::move(result));
		}
		LightBTS::sort_tickets(tickets, definitions, options.sort);
		results.clear();
		for (auto &ticket: tickets)
			results.push_back(std::move(by_number.at(ticket.id)));
	}

	Pager pager(options, bts.get_config("cli", "pager"));

	if (options.json) {
		LightBTS::Json json = LightBTS::Json::Array{};
		for (auto &result: results) {
			auto ticket = LightBTS::ticket_to_json(bts, result.ticket);
			LightBTS::Json matches = LightBTS::Json::Array{};
			for (auto &match: result.matches)
				matches.push_back(LightBTS::Json::Object{{"message_id", match.message_id}, {"snippet", match.snippet}});
			ticket.set("matches", matches);
			json.push_back(ticket);
		}
		std::print(pager, "{}\n", json.dump());
		return 0;
	}

	std::vector<LightBTS::Ticket> tickets;
	for (auto &result: results)
		tickets.push_back(result.ticket);
	auto lines = format_ticket_lines(bts, tickets);

	for (std::size_t i = 0; i < results.size(); i++) {
		std::print(pager, "{}\n", lines[i]);
		if (options.verbose)
			for (auto &match: results[i].matches)
				std::print(pager, "        {}: {}\n", match.message_id, match.snippet);
	}

	return 0;
}

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <print>

#include "commands.hpp"

static std::string count(std::size_t number, std::string_view noun) {
	return std::format("{} {}{}", number, noun, number == 1 ? "" : "s");
}

int do_reindex(const Options &options, const Arguments &args) {
	if (!args.empty()) {
		std::println(std::cerr, "Too many arguments");
		return 1;
	}

	auto bts = options.open_instance();
	auto result = bts.reindex();

	for (auto &error: result.errors)
		std::println(std::cerr, "Error: {}", error);

	if (options.verbose)
		for (auto &warning: result.warnings)
			std::println(std::cerr, "Warning: {}", warning);
	else if (!result.warnings.empty())
		std::println(std::cerr, "There are {} with the pseudo-headers of messages, use -v to show them.", count(result.warnings.size(), "problem"));

	for (auto ticket: result.removed_tickets)
		std::println(std::cerr, "Ticket {} no longer exists, no message creates it anymore.", ticket);

	std::println(std::cerr, "Indexed {} in {}.", count(result.messages, "message"), count(result.tickets, "ticket"));

	return result.errors.empty() ? 0 : 1;
}

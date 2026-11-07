/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <print>

#include "commands.hpp"

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
		std::println(std::cerr, "{} messages have problems with their pseudo-headers, use -v to show them.", result.warnings.size());

	for (auto ticket: result.removed_tickets)
		std::println(std::cerr, "Ticket {} no longer exists, its first message is missing or belongs to another ticket.", ticket);

	std::println(std::cerr, "Indexed {} messages in {} tickets.", result.messages, result.tickets);

	return result.errors.empty() ? 0 : 1;
}

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <print>
#include <ranges>

#include "commands.hpp"
#include "compose.hpp"

int do_create(const Options &options, const Arguments &args) {
	if (args.empty()) {
		std::println(std::cerr, "Missing title.");
		return 1;
	}

	auto bts = options.open_instance();

	auto message = compose_message(bts, options, {
		.subject = args | std::views::join_with(' ') | std::ranges::to<std::string>(),
		.pseudo_headers = option_headers(options),
		.editor_hint = "create",
	});

	if (!message) {
		std::println(std::cerr, "Aborting creating new issue.");
		return 1;
	}

	auto result = import_composed(bts, *message);
	if (result.kind != LightBTS::ImportResult::Kind::created) {
		std::println(std::cerr, "Message import failed");
		return 1;
	}

	std::println(std::cerr, "Thank you for reporting a bug, which has been assigned number {}", result.ticket);
	return 0;
}

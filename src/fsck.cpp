/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <print>

#include "commands.hpp"

int do_fsck(const Options &options, const Arguments &args) {
	if (!args.empty()) {
		std::println(std::cerr, "Too many arguments");
		return 1;
	}

	auto bts = options.open_instance();
	auto result = bts.fsck();

	for (auto &problem: result.file_problems)
		std::println("Message file: {}", problem);
	for (auto &problem: result.database_problems)
		std::println("Database: {}", problem);
	for (auto &problem: result.index_problems)
		std::println("Index: {}", problem);

	if (result.ok()) {
		std::println(std::cerr, "No problems found.");
		return 0;
	}

	if (!result.file_problems.empty())
		std::println(std::cerr, "Message files with problems have to be fixed or removed by hand.");
	if (!result.database_problems.empty() || !result.index_problems.empty())
		std::println(std::cerr, "Run lbts reindex to rebuild the index from the messages.");

	return 1;
}

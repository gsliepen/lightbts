/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "pager.hpp"

#include <cstdlib>

Pager::Pager(const Options &options, std::string command) {
	if (options.batch)
		return;

	if (command.empty()) {
		if (auto pager = std::getenv("LIGHTBTS_PAGER"))
			command = pager;
		else if (auto pager = std::getenv("PAGER"))
			command = pager;
		else
			command = "less";
	}

	if (command.empty() || command == "-" || command == "cat")
		return;

	if (!std::getenv("LESS"))
		command = "LESS=FRX " + command;

	if (auto pipe = popen(command.c_str(), "w")) {
		file = pipe;
		is_pipe = true;
	}
}

Pager::~Pager() {
	if (is_pipe)
		pclose(file);
	else
		std::fflush(file);
}

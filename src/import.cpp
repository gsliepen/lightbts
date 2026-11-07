/* LightBTS -- a lightweight issue tracking system
   Copyright © 2018-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <print>

#include "commands.hpp"

using Kind = LightBTS::ImportResult::Kind;

static bool import_one(LightBTS::Instance &bts, std::istream &in, std::string_view name) {
	LightBTS::Message message;
	try {
		message.load(in);
	} catch (const std::exception &error) {
		std::println(std::cerr, "Error parsing {}: {}", name, error.what());
		return false;
	}

	auto result = bts.import(message);
	for (auto &warning: result.warnings)
		std::println(std::cerr, "{}: warning: {}", name, warning);

	switch (result.kind) {
	case Kind::created:
	case Kind::appended:
		return true;
	case Kind::duplicate:
		std::println(std::cerr, "Ignoring duplicate message from {} with Message-ID {}", message.get_header_value("From"), result.message_id);
		return true;
	case Kind::rejected:
		std::println(std::cerr, "{}: rejected by the pre-index hook", name);
		return false;
	}

	return false;
}

int do_import(const Options &options, const Arguments &args) {
	auto bts = options.open_instance();

	if (args.empty())
		return import_one(bts, std::cin, "stdin") ? 0 : 1;

	int result = 0;

	for (auto &filename: args) {
		std::ifstream file(filename, std::ios::binary);
		if (!file.is_open()) {
			std::println(std::cerr, "Could not open {}: {}", filename, std::strerror(errno));
			result = 1;
			continue;
		}

		if (!import_one(bts, file, filename))
			result = 1;
	}

	return result;
}

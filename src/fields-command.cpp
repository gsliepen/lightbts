/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <print>
#include <ranges>

#include "commands.hpp"
#include "pager.hpp"

namespace {

std::string join(const std::vector<std::string> &words) {
	return words | std::views::join_with(std::string_view(", ")) | std::ranges::to<std::string>();
}

void print_variable(FILE *out, std::string_view variable, const std::string &value) {
	if (!value.empty())
		std::print(out, "\t{}: {}\n", variable, value);
}

}

int do_fields(const Options &options, const Arguments &args) {
	if (!args.empty()) {
		std::println(std::cerr, "Too many arguments");
		return 1;
	}

	auto bts = options.open_instance();
	auto &definitions = bts.get_fields();
	Pager pager(options, bts.get_config("cli", "pager"));

	bool first = true;
	for (auto &field: definitions.fields) {
		if (!first)
			std::print(pager, "\n");
		first = false;

		std::print(pager, "{}\n", field.name);
		print_variable(pager, "type", std::string(LightBTS::field_type_names[std::to_underlying(field.type)]));
		if (field.label != field.name)
			print_variable(pager, "label", field.label);
		print_variable(pager, "aliases", join(field.aliases));
		print_variable(pager, "values", join(field.values));
		if (field.ordered)
			print_variable(pager, "ordered", "true");
		if (field.minimum)
			print_variable(pager, "min", std::to_string(*field.minimum));
		if (field.maximum)
			print_variable(pager, "max", std::to_string(*field.maximum));
		print_variable(pager, "unit", field.unit);
		print_variable(pager, "default", join(field.default_values));

		if (field.name == "status") {
			auto &status = definitions.status;
			auto to_do = field.values | std::views::filter([&](auto &value) { return status.category(value) == LightBTS::StatusCategory::to_do; }) | std::ranges::to<std::vector>();
			print_variable(pager, "to-do", join(to_do));
			print_variable(pager, "in-progress", join(status.in_progress));
			print_variable(pager, "done", join(status.done));
			print_variable(pager, "close", status.close);
			print_variable(pager, "reopen", status.reopen);
		}
	}

	return 0;
}

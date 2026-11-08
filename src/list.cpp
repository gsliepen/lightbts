/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <algorithm>
#include <print>
#include <set>
#include <span>

#include "commands.hpp"
#include "pager.hpp"
#include "ticket-json.hpp"

namespace {

/// The field whose values to list, for a last word like "tags" or "milestones".
const LightBTS::FieldDefinition *find_listed_field(const LightBTS::FieldDefinitions &definitions, std::string_view word) {
	for (auto &field: definitions.fields)
		if (word == field.name || word == field.name + "s")
			return &field;
	return nullptr;
}

/// The width of a column, which fits all possible values of enum fields, otherwise all values that are shown.
std::size_t column_width(const LightBTS::FieldDefinition &field, const std::vector<LightBTS::Ticket> &tickets) {
	std::size_t width = 0;
	if (field.type == LightBTS::FieldType::enumeration)
		for (auto &value: field.values)
			width = std::max(width, value.size());
	for (auto &ticket: tickets)
		width = std::max(width, field.format(ticket.get(field.name)).size());
	return width;
}

}

int do_list(const Options &options, const Arguments &args) {
	auto bts = options.open_instance();
	auto &definitions = bts.get_fields();

	std::span<const std::string> selectors = args;
	const LightBTS::FieldDefinition *listed_field = nullptr;

	if (!selectors.empty()) {
		listed_field = find_listed_field(definitions, selectors.back());
		if (listed_field || selectors.back() == "bugs" || selectors.back() == "tickets")
			selectors = selectors.first(selectors.size() - 1);
	}

	auto tickets = bts.list(LightBTS::parse_filter(definitions, selectors));
	LightBTS::sort_tickets(tickets, definitions, options.sort.empty() ? bts.get_config("cli", "sort") : options.sort);
	Pager pager(options, bts.get_config("cli", "pager"));

	if (listed_field) {
		std::set<std::string> values;
		for (auto &ticket: tickets)
			if (auto found = ticket.fields.find(listed_field->name); found != ticket.fields.end())
				values.insert(found->second.begin(), found->second.end());
		if (options.json)
			std::print(pager, "{}\n", LightBTS::Json::array_of(values).dump());
		else
			for (auto &value: values)
				std::print(pager, "{}\n", listed_field->format(value));
		return 0;
	}

	if (options.json) {
		LightBTS::Json json = LightBTS::Json::Array{};
		for (auto &ticket: tickets)
			json.push_back(LightBTS::ticket_to_json(bts, ticket));
		std::print(pager, "{}\n", json.dump());
		return 0;
	}

	auto columns = definitions.parse_columns(bts.get_config("cli", "columns"));
	std::vector<std::size_t> widths;
	for (auto column: columns)
		widths.push_back(column_width(*column, tickets));

	for (auto &ticket: tickets) {
		std::string line = std::format("{:>6} {}", ticket.id, bts.short_id(ticket));
		for (std::size_t i = 0; i < columns.size(); i++)
			line += std::format(" {:{}}", columns[i]->format(ticket.get(columns[i]->name)), widths[i]);
		std::print(pager, "{}  {}\n", line, ticket.title);
	}

	return 0;
}

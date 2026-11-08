/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "ticket-json.hpp"

#include <chrono>
#include <ranges>

#include "strings.hpp"

namespace LightBTS {

namespace {

Json field_value(const FieldDefinition &field, const std::string &value) {
	if (field.type == FieldType::number)
		if (auto number = parse_integer(value))
			return *number;
	return value;
}

Json optional_integer(const std::optional<std::int64_t> &value) {
	return value ? Json(*value) : Json();
}

Json link_to_json(Instance &bts, const Link &link) {
	auto other = bts.get_ticket(link.ticket);
	return Json::Object{
		{"type", std::string(name(link.type))},
		{"id", other.hash},
		{"short_id", bts.short_id(other)},
		{"number", link.ticket},
		{"title", link.title},
	};
}

}

Json fields_to_json(const FieldDefinitions &definitions, const Ticket &ticket) {
	Json fields = Json::Object{};
	for (auto &field: definitions.fields) {
		auto values = ticket.fields.find(field.name);
		if (values == ticket.fields.end())
			continue;
		if (field.has_multiple_values())
			fields.set(field.name, Json::array_of(values->second));
		else
			fields.set(field.name, field_value(field, values->second.front()));
	}
	return fields;
}

Json ticket_to_json(Instance &bts, const Ticket &ticket) {
	return Json::Object{
		{"id", ticket.hash},
		{"short_id", bts.short_id(ticket)},
		{"number", ticket.id},
		{"title", ticket.title},
		{"fields", fields_to_json(bts.get_fields(), ticket)},
	};
}

Json details_to_json(Instance &bts, const TicketDetails &details, bool with_text) {
	auto json = ticket_to_json(bts, details.ticket);

	json.set("submitter", details.submitter);
	if (details.date)
		json.set("date", std::format("{:%FT%TZ}", std::chrono::sys_seconds{std::chrono::seconds{details.date}}));
	else
		json.set("date", nullptr);

	json.set("versions", Json::Object{
		{"found", Json::array_of(details.found_versions)},
		{"fixed", Json::array_of(details.fixed_versions)},
	});

	Json links = Json::Array{};
	for (auto &link: details.links)
		links.push_back(link_to_json(bts, link));
	json.set("links", links);

	Json reverse_links = Json::Array{};
	for (auto &link: details.reverse_links)
		reverse_links.push_back(link_to_json(bts, link));
	json.set("linked_from", reverse_links);

	Json messages = Json::Array{};
	for (auto &message_id: bts.get_message_ids(details.ticket))
		messages.push_back(message_to_json(bts.get_message(message_id), message_id, with_text));
	json.set("messages", messages);

	return json;
}

Json message_to_json(const Message &message, std::string_view message_id, bool with_text) {
	Json json = Json::Object{{"message_id", message_id}};
	for (auto [header, name]: {std::pair{"From", "from"}, {"To", "to"}, {"Subject", "subject"}, {"Date", "date"}})
		json.set(name, message.get_header_value(header));
	if (with_text)
		json.set("text", message.get_text());
	return json;
}

Json field_definitions_to_json(const FieldDefinitions &definitions) {
	Json fields = Json::Array{};

	for (auto &field: definitions.fields) {
		Json json = Json::Object{
			{"name", field.name},
			{"type", std::string(field_type_names[std::to_underlying(field.type)])},
			{"label", field.label},
			{"aliases", Json::array_of(field.aliases)},
			{"values", Json::array_of(field.values)},
			{"default", Json::array_of(field.default_values)},
		};

		if (field.type == FieldType::enumeration)
			json.set("ordered", field.ordered);

		if (field.type == FieldType::number) {
			json.set("min", optional_integer(field.minimum));
			json.set("max", optional_integer(field.maximum));
			json.set("unit", field.unit);
		}

		if (field.name == "status") {
			auto &status = definitions.status;
			auto to_do = field.values | std::views::filter([&](auto &value) { return status.category(value) == StatusCategory::to_do; });
			json.set("categories", Json::Object{
				{"to-do", Json::array_of(to_do)},
				{"in-progress", Json::array_of(status.in_progress)},
				{"done", Json::array_of(status.done)},
			});
			json.set("close", status.close);
			json.set("reopen", status.reopen);
		}

		fields.push_back(json);
	}

	return fields;
}

}

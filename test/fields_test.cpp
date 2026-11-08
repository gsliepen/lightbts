/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <format>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "check.hpp"
#include "fields.hpp"

using namespace LightBTS;

namespace {

using Variables = std::initializer_list<std::pair<std::string_view, std::string_view>>;

Config status_config(Variables extra = {}) {
	Config config;
	config.set("field.status", "type", "enum");
	config.set("field.status", "values", "new, confirmed, in-progress, fixed, wontfix");
	config.set("field.status", "in-progress", "in-progress");
	config.set("field.status", "done", "fixed, wontfix");
	for (auto [variable, value]: extra)
		config.set("field.status", variable, value);
	return config;
}

/// Parses the status field plus one other field with the given variables.
FieldDefinitions parse_with(std::string_view name, Variables variables) {
	auto config = status_config();
	for (auto [variable, value]: variables)
		config.set(std::format("field.{}", name), variable, value);
	return parse_field_definitions(config);
}

bool is_invalid(std::string_view name, Variables variables) {
	return throws([&] { parse_with(name, variables); });
}

bool is_invalid_status(Variables variables) {
	return throws([&] { parse_field_definitions(status_config(variables)); });
}

}

int main() {
	// Without field sections, the default fields are used
	{
		Config config;
		config.set("core", "project", "Test");
		auto definitions = parse_field_definitions(config);

		std::vector<std::string> names;
		for (auto &field: definitions.fields)
			names.push_back(field.name);
		CHECK(names == std::vector<std::string>{"status", "severity", "tags", "owner", "milestone", "deadline", "progress"});

		CHECK(definitions.find("Tag") == definitions.find("tags"));
		CHECK(!definitions.find("title"));
		CHECK(definitions.find("severity")->ordered);
		CHECK(definitions.find("severity")->default_value == "normal");
		CHECK(definitions.find("progress")->allows("100"));
		CHECK(!definitions.find("progress")->allows("101"));
		CHECK(!definitions.find("progress")->allows("-1"));
		CHECK(!definitions.find("progress")->allows("5%"));
		CHECK(definitions.find("deadline")->allows("2026-02-28"));
		CHECK(!definitions.find("deadline")->allows("2026-02-30"));
		CHECK(!definitions.find("deadline")->allows("2026-2-3"));
		CHECK(definitions.find("tags")->allows("anything"));
		CHECK(!definitions.find("tags")->allows("two words"));

		CHECK(definitions.status.is_open("open"));
		CHECK(!definitions.status.is_open("closed"));
		CHECK(definitions.status.close == "closed");
		CHECK(definitions.status.reopen == "open");
	}

	// The default fields written into a configuration are the same as the built-in ones
	{
		Config config;
		add_default_fields(config);
		CHECK(parse_field_definitions(config).fields.size() == 7);
	}

	// Status categories, and the defaults of close, reopen and default
	{
		auto definitions = parse_field_definitions(status_config());
		auto &status = definitions.status;
		CHECK(status.category("new") == StatusCategory::to_do);
		CHECK(status.category("confirmed") == StatusCategory::to_do);
		CHECK(status.category("in-progress") == StatusCategory::in_progress);
		CHECK(status.category("wontfix") == StatusCategory::done);
		CHECK(definitions.find("status")->default_value == "new");
		CHECK(status.close == "fixed");
		CHECK(status.reopen == "new");
	}
	{
		auto definitions = parse_field_definitions(status_config({{"default", "Confirmed"}, {"close", "wontfix"}}));
		CHECK(definitions.find("status")->default_value == "confirmed");
		CHECK(definitions.status.close == "wontfix");
		CHECK(definitions.status.reopen == "confirmed");
	}

	// Invalid status definitions
	CHECK(throws([] { Config config; config.set("field.severity", "type", "enum"); config.set("field.severity", "values", "low"); parse_field_definitions(config); }));
	CHECK(is_invalid_status({{"type", "text"}}));
	CHECK(is_invalid_status({{"done", ""}}));
	CHECK(is_invalid_status({{"done", "new, confirmed, in-progress, fixed, wontfix"}}));
	CHECK(is_invalid_status({{"done", "fixed, unknown"}}));
	CHECK(is_invalid_status({{"in-progress", "in-progress, fixed"}}));
	CHECK(is_invalid_status({{"default", "fixed"}}));
	CHECK(is_invalid_status({{"close", "new"}}));
	CHECK(is_invalid_status({{"close", "fixed, wontfix"}}));
	CHECK(is_invalid_status({{"reopen", "wontfix"}}));

	// Other fields
	{
		auto definitions = parse_with("Priority", {{"type", "enum"}, {"values", "Low, normal, high"}, {"default", "normal"}, {"ordered", "yes"}, {"label", "How urgent"}, {"aliases", "prio"}});
		auto priority = definitions.find("PRIO");
		CHECK(priority && priority->name == "priority");
		CHECK(priority->type == FieldType::enumeration);
		CHECK(priority->values == std::vector<std::string>{"low", "normal", "high"});
		CHECK(priority->label == "How urgent");
		CHECK(priority->ordered);
		CHECK(priority->allows("LOW"));
		CHECK(!priority->allows("urgent"));
	}
	{
		auto definitions = parse_with("component", {{"type", "set"}, {"values", "core, web"}, {"default", "core web"}});
		CHECK(definitions.find("component")->allows("web"));
		CHECK(!definitions.find("component")->allows("email"));
	}
	CHECK(!is_invalid("estimate", {{"type", "number"}, {"min", "0"}, {"unit", "h"}, {"default", "1"}}));
	CHECK(!is_invalid("due", {{"type", "date"}, {"default", "2026-12-31"}}));
	CHECK(!is_invalid("reviewer", {{"type", "user"}, {"default", "Some One <some@example.org>"}}));
	CHECK(!is_invalid("milestone", {{"type", "text"}, {"label", "Target"}}));

	// Invalid field definitions
	CHECK(is_invalid("missing-type", {{"values", "a, b"}}));
	CHECK(is_invalid("unknown-type", {{"type", "colour"}}));
	CHECK(is_invalid("no-values", {{"type", "enum"}}));
	CHECK(is_invalid("duplicate-values", {{"type", "enum"}, {"values", "a, b, A"}}));
	CHECK(is_invalid("not-a-word", {{"type", "set"}, {"values", "a, b/c"}}));
	CHECK(is_invalid("bad-default", {{"type", "enum"}, {"values", "a, b"}, {"default", "c"}}));
	CHECK(is_invalid("bad-set-default", {{"type", "set"}, {"values", "a, b"}, {"default", "a c"}}));
	CHECK(is_invalid("bad-date", {{"type", "date"}, {"default", "tomorrow"}}));
	CHECK(is_invalid("bad-number", {{"type", "number"}, {"max", "10"}, {"default", "11"}}));
	CHECK(is_invalid("bad-limit", {{"type", "number"}, {"min", "zero"}}));
	CHECK(is_invalid("bad-limits", {{"type", "number"}, {"min", "10"}, {"max", "0"}}));
	CHECK(is_invalid("bad-ordered", {{"type", "enum"}, {"values", "a"}, {"ordered", "maybe"}}));
	CHECK(is_invalid("misplaced", {{"type", "text"}, {"values", "a"}}));
	CHECK(is_invalid("misplaced", {{"type", "set"}, {"ordered", "true"}}));
	CHECK(is_invalid("misplaced", {{"type", "enum"}, {"values", "a"}, {"unit", "h"}}));
	CHECK(is_invalid("misplaced", {{"type", "enum"}, {"values", "a"}, {"done", "a"}}));
	CHECK(is_invalid("typo", {{"type", "text"}, {"lable", "Typo"}}));
	CHECK(is_invalid("two words", {{"type", "text"}}));
	CHECK(is_invalid("", {{"type", "text"}}));
	CHECK(is_invalid("title", {{"type", "text"}}));
	CHECK(is_invalid("unblocks", {{"type", "text"}}));
	CHECK(is_invalid("other", {{"type", "text"}, {"aliases", "fixed"}}));
	CHECK(is_invalid("other", {{"type", "text"}, {"aliases", "status"}}));
	CHECK(throws([] { Config config = status_config(); config.set("field", "type", "text"); parse_field_definitions(config); }));
	CHECK(throws([] { Config config = status_config(); config.set("field.Status", "type", "text"); parse_field_definitions(config); }));

	return check_result();
}

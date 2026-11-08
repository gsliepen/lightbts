/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <format>
#include <initializer_list>
#include <string>
#include <string_view>
#include <set>
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

bool allows(const FieldDefinitions &definitions, std::string_view field, std::string_view value) {
	return definitions.find(field)->normalize(value).has_value();
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
		CHECK(definitions.find("severity")->default_values == std::vector<std::string>{"normal"});
		CHECK(allows(definitions, "progress", "100"));
		CHECK(!allows(definitions, "progress", "101"));
		CHECK(!allows(definitions, "progress", "-1"));
		CHECK(allows(definitions, "progress", "5%"));
		CHECK(definitions.find("progress")->normalize(" 05 % ") == "5");
		CHECK(definitions.find("progress")->format("5") == "5%");
		CHECK(definitions.find("progress")->format("").empty());
		CHECK(!allows(definitions, "progress", "5 percent"));
		CHECK(allows(definitions, "deadline", "2026-02-28"));
		CHECK(!allows(definitions, "deadline", "2026-02-30"));
		CHECK(!allows(definitions, "deadline", "2026-2-3"));
		CHECK(allows(definitions, "tags", "anything"));
		CHECK(!allows(definitions, "tags", "two words"));

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
		CHECK(definitions.find("status")->default_values.front() == "new");
		CHECK(status.close == "fixed");
		CHECK(status.reopen == "new");
	}
	{
		auto definitions = parse_field_definitions(status_config({{"default", "Confirmed"}, {"close", "wontfix"}}));
		CHECK(definitions.find("status")->default_values.front() == "confirmed");
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
		CHECK(priority->normalize("LOW"));
		CHECK(!priority->normalize("urgent"));
	}
	{
		auto definitions = parse_with("component", {{"type", "set"}, {"values", "core, web"}, {"default", "core web"}});
		CHECK(allows(definitions, "component", "web"));
		CHECK(!allows(definitions, "component", "email"));
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
	CHECK(is_invalid("not-a-word", {{"type", "set"}, {"values", "a, b=c"}}));
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
	CHECK(is_invalid("received", {{"type", "text"}}));
	CHECK(is_invalid("done", {{"type", "text"}}));
	CHECK(is_invalid("other", {{"type", "text"}, {"aliases", "all"}}));
	CHECK(is_invalid("other", {{"type", "text"}, {"aliases", "fixed"}}));
	CHECK(is_invalid("other", {{"type", "text"}, {"aliases", "status"}}));
	CHECK(throws([] { Config config = status_config(); config.set("field", "type", "text"); parse_field_definitions(config); }));
	CHECK(throws([] { Config config = status_config(); config.set("field.Status", "type", "text"); parse_field_definitions(config); }));

	CHECK(is_invalid("bad-value", {{"type", "set"}, {"values", "a, -b"}}));
	CHECK(!is_invalid("good-values", {{"type", "set"}, {"values", "c++, v1.0, a/b"}}));
	CHECK(is_invalid("bad-alias", {{"type", "text"}, {"aliases", "a.b"}}));

	// Header names and columns
	{
		auto definitions = parse_with("due-date", {{"type", "date"}});
		CHECK(definitions.find("due-date")->header_name() == "Due-Date");
		CHECK(definitions.parse_columns("").size() == 1);
		CHECK(definitions.parse_columns("due-date, status").front()->name == "due-date");
		CHECK(throws([&] { definitions.parse_columns("unknown"); }));
	}

	// The signature changes with the definitions, but not with whitespace
	{
		auto signature = [](std::string_view values) {
			Config config;
			add_default_fields(config);
			config.set("field.severity", "values", values);
			return parse_field_definitions(config).signature;
		};
		CHECK(signature("low, normal") != signature("low, normal, high"));
		CHECK(signature("low, normal") == signature("  low, normal "));
		Config defaults;
		add_default_fields(defaults);
		CHECK(parse_field_definitions(defaults).signature == parse_field_definitions(Config()).signature);
	}

	// Selectors
	{
		auto definitions = parse_with("priority", {{"type", "enum"}, {"values", "low, normal, high"}, {"ordered", "true"}});
		definitions.fields.push_back(parse_field_definitions(Config()).get("tags"));
		definitions.fields.push_back(parse_field_definitions(Config()).get("progress"));
		definitions.fields.push_back(parse_field_definitions(Config()).get("owner"));

		auto parse = [&](std::vector<std::string> selectors) { return parse_filter(definitions, selectors); };
		using enum StatusCategory;

		CHECK(parse({}).categories == std::set{to_do, in_progress});
		CHECK(parse({"all"}).categories.size() == 3);
		CHECK(parse({"closed"}).categories == std::set{done});
		CHECK(parse({"in-progress"}).categories == std::set{in_progress});
		CHECK(parse({"done", "open"}).categories == std::set{to_do, in_progress});

		auto filter = parse({"priority>=High"});
		CHECK(filter.conditions.size() == 1);
		CHECK(filter.conditions[0].field == "priority");
		CHECK(filter.conditions[0].comparison == Comparison::greater_equal);
		CHECK(filter.conditions[0].value == "high");

		filter = parse({"Tag=foo", "progress<50%", "owner!=Some One"});
		CHECK(filter.conditions[0].field == "tags");
		CHECK(filter.conditions[1].comparison == Comparison::less);
		CHECK(filter.conditions[1].value == "50");
		CHECK(filter.conditions[2].comparison == Comparison::not_equal);
		CHECK(filter.conditions[2].value == "Some One");

		// Bare words are values of enum fields, otherwise of set fields
		filter = parse({"low", "bar"});
		CHECK(filter.conditions[0].field == "priority");
		CHECK(filter.conditions[1].field == "tags");
		CHECK(filter.conditions[1].value == "bar");

		// Selecting a status selects tickets in any category, unless a category is given
		CHECK(parse({"wontfix"}).categories.size() == 3);
		CHECK(parse({"status=new"}).categories.size() == 3);
		CHECK(parse({"status=new", "closed"}).categories == std::set{done});

		CHECK(throws([&] { parse({"unknown=1"}); }));
		CHECK(throws([&] { parse({"priority=highest"}); }));
		CHECK(throws([&] { parse({"tags>foo"}); }));
		CHECK(throws([&] { parse({"owner<a"}); }));
		CHECK(throws([&] { parse({"priority=>low"}); }));
		CHECK(throws([&] { parse({"progress=lots"}); }));
	}

	return check_result();
}

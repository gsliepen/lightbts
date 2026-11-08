/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "fields.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <format>
#include <ranges>
#include <set>
#include <sstream>
#include <stdexcept>

#include "lightbts.hpp"
#include "strings.hpp"

namespace LightBTS {

namespace {

constexpr std::string_view section_prefix = "field.";

/// Names that fields can't have: other pseudo-headers, the X-LightBTS headers that are not fields,
/// and words with a special meaning for lbts list.
std::set<std::string, std::less<>> reserved_names() {
	std::set<std::string, std::less<>> names = {
		"title", "topic", "description", "version", "found", "notfound", "fixed", "notfixed",
		"received", "control",
		"all", "open", "closed", "bugs", "tickets",
	};
	names.insert(status_category_names.begin(), status_category_names.end());
	for (auto link: link_names) {
		names.emplace(link);
		names.insert(std::format("un{}", link));
	}
	return names;
}

/// Whether text is valid as the name of a field or an alias.
bool is_name(std::string_view text) {
	return !text.empty() && std::ranges::all_of(text, [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; });
}

/// Whether text is valid as a value of an enum or set field. Values can't contain characters that have
/// a meaning in pseudo-headers and selectors.
bool is_value(std::string_view text) {
	return !text.empty() && !text.starts_with('+') && !text.starts_with('-') && text.find_first_of(" \t\r\n,=<>!") == text.npos;
}

bool is_date(std::string_view text) {
	std::istringstream stream{std::string(text)};
	std::chrono::year_month_day date;
	stream >> std::chrono::parse("%F", date);
	return stream && stream.peek() == EOF && date.ok() && text.size() == 10;
}

/// Parses a list of words, separated by commas, as they appear in the values, aliases and status variables.
std::vector<std::string> parse_words(std::string_view text, bool (*is_valid)(std::string_view) = is_value) {
	auto words = split(to_lower(text), ", \t");
	for (auto &word: words)
		if (!is_valid(word))
			throw std::runtime_error(std::format("\"{}\" is not valid as a name or value", word));
	return words;
}

bool contains(const std::vector<std::string> &list, std::string_view value) {
	return std::ranges::find(list, value) != list.end();
}

FieldDefinition parse_field(const Config::Section &section, StatusDefinition &status) {
	FieldDefinition field;
	field.name = to_lower(std::string_view(section.name).substr(section_prefix.size()));
	if (!is_name(field.name))
		throw std::runtime_error("the name must be a single word of letters, digits, - and _");

	auto has = [&](std::string_view variable) { return std::ranges::contains(section.variables, variable, &std::pair<std::string, std::string>::first); };
	auto get = [&](std::string_view variable) { return std::string(trim(std::ranges::find(section.variables, variable, &std::pair<std::string, std::string>::first)->second)); };

	if (!has("type"))
		throw std::runtime_error("the type is missing");
	auto type = parse_enum<FieldType>(field_type_names, to_lower(get("type")));
	if (!type)
		throw std::runtime_error(std::format("unknown type \"{}\"", get("type")));
	field.type = *type;

	using enum FieldType;
	bool is_status = field.name == "status";

	for (auto &[variable, value]: section.variables) {
		auto allowed = [&] {
			if (variable == "type" || variable == "label" || variable == "aliases" || variable == "default")
				return true;
			if (variable == "values")
				return field.type == enumeration || field.type == set;
			if (variable == "ordered")
				return field.type == enumeration;
			if (variable == "min" || variable == "max" || variable == "unit")
				return field.type == number;
			if (variable == "in-progress" || variable == "done" || variable == "close" || variable == "reopen")
				return is_status;
			return false;
		};
		if (!allowed())
			throw std::runtime_error(std::format("the variable \"{}\" is not allowed here", variable));
	}

	field.label = has("label") ? get("label") : field.name;
	if (has("aliases"))
		field.aliases = parse_words(get("aliases"), is_name);
	if (has("values"))
		field.values = parse_words(get("values"));
	if (has("unit"))
		field.unit = get("unit");

	if (field.type == enumeration && field.values.empty())
		throw std::runtime_error("an enum field needs values");
	if (std::set(field.values.begin(), field.values.end()).size() != field.values.size())
		throw std::runtime_error("the values contain duplicates");

	if (has("ordered")) {
		auto ordered = parse_bool(get("ordered"));
		if (!ordered)
			throw std::runtime_error(std::format("invalid value \"{}\" for ordered", get("ordered")));
		field.ordered = *ordered;
	}

	for (auto [variable, limit]: {std::pair{"min", &field.minimum}, {"max", &field.maximum}}) {
		if (!has(variable))
			continue;
		*limit = parse_integer(get(variable));
		if (!*limit)
			throw std::runtime_error(std::format("{} must be an integer", variable));
	}
	if (field.minimum && field.maximum && *field.minimum > *field.maximum)
		throw std::runtime_error("min is larger than max");

	if (has("default")) {
		auto text = get("default");
		auto words = field.has_multiple_values() ? split(text) : std::vector<std::string>{text};
		for (auto &word: words) {
			auto value = field.normalize(word);
			if (!value)
				throw std::runtime_error(std::format("the default \"{}\" is not {}", word, field.describe_valid_values()));
			field.default_values.push_back(*value);
		}
	}

	if (is_status) {
		if (field.type != enumeration)
			throw std::runtime_error("the status field must be an enum field");

		auto statuses = [&](std::string_view variable) {
			auto words = has(variable) ? parse_words(get(variable)) : std::vector<std::string>{};
			for (auto &word: words)
				if (!contains(field.values, word))
					throw std::runtime_error(std::format("\"{}\" in {} is not one of the values", word, variable));
			return words;
		};

		status.in_progress = statuses("in-progress");
		status.done = statuses("done");

		if (status.done.empty())
			throw std::runtime_error("done must list at least one value");
		for (auto &value: status.in_progress)
			if (contains(status.done, value))
				throw std::runtime_error(std::format("\"{}\" is both in-progress and done", value));
		if (status.done.size() == field.values.size())
			throw std::runtime_error("at least one value must not be done");

		auto single_status = [&](std::string_view variable) {
			auto words = statuses(variable);
			if (words.size() > 1)
				throw std::runtime_error(std::format("{} must be a single value", variable));
			return words.empty() ? std::string() : words.front();
		};

		if (field.default_values.empty())
			field.default_values.push_back(*std::ranges::find_if(field.values, [&](auto &value) { return status.is_open(value); }));
		status.close = single_status("close");
		if (status.close.empty())
			status.close = status.done.front();
		status.reopen = single_status("reopen");
		if (status.reopen.empty())
			status.reopen = field.default_values.front();

		if (!status.is_open(field.default_values.front()))
			throw std::runtime_error("the default must not be done");
		if (status.is_open(status.close))
			throw std::runtime_error("close must be done");
		if (!status.is_open(status.reopen))
			throw std::runtime_error("reopen must not be done");
	}

	return field;
}

/// Text that represents the definitions in a section, for the signature of all definitions.
std::string section_text(const Config::Section &section) {
	auto text = std::format("[{}]\n", section.name);
	for (auto &[variable, value]: section.variables)
		text += std::format("{} = {}\n", variable, trim(value));
	return text;
}

std::optional<Comparison> parse_comparison(std::string_view text) {
	auto it = std::ranges::find(comparison_operators, text);
	if (it == comparison_operators.end())
		return std::nullopt;
	return static_cast<Comparison>(it - comparison_operators.begin());
}

/// Parses a selector of the form <field><operator><value>.
std::optional<Condition> parse_condition(const FieldDefinitions &definitions, std::string_view selector) {
	auto operator_start = selector.find_first_of("!<>=");
	if (operator_start == selector.npos)
		return std::nullopt;
	auto operator_end = selector.find_first_not_of("!<>=", operator_start);
	if (operator_end == selector.npos)
		operator_end = selector.size();

	auto name = selector.substr(0, operator_start);
	auto text = selector.substr(operator_end);
	auto comparison = parse_comparison(selector.substr(operator_start, operator_end - operator_start));
	if (!comparison)
		throw std::runtime_error(std::format("Invalid operator in \"{}\"", selector));

	auto &field = definitions.get(name);
	if (*comparison != Comparison::equal && *comparison != Comparison::not_equal && !field.is_comparable())
		throw std::runtime_error(std::format("The values of the {} field can't be compared with {}", field.name, comparison_operators[std::to_underlying(*comparison)]));

	auto value = field.normalize(text);
	if (!value)
		throw std::runtime_error(std::format("Invalid {} \"{}\", expected {}", field.name, text, field.describe_valid_values()));

	return Condition{field.name, *comparison, *value};
}

/// Parses a selector of the form <link type>=<ticket>.
std::optional<LinkCondition> parse_link_condition(std::string_view selector) {
	auto equals = selector.find('=');
	if (equals == selector.npos || !parse_link_type(selector.substr(0, equals)))
		return std::nullopt;
	return LinkCondition{std::string(selector.substr(0, equals)), std::string(selector.substr(equals + 1))};
}

/// Finds a value in the first enum field that has it, otherwise in the first set field that lists it,
/// and otherwise in the first set field that allows any value.
std::optional<Condition> find_value(const FieldDefinitions &definitions, std::string_view selector) {
	auto find = [&](auto matches) -> std::optional<Condition> {
		for (auto &field: definitions.fields)
			if (matches(field))
				if (auto value = field.normalize(selector))
					return Condition{field.name, Comparison::equal, *value};
		return std::nullopt;
	};

	if (auto condition = find([](auto &field) { return field.type == FieldType::enumeration; }))
		return condition;
	if (auto condition = find([](auto &field) { return field.type == FieldType::set && !field.values.empty(); }))
		return condition;
	return find([](auto &field) { return field.type == FieldType::set; });
}

}

bool FieldDefinition::has_name(std::string_view other) const {
	auto lower = to_lower(other);
	return name == lower || contains(aliases, lower);
}

std::optional<std::string> FieldDefinition::normalize(std::string_view text) const {
	auto value = trim(text);

	switch (type) {
	case FieldType::enumeration:
	case FieldType::set: {
		auto lower = to_lower(value);
		if (values.empty() ? is_value(lower) : contains(values, lower))
			return lower;
		return std::nullopt;
	}

	case FieldType::date:
		if (is_date(value))
			return std::string(value);
		return std::nullopt;

	case FieldType::number: {
		if (!unit.empty() && value.ends_with(unit))
			value = trim(value.substr(0, value.size() - unit.size()));
		auto number = parse_integer(value);
		if (!number || (minimum && *number < *minimum) || (maximum && *number > *maximum))
			return std::nullopt;
		return std::to_string(*number);
	}

	case FieldType::text:
	case FieldType::user:
		if (value.empty())
			return std::nullopt;
		return std::string(value);
	}

	return std::nullopt;
}

std::string FieldDefinition::describe_valid_values() const {
	switch (type) {
	case FieldType::enumeration:
		return std::format("one of {}", values | std::views::join_with(std::string_view(", ")) | std::ranges::to<std::string>());
	case FieldType::set:
		if (values.empty())
			return "a word without spaces, commas or any of = < > !, that doesn't start with + or -";
		return std::format("any of {}", values | std::views::join_with(std::string_view(", ")) | std::ranges::to<std::string>());
	case FieldType::date:
		return "a date of the form YYYY-MM-DD";
	case FieldType::number:
		if (minimum && maximum)
			return std::format("an integer from {} to {}", *minimum, *maximum);
		if (minimum)
			return std::format("an integer of at least {}", *minimum);
		if (maximum)
			return std::format("an integer of at most {}", *maximum);
		return "an integer";
	case FieldType::text:
	case FieldType::user:
		return "a text that is not empty";
	}
	return {};
}

std::string FieldDefinition::header_name() const {
	auto result = name;
	for (std::size_t i = 0; i < result.size(); i++)
		if (i == 0 || result[i - 1] == '-')
			result[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[i])));
	return result;
}

std::string FieldDefinition::format(std::string_view value) const {
	if (type == FieldType::number && !value.empty())
		return std::format("{}{}", value, unit);
	return std::string(value);
}

StatusCategory StatusDefinition::category(std::string_view value) const {
	if (contains(done, value))
		return StatusCategory::done;
	if (contains(in_progress, value))
		return StatusCategory::in_progress;
	return StatusCategory::to_do;
}

const FieldDefinition *FieldDefinitions::find(std::string_view name) const {
	auto field = std::ranges::find_if(fields, [&](auto &field) { return field.has_name(name); });
	return field == fields.end() ? nullptr : &*field;
}

const FieldDefinition &FieldDefinitions::get(std::string_view name) const {
	if (auto field = find(name))
		return *field;
	throw std::runtime_error(std::format("There is no field named \"{}\"", name));
}

std::vector<const FieldDefinition *> FieldDefinitions::parse_columns(std::string_view names) const {
	std::vector<const FieldDefinition *> columns;

	if (trim(names).empty()) {
		for (auto name: {"status", "severity"})
			if (auto field = find(name))
				columns.push_back(field);
		return columns;
	}

	for (auto &name: split(names))
		columns.push_back(&get(name));
	return columns;
}

void add_default_fields(Config &config) {
	config.set("field.status", "type", "enum");
	config.set("field.status", "values", "open, closed");
	config.set("field.status", "default", "open");
	config.set("field.status", "done", "closed");
	config.set("field.status", "close", "closed");
	config.set("field.status", "reopen", "open");

	config.set("field.severity", "type", "enum");
	config.set("field.severity", "values", "wishlist, minor, normal, important, serious, critical, grave");
	config.set("field.severity", "default", "normal");
	config.set("field.severity", "ordered", "true");

	config.set("field.tags", "type", "set");
	config.set("field.tags", "aliases", "tag");

	config.set("field.owner", "type", "user");

	config.set("field.milestone", "type", "text");

	config.set("field.deadline", "type", "date");

	config.set("field.progress", "type", "number");
	config.set("field.progress", "min", "0");
	config.set("field.progress", "max", "100");
	config.set("field.progress", "unit", "%");
}

FieldDefinitions parse_field_definitions(const Config &config) {
	auto is_field_section = [](const Config::Section &section) { return section.name.starts_with(section_prefix) || section.name == "field"; };

	if (std::ranges::none_of(config.get_sections(), is_field_section)) {
		Config defaults;
		add_default_fields(defaults);
		return parse_field_definitions(defaults);
	}

	FieldDefinitions definitions;
	auto reserved = reserved_names();
	std::set<std::string, std::less<>> names;

	for (auto &section: config.get_sections() | std::views::filter(is_field_section)) {
		try {
			if (section.name == "field")
				throw std::runtime_error("the name is missing");

			auto field = parse_field(section, definitions.status);
			definitions.signature += section_text(section);

			auto all_names = field.aliases;
			all_names.insert(all_names.begin(), field.name);
			for (auto &name: all_names) {
				if (reserved.contains(name))
					throw std::runtime_error(std::format("\"{}\" is reserved", name));
				if (!names.insert(name).second)
					throw std::runtime_error(std::format("\"{}\" is already used by another field", name));
			}

			definitions.fields.push_back(std::move(field));
		} catch (const std::runtime_error &error) {
			throw std::runtime_error(std::format("Invalid definition of field \"{}\": {}", section.name.substr(std::min(section.name.size(), section_prefix.size())), error.what()));
		}
	}

	if (!definitions.find("status"))
		throw std::runtime_error("The status field is not defined");

	return definitions;
}

}

namespace LightBTS {

Filter parse_filter(const FieldDefinitions &definitions, std::span<const std::string> selectors) {
	using enum StatusCategory;
	Filter filter;
	bool has_category = false;

	for (auto &selector: selectors) {
		if (selector == "all" || selector == "open" || selector == "closed" || parse_enum<StatusCategory>(status_category_names, selector))
			has_category = true;

		if (selector == "all")
			filter.categories = {to_do, in_progress, done};
		else if (selector == "open")
			filter.categories = {to_do, in_progress};
		else if (selector == "closed")
			filter.categories = {done};
		else if (auto category = parse_enum<StatusCategory>(status_category_names, selector))
			filter.categories = {*category};
		else if (auto link = parse_link_condition(selector))
			filter.links.push_back(*link);
		else if (auto condition = parse_condition(definitions, selector))
			filter.conditions.push_back(*condition);
		else if (auto value = find_value(definitions, selector))
			filter.conditions.push_back(*value);
		else
			throw std::runtime_error(std::format("Invalid selector \"{}\"", selector));
	}

	// Selecting a status, like "lbts list wontfix", should not be limited to open tickets.
	if (!has_category && std::ranges::contains(filter.conditions, "status", &Condition::field))
		filter.categories = {to_do, in_progress, done};

	return filter;
}

}

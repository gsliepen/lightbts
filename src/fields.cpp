/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "fields.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
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

/// Names of pseudo-headers that are not fields.
std::set<std::string, std::less<>> reserved_names() {
	std::set<std::string, std::less<>> names = {"title", "topic", "version", "found", "notfound", "fixed", "notfixed"};
	for (auto link: link_names) {
		names.emplace(link);
		names.insert(std::format("un{}", link));
	}
	return names;
}

bool is_word(std::string_view text) {
	return !text.empty() && std::ranges::all_of(text, [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; });
}

std::optional<std::int64_t> parse_number(std::string_view text) {
	std::int64_t number;
	auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
	if (error != std::errc() || end != text.data() + text.size())
		return std::nullopt;
	return number;
}

bool is_date(std::string_view text) {
	std::istringstream stream{std::string(text)};
	std::chrono::year_month_day date;
	stream >> std::chrono::parse("%F", date);
	return stream && stream.peek() == EOF && date.ok() && text.size() == 10;
}

/// Parses a list of words, separated by commas, as they appear in the values, aliases and status variables.
std::vector<std::string> parse_words(std::string_view text) {
	auto words = split(to_lower(text), ", \t");
	for (auto &word: words)
		if (!is_word(word))
			throw std::runtime_error(std::format("\"{}\" is not a single word", word));
	return words;
}

bool contains(const std::vector<std::string> &list, std::string_view value) {
	return std::ranges::find(list, value) != list.end();
}

FieldDefinition parse_field(const Config::Section &section, StatusDefinition &status) {
	FieldDefinition field;
	field.name = to_lower(std::string_view(section.name).substr(section_prefix.size()));
	if (!is_word(field.name))
		throw std::runtime_error("the name must be a single word");

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
		field.aliases = parse_words(get("aliases"));
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
		*limit = parse_number(get(variable));
		if (!*limit)
			throw std::runtime_error(std::format("{} must be an integer", variable));
	}
	if (field.minimum && field.maximum && *field.minimum > *field.maximum)
		throw std::runtime_error("min is larger than max");

	if (has("default")) {
		field.default_value = field.type == enumeration || field.type == set ? to_lower(get("default")) : get("default");
		auto &value = field.default_value;
		switch (field.type) {
		case enumeration:
			if (!field.allows(value))
				throw std::runtime_error(std::format("the default \"{}\" is not one of the values", value));
			break;
		case set:
			for (auto &word: parse_words(value))
				if (!field.allows(word))
					throw std::runtime_error(std::format("the default \"{}\" is not one of the values", word));
			break;
		case date:
			if (!is_date(value))
				throw std::runtime_error(std::format("the default \"{}\" is not a date of the form YYYY-MM-DD", value));
			break;
		case number:
			if (!field.allows(value))
				throw std::runtime_error(std::format("the default \"{}\" is not an integer within the limits", value));
			break;
		case text:
		case user:
			break;
		}
	}

	if (is_status) {
		if (field.type != enumeration)
			throw std::runtime_error("the status field must be an enum field");

		auto statuses = [&](std::string_view variable) {
			auto words = has(variable) ? parse_words(get(variable)) : std::vector<std::string>{};
			for (auto &word: words)
				if (!field.allows(word))
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

		if (field.default_value.empty())
			field.default_value = *std::ranges::find_if(field.values, [&](auto &value) { return status.is_open(value); });
		status.close = single_status("close");
		if (status.close.empty())
			status.close = status.done.front();
		status.reopen = single_status("reopen");
		if (status.reopen.empty())
			status.reopen = field.default_value;

		if (!status.is_open(field.default_value))
			throw std::runtime_error("the default must not be done");
		if (status.is_open(status.close))
			throw std::runtime_error("close must be done");
		if (!status.is_open(status.reopen))
			throw std::runtime_error("reopen must not be done");
	}

	return field;
}

}

bool FieldDefinition::has_name(std::string_view other) const {
	auto lower = to_lower(other);
	return name == lower || contains(aliases, lower);
}

bool FieldDefinition::allows(std::string_view value) const {
	switch (type) {
	case FieldType::enumeration:
		return contains(values, to_lower(value));
	case FieldType::set:
		return values.empty() ? is_word(value) : contains(values, to_lower(value));
	case FieldType::date:
		return is_date(value);
	case FieldType::number: {
		auto number = parse_number(value);
		return number && (!minimum || *number >= *minimum) && (!maximum || *number <= *maximum);
	}
	case FieldType::text:
	case FieldType::user:
		return !value.empty();
	}
	return false;
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

			auto all_names = field.aliases;
			all_names.insert(all_names.begin(), field.name);
			for (auto &name: all_names) {
				if (reserved.contains(name))
					throw std::runtime_error(std::format("\"{}\" is a built-in pseudo-header", name));
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

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "config.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <print>
#include <stdexcept>

#include "strings.hpp"

namespace LightBTS {

void Config::load(const std::filesystem::path &path) {
	std::ifstream file(path);
	if (!file.is_open())
		throw std::runtime_error(std::format("Could not read {}", path.string()));

	std::string line;
	std::string section;

	while (std::getline(file, line)) {
		auto text = trim(line);

		if (text.empty() || text.front() == '#' || text.front() == ';')
			continue;

		if (text.front() == '[') {
			if (text.back() == ']')
				section = trim(text.substr(1, text.size() - 2));
			continue;
		}

		auto equals = text.find('=');
		if (equals == text.npos)
			continue;

		auto variable = trim(text.substr(0, equals));
		if (variable.empty())
			continue;

		set(section, variable, trim(text.substr(equals + 1)));
	}

	if (file.bad())
		throw std::runtime_error(std::format("Error while reading {}", path.string()));
}

void Config::save(const std::filesystem::path &path) const {
	auto temporary = path;
	temporary += ".new";

	{
		std::ofstream file(temporary);
		for (auto &[section, variables]: sections) {
			if (&section != &sections.front().name)
				std::println(file);
			std::println(file, "[{}]", section);
			for (auto &[variable, value]: variables)
				std::println(file, "\t{} = {}", variable, value);
		}
		file.close();
		if (!file)
			throw std::runtime_error(std::format("Could not write {}", temporary.string()));
	}

	std::filesystem::rename(temporary, path);
}

const std::string *Config::find(std::string_view section, std::string_view variable) const {
	auto found_section = std::ranges::find(sections, section, &Section::name);
	if (found_section == sections.end())
		return nullptr;
	auto &variables = found_section->variables;
	auto found_variable = std::ranges::find(variables, variable, &std::pair<std::string, std::string>::first);
	if (found_variable == variables.end())
		return nullptr;
	return &found_variable->second;
}

bool Config::exists(std::string_view section, std::string_view variable) const {
	return find(section, variable);
}

std::string Config::get(std::string_view section, std::string_view variable, std::string_view fallback) const {
	auto value = find(section, variable);
	return std::string(value ? *value : fallback);
}

bool Config::get_bool(std::string_view section, std::string_view variable, bool fallback) const {
	if (!exists(section, variable))
		return fallback;

	if (auto value = parse_bool(get(section, variable)))
		return *value;
	throw std::runtime_error(std::format("Invalid value for boolean variable {}.{}", section, variable));
}

void Config::set(std::string_view section, std::string_view variable, std::string_view value) {
	auto found_section = std::ranges::find(sections, section, &Section::name);
	if (found_section == sections.end())
		found_section = sections.insert(sections.end(), {std::string(section), {}});

	auto &variables = found_section->variables;
	auto found_variable = std::ranges::find(variables, variable, &std::pair<std::string, std::string>::first);
	if (found_variable == variables.end())
		variables.emplace_back(variable, value);
	else
		found_variable->second = value;
}

void Config::merge(const Config &other) {
	for (auto &[section, variables]: other.sections)
		for (auto &[variable, value]: variables)
			set(section, variable, value);
}

void Config::set_bool(std::string_view section, std::string_view variable, bool value) {
	set(section, variable, value ? "true" : "false");
}

}

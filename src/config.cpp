/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "config.hpp"

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

		sections[section][std::string(variable)] = trim(text.substr(equals + 1));
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

bool Config::exists(const std::string &section, const std::string &variable) const {
	auto it = sections.find(section);
	return it != sections.end() && it->second.contains(variable);
}

std::string Config::get(const std::string &section, const std::string &variable, const std::string &fallback) const {
	if (auto it = sections.find(section); it != sections.end())
		if (auto value = it->second.find(variable); value != it->second.end())
			return value->second;
	return fallback;
}

bool Config::get_bool(const std::string &section, const std::string &variable, bool fallback) const {
	if (!exists(section, variable))
		return fallback;

	auto value = to_lower(get(section, variable));
	if (value == "true" || value == "yes" || value == "1")
		return true;
	if (value == "false" || value == "no" || value == "0")
		return false;
	throw std::runtime_error(std::format("Invalid value for boolean variable {}.{}", section, variable));
}

void Config::set(const std::string &section, const std::string &variable, const std::string &value) {
	sections[section][variable] = value;
}

void Config::set_bool(const std::string &section, const std::string &variable, bool value) {
	sections[section][variable] = value ? "true" : "false";
}

}

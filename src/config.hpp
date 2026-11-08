#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace LightBTS {

/// An INI-style configuration file with sections and variables.
/// Sections and variables keep the order in which they were read or added.
class Config {
	public:
	struct Section {
		std::string name;
		std::vector<std::pair<std::string, std::string>> variables;
	};

	void load(const std::filesystem::path &path);
	void save(const std::filesystem::path &path) const;

	bool exists(std::string_view section, std::string_view variable) const;
	std::string get(std::string_view section, std::string_view variable, std::string_view fallback = {}) const;
	bool get_bool(std::string_view section, std::string_view variable, bool fallback) const;
	void set(std::string_view section, std::string_view variable, std::string_view value);
	void set_bool(std::string_view section, std::string_view variable, bool value);
	/// Sets all variables of another configuration, overriding those that are set in both.
	void merge(const Config &other);

	const std::vector<Section> &get_sections() const { return sections; }

	private:
	std::vector<Section> sections;

	const std::string *find(std::string_view section, std::string_view variable) const;
};

}

#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <filesystem>
#include <map>
#include <string>

namespace LightBTS {

/// An INI-style configuration file with sections and variables.
class Config {
	std::map<std::string, std::map<std::string, std::string>, std::less<>> sections;

	public:
	void load(const std::filesystem::path &path);
	void save(const std::filesystem::path &path) const;

	bool exists(const std::string &section, const std::string &variable) const;
	std::string get(const std::string &section, const std::string &variable, const std::string &fallback = {}) const;
	bool get_bool(const std::string &section, const std::string &variable, bool fallback) const;
	void set(const std::string &section, const std::string &variable, const std::string &value);
	void set_bool(const std::string &section, const std::string &variable, bool value);
};

}

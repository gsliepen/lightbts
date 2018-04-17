#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <map>
#include <string>
#include <boost/filesystem.hpp>

class Config {
	std::map<std::string, std::map<std::string, std::string>> db;

	public:
	void load(const boost::filesystem::path &path);
	void save(const boost::filesystem::path &path);
	std::string get(const std::string &section, const std::string &variable, const std::string &def = {});
	bool get_bool(const std::string &section, const std::string &variable, bool def);
	void set(const std::string &section, const std::string &variable, const std::string &value);
	void set_bool(const std::string &section, const std::string &variable, const bool value);
	bool exists(const std::string &section, const std::string &variable = "");
};

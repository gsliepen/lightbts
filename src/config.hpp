#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017 Guus Sliepen <guus@lightbts.info>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

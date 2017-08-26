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

#include "config.hpp"
#include "sqlite3.hpp"

class LightBTS {
	std::string base_dir;

	std::string project;
	std::string admin;

	bool quiet;
	bool no_hooks;
	bool no_email;
	bool respond_to_new;
	bool respond_to_reply;

	SQLite3::database db;
	Config config;

	void init_index(const std::string &path);

	public:
	LightBTS(const std::string &path);
	~LightBTS();

	std::string get_config(const std::string &section, const std::string &variable);
	void set_config(const std::string &section, const std::string &variable, const std::string &value);
};

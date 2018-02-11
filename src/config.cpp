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

#include <fstream>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include "config.hpp"

using namespace std;
using namespace fmt;
namespace fs = boost::filesystem;

static void strip(string &s) {
	auto start = s.find_first_not_of(" \t\r\n");
	auto end = s.find_last_not_of(" \t\r\n");
	if (start == string::npos) {
		s.clear();
	} else {
		s.erase(end + 1);
		s.erase(0, start);
	}
}

void Config::load(const fs::path &path) {
	ifstream file(path.string());

	string line;
	string section;

	while (getline(file, line)) {
		strip(line);

		if (line.empty() || line[0] == '#')
			continue;

		if (line[0] == '[') {
			if (line[line.size() - 1] == ']')
				section = line.substr(1, line.size() - 2);
			continue;
		}

		auto it = line.find('=');
		if (it == string::npos)
			continue;

		string variable = line.substr(0, it - 1);
		string value = line.substr(it + 1);
		strip(variable);
		strip(value);

		if (variable.empty() || value.empty())
			continue;

		db[section][variable] = value;
	}
}

void Config::save(const fs::path &path) {
	ofstream file(path.string());

	for (auto &section: db) {
		print(file, "[{}]\n", section.first);
		for (auto &entry: section.second)
			print(file, "\t{} = {}\n", entry.first, entry.second);
	}
}

bool Config::exists(const string &section, const string &variable) {
	if (db.find(section) == db.end())
		return false;

	if (!variable.empty() && db[section].find(variable) == db[section].end())
		return false;

	return true;
}

void Config::set(const string &section, const string &variable, const string &value) {
	db[section][variable] = value;
}

void Config::set_bool(const string &section, const string &variable, const bool value) {
	db[section][variable] = value ? "true" : "false";
}

string Config::get(const string &section, const string &variable, const string &def) {
	auto sit = db.find(section);
	if (sit != db.end()) {
		auto vit = sit->second.find(variable);
		if (vit != db[section].end())
			return vit->second;
	}

	if (!def.empty())
		db[section][variable] = def;

	return def;
}

bool Config::get_bool(const string &section, const string &variable, bool def) {
	auto val = get(section, variable, def ? "true" : "false");
	if (val == "true" || val == "yes")
		return true;
	if (val == "false" || val == "no")
		return false;
	throw runtime_error("Invalid value for boolean variable " + section + "." + variable);
}


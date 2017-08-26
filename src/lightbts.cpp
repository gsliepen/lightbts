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

#include "lightbts.hpp"

using namespace std;

LightBTS::LightBTS(const string &path): base_dir(path) {
	// Read configuration
	config.load(base_dir + "/config");

	project = config.get("core", "project");
	admin = config.get("core", "admin");
	respond_to_new = config.get("core", "respond-to-new", "yes") == "yes";
	respond_to_reply = config.get("core", "respond-to-reply", "yes") == "yes";

	// Open the index
	init_index(index_filename);
}

LightBTS::~LightBTS() {
}

string LightBTS::get_config(const string &section, const string &variable) {
	return config.get(section, variable);
}

void LightBTS::set_config(const string &section, const string &variable, const string &value) {
	return config.set(section, variable, value);
}

#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

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

#include <unistd.h>

#include <string>
#include <vector>

extern bool verbose;
extern bool no_hooks;
extern bool no_email;
extern bool batch;

extern const std::string lightbts_version;

extern std::string severity;
extern std::string data_dir;
extern std::string cl_message;

extern std::vector<std::string> tags;
extern std::vector<std::string> versions;
extern std::vector<std::string> attachments;

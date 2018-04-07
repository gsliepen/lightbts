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

#include <string>
#include <vector>

extern int do_close(const char *argv0, const std::vector<std::string> &args);
extern int do_reopen(const char *argv0, const std::vector<std::string> &args);
extern int do_retitle(const char *argv0, const std::vector<std::string> &args);
extern int do_found(const char *argv0, const std::vector<std::string> &args);
extern int do_notfound(const char *argv0, const std::vector<std::string> &args);
extern int do_fixed(const char *argv0, const std::vector<std::string> &args);
extern int do_notfixed(const char *argv0, const std::vector<std::string> &args);
extern int do_severity(const char *argv0, const std::vector<std::string> &args);
extern int do_link(const char *argv0, const std::vector<std::string> &args);
extern int do_unlink(const char *argv0, const std::vector<std::string> &args);
extern int do_tags(const char *argv0, const std::vector<std::string> &args);
extern int do_owner(const char *argv0, const std::vector<std::string> &args);
extern int do_noowner(const char *argv0, const std::vector<std::string> &args);
extern int do_deadline(const char *argv0, const std::vector<std::string> &args);
extern int do_milestone(const char *argv0, const std::vector<std::string> &args);
extern int do_progress(const char *argv0, const std::vector<std::string> &args);

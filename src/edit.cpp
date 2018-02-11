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

#include "edit.hpp"

#include <iostream>
#include <fmt/ostream.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lightbts.hpp"

using namespace std;
using namespace fmt;

bool edit_interactive(LightBTS::Instance &bts, LightBTS::Message &msg, const string &hint) {
	string editor = bts.get_config("cli", "editor");
	if (editor.empty() && getenv("VISUAL"))
		editor = getenv("VISUAL");
	if (editor.empty() && getenv("EDITOR"))
		editor = getenv("EDITOR");
	if (editor.empty())
		editor = "edit";

	string tmpdir;
	if (getenv("XDG_RUNTIME_DIR"))
		tmpdir = getenv("XDG_RUNTIME_DIR");
	else
		tmpdir = "/tmp";
	string tmpl = tmpdir + "/lightbts-" + hint + "-XXXXXX.eml";
	int fd = mkstemps((char *)tmpl.c_str(), 4);
	if (fd == -1) {
		print(cerr, "Could not create temporary file {}: {}\n", tmpl, strerror(errno));
		return false;
	}

	string buf = msg.to_string();
	if(write(fd, buf.data(), buf.size()) != buf.size()) {
		unlink(tmpl.c_str());
		close(fd);
		throw runtime_error("Error writing to temporary file\n");
	}

	close(fd);

	struct stat stat_before, stat_after;

	stat(tmpl.c_str(), &stat_before);

	string command = editor + " + " + tmpl;
	if(system(command.c_str()) != 0) {
		print(cerr, "{} returned with a non-zero exit code\n", editor);
		unlink(tmpl.c_str());
		return false;
	}

	stat(tmpl.c_str(), &stat_after);

	if (stat_before.st_mtime == stat_after.st_mtime && stat_before.st_size == stat_after.st_size) {
		print(cerr, "Message was not edited.\n");
		unlink(tmpl.c_str());
		return false;
	}

	msg.clear();
	msg.load(tmpl);
	unlink(tmpl.c_str());

	return true;
}

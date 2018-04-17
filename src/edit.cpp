/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
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

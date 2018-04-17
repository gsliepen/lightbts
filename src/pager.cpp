/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "pager.hpp"

#include "cli.hpp"

using namespace std;

Pager::Pager(string cmd) {
	if (batch) {
		fd = stdout;
		return;
	}

	if (cmd.empty()) {
		if (getenv("LIGHTBTS_PAGER"))
			cmd = getenv("LIGHTBTS_PAGER");
		else if (getenv("PAGER"))
			cmd = getenv("PAGER");
		else
			cmd = "less";
	}
	
	if (cmd.empty() || cmd == "-" || cmd == "cat") {
		fd = stdout;
	} else {
		if (!getenv("LESS"))
			cmd = "LESS=FRX " + cmd;
		fd = popen(cmd.c_str(), "w");
		if (fd)
			fd_is_popen = true;
		else
			fd = stdout;
	}
}

Pager::~Pager() {
	if (fd_is_popen)
		pclose(fd);
}

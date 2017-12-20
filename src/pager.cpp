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
	
	if (cmd == "-") {
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

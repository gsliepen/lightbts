#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <cstdio>
#include <string>

class Pager {
	FILE *fd = nullptr;
	bool fd_is_popen = false;

	public:
	Pager(std::string cmd = {});
	~Pager();
	operator FILE *() { return fd; }
};

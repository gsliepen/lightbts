#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <cstdio>
#include <string>

#include "options.hpp"

/// Output that goes through a pager when running interactively, or to stdout otherwise.
class Pager {
	FILE *file = stdout;
	bool is_pipe = false;

	public:
	Pager(const Options &options, std::string command = {});
	~Pager();

	Pager(const Pager &) = delete;
	Pager &operator=(const Pager &) = delete;

	operator FILE *() { return file; }
};

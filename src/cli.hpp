#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
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

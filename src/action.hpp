#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
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

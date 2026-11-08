#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "options.hpp"

int do_close(const Options &options, const Arguments &args);
int do_reopen(const Options &options, const Arguments &args);
int do_retitle(const Options &options, const Arguments &args);
int do_found(const Options &options, const Arguments &args);
int do_notfound(const Options &options, const Arguments &args);
int do_fixed(const Options &options, const Arguments &args);
int do_notfixed(const Options &options, const Arguments &args);
int do_severity(const Options &options, const Arguments &args);
int do_link(const Options &options, const Arguments &args);
int do_unlink(const Options &options, const Arguments &args);
int do_tags(const Options &options, const Arguments &args);
int do_owner(const Options &options, const Arguments &args);
int do_noowner(const Options &options, const Arguments &args);
int do_deadline(const Options &options, const Arguments &args);
int do_milestone(const Options &options, const Arguments &args);
int do_progress(const Options &options, const Arguments &args);
int do_set(const Options &options, const Arguments &args);
int do_unset(const Options &options, const Arguments &args);

int do_create(const Options &options, const Arguments &args);
int do_fields(const Options &options, const Arguments &args);
int do_fsck(const Options &options, const Arguments &args);
int do_import(const Options &options, const Arguments &args);
int do_list(const Options &options, const Arguments &args);
int do_reindex(const Options &options, const Arguments &args);
int do_reply(const Options &options, const Arguments &args);
int do_show(const Options &options, const Arguments &args);

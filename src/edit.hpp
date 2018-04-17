#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <string>
#include "lightbts.hpp"

extern bool edit_interactive(LightBTS::Instance &bts, LightBTS::Message &msg, const std::string &hint = "bug");

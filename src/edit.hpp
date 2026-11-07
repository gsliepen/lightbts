#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <string_view>

#include "lightbts.hpp"

/// Lets the user edit a message in their editor.
/// \param require_change Whether to abort if the user did not change the message.
/// \return false if editing was aborted.
bool edit_interactive(const LightBTS::Instance &bts, LightBTS::Message &message, std::string_view hint, bool require_change);

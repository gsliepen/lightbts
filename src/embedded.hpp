#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <array>
#include <string_view>

namespace LightBTS {

/// A file embedded in the executable by gentemplates.
struct EmbeddedFile {
	std::string_view name;
	std::string_view data;
};

}

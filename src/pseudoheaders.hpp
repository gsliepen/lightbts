#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <string>
#include <string_view>
#include <vector>

namespace LightBTS {

struct PseudoHeader {
	std::string key;   ///< Always in lower case.
	std::string value; ///< Without leading and trailing whitespace.

	bool operator==(const PseudoHeader &other) const = default;
};

/// Parses the "Key: value" lines at the start of a message body.
///
/// Leading empty lines are skipped. Parsing stops at the first empty line, or at the first line
/// that does not look like a pseudo-header.
std::vector<PseudoHeader> parse_pseudo_headers(std::string_view text);

}

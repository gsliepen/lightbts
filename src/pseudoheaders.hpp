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

/// The pseudo-headers at the start of a message body, and the text that follows them.
struct PseudoHeaderBlock {
	std::vector<PseudoHeader> headers;
	std::string_view text; ///< The rest of the body, without the empty line that ends the pseudo-headers.
};

/// Splits the "Key: value" lines at the start of a message body from the rest of the text.
///
/// Leading empty lines are skipped. The pseudo-headers end at the first empty line, or at the first line
/// that does not look like a pseudo-header.
PseudoHeaderBlock split_pseudo_headers(std::string_view text);

inline std::vector<PseudoHeader> parse_pseudo_headers(std::string_view text) { return split_pseudo_headers(text).headers; }
inline std::string_view strip_pseudo_headers(std::string_view text) { return split_pseudo_headers(text).text; }

}

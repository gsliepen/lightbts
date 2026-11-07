/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "pseudoheaders.hpp"

#include <algorithm>
#include <cctype>

#include "strings.hpp"

namespace LightBTS {

static bool is_key_character(unsigned char c) {
	return std::isalnum(c) || c == '-' || c == '_';
}

std::vector<PseudoHeader> parse_pseudo_headers(std::string_view text) {
	std::vector<PseudoHeader> headers;

	while (!text.empty()) {
		auto end = text.find('\n');
		auto line = trim(text.substr(0, end));
		text = end == text.npos ? std::string_view{} : text.substr(end + 1);

		if (line.empty()) {
			if (headers.empty())
				continue;
			break;
		}

		auto colon = line.find(':');
		if (colon == line.npos)
			break;

		auto key = trim(line.substr(0, colon));
		if (key.empty() || !std::ranges::all_of(key, is_key_character))
			break;

		headers.push_back({to_lower(key), std::string(trim(line.substr(colon + 1)))});
	}

	return headers;
}

}

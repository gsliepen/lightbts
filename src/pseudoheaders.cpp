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

PseudoHeaderBlock split_pseudo_headers(std::string_view text) {
	PseudoHeaderBlock block;
	auto rest = text;

	while (!rest.empty()) {
		auto end = rest.find('\n');
		auto line = trim(rest.substr(0, end));
		auto next = end == rest.npos ? std::string_view{} : rest.substr(end + 1);

		if (line.empty()) {
			rest = next;
			if (block.headers.empty())
				continue;
			break;
		}

		auto colon = line.find(':');
		if (colon == line.npos)
			break;

		auto key = trim(line.substr(0, colon));
		if (key.empty() || !std::ranges::all_of(key, is_key_character))
			break;

		block.headers.push_back({to_lower(key), std::string(trim(line.substr(colon + 1)))});
		rest = next;
	}

	// Without pseudo-headers, the text is unchanged, apart from leading empty lines.
	block.text = rest;
	return block;
}

}

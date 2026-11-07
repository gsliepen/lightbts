#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace LightBTS {

inline constexpr std::string_view whitespace = " \t\r\n";

/// Removes leading and trailing whitespace. The result refers to the same characters as the input.
inline std::string_view trim(std::string_view text) {
	auto start = text.find_first_not_of(whitespace);
	if (start == text.npos)
		return {};
	auto end = text.find_last_not_of(whitespace);
	return text.substr(start, end - start + 1);
}

inline std::string to_lower(std::string_view text) {
	std::string result(text);
	std::ranges::transform(result, result.begin(), [](unsigned char c) { return std::tolower(c); });
	return result;
}

/// Splits text at any of the separator characters, skipping empty tokens.
inline std::vector<std::string> split(std::string_view text, std::string_view separators = ", \t") {
	std::vector<std::string> tokens;
	auto start = text.find_first_not_of(separators);
	while (start != text.npos) {
		auto end = text.find_first_of(separators, start);
		tokens.emplace_back(text.substr(start, end - start));
		start = text.find_first_not_of(separators, end);
	}
	return tokens;
}

}

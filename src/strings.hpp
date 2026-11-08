#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>
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

inline std::string_view trim(const char *text) { return trim(std::string_view(text)); }

/// Trimming a temporary string would return a view of a string that no longer exists.
std::string_view trim(const std::string &&text) = delete;

inline std::string to_lower(std::string_view text) {
	std::string result(text);
	std::ranges::transform(result, result.begin(), [](unsigned char c) { return std::tolower(c); });
	return result;
}

inline std::string capitalize(std::string_view text) {
	std::string result(text);
	if (!result.empty())
		result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
	return result;
}

inline std::string join(const std::vector<std::string> &words, std::string_view separator) {
	std::string result;
	for (auto &word: words) {
		if (!result.empty())
			result += separator;
		result += word;
	}
	return result;
}

/// Parses a whole text as an integer.
inline std::optional<std::int64_t> parse_integer(std::string_view text) {
	std::int64_t value;
	auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc() || end != text.data() + text.size())
		return std::nullopt;
	return value;
}

/// Parses true, yes, 1, false, no or 0, ignoring case.
inline std::optional<bool> parse_bool(std::string_view text) {
	auto lower = to_lower(text);
	if (lower == "true" || lower == "yes" || lower == "1")
		return true;
	if (lower == "false" || lower == "no" || lower == "0")
		return false;
	return std::nullopt;
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

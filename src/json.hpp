#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace LightBTS {

/// A JSON value, which can only be built and written, not parsed. Objects keep the order of their members.
class Json {
	public:
	struct Member;
	using Array = std::vector<Json>;
	using Object = std::vector<Member>;

	Json() = default;
	Json(std::nullptr_t) {}
	Json(bool value): value(value) {}
	Json(int value): value(std::int64_t{value}) {}
	Json(std::int64_t value): value(value) {}
	Json(std::string value): value(std::move(value)) {}
	Json(std::string_view value): value(std::string(value)) {}
	Json(const char *value): value(std::string(value)) {}
	Json(Array value): value(std::move(value)) {}
	Json(Object value): value(std::move(value)) {}

	/// Converts a range of values to an array.
	template<typename Range>
	static Json array_of(Range &&range) {
		Array array;
		for (auto &element: range)
			array.emplace_back(element);
		return array;
	}

	/// Adds an element to an array.
	void push_back(Json element) { std::get<Array>(value).push_back(std::move(element)); }
	/// Adds a member to an object.
	void set(std::string name, Json member);

	/// Writes the value with an indentation of two spaces per level, without a trailing newline.
	std::string dump() const {
		std::string out;
		write(out, 0);
		return out;
	}

	private:
	std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object> value;

	void write(std::string &out, int depth) const;
};

struct Json::Member {
	std::string name;
	Json value;
};

inline void Json::set(std::string name, Json member) {
	std::get<Object>(value).push_back({std::move(name), std::move(member)});
}

namespace JsonDetail {

/// The length of the valid UTF-8 sequence at the start of text, or 0 if it is not valid.
inline std::size_t utf8_sequence_length(std::string_view text) {
	auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
	auto is_continuation = [&](std::size_t i) { return i < text.size() && (byte(i) & 0xc0) == 0x80; };

	if (byte(0) < 0x80)
		return 1;
	if (byte(0) >= 0xc2 && byte(0) < 0xe0 && is_continuation(1))
		return 2;
	if (byte(0) >= 0xe0 && byte(0) < 0xf0 && is_continuation(1) && is_continuation(2)) {
		bool overlong = byte(0) == 0xe0 && byte(1) < 0xa0;
		bool surrogate = byte(0) == 0xed && byte(1) >= 0xa0;
		return overlong || surrogate ? 0 : 3;
	}
	if (byte(0) >= 0xf0 && byte(0) < 0xf5 && is_continuation(1) && is_continuation(2) && is_continuation(3)) {
		bool overlong = byte(0) == 0xf0 && byte(1) < 0x90;
		bool too_large = byte(0) == 0xf4 && byte(1) >= 0x90;
		return overlong || too_large ? 0 : 4;
	}
	return 0;
}

/// Writes a string with JSON escapes. Invalid UTF-8 is replaced by U+FFFD, so the output is always valid JSON.
inline void write_string(std::string &out, std::string_view text) {
	out += '"';
	while (!text.empty()) {
		auto length = utf8_sequence_length(text);
		auto c = static_cast<unsigned char>(text[0]);

		if (length == 0) {
			out += "\\ufffd";
			length = 1;
		} else if (c == '"') {
			out += "\\\"";
		} else if (c == '\\') {
			out += "\\\\";
		} else if (c == '\n') {
			out += "\\n";
		} else if (c == '\t') {
			out += "\\t";
		} else if (c == '\r') {
			out += "\\r";
		} else if (c < 0x20 || c == 0x7f) {
			out += std::format("\\u{:04x}", c);
		} else {
			out += text.substr(0, length);
		}

		text.remove_prefix(length);
	}
	out += '"';
}

}

inline void Json::write(std::string &out, int depth) const {
	auto newline = [&](int level) {
		out += '\n';
		out.append(2 * level, ' ');
	};

	if (std::holds_alternative<std::nullptr_t>(value)) {
		out += "null";
	} else if (auto boolean = std::get_if<bool>(&value)) {
		out += *boolean ? "true" : "false";
	} else if (auto number = std::get_if<std::int64_t>(&value)) {
		out += std::to_string(*number);
	} else if (auto string = std::get_if<std::string>(&value)) {
		JsonDetail::write_string(out, *string);
	} else if (auto array = std::get_if<Array>(&value)) {
		if (array->empty()) {
			out += "[]";
			return;
		}
		out += '[';
		for (std::size_t i = 0; i < array->size(); i++) {
			out += i ? "," : "";
			newline(depth + 1);
			(*array)[i].write(out, depth + 1);
		}
		newline(depth);
		out += ']';
	} else if (auto object = std::get_if<Object>(&value)) {
		if (object->empty()) {
			out += "{}";
			return;
		}
		out += '{';
		for (std::size_t i = 0; i < object->size(); i++) {
			out += i ? "," : "";
			newline(depth + 1);
			JsonDetail::write_string(out, (*object)[i].name);
			out += ": ";
			(*object)[i].value.write(out, depth + 1);
		}
		newline(depth);
		out += '}';
	}
}

}

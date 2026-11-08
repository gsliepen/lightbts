/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <string>
#include <vector>

#include "check.hpp"
#include "json.hpp"

using LightBTS::Json;

int main() {
	CHECK(Json().dump() == "null");
	CHECK(Json(true).dump() == "true");
	CHECK(Json(-42).dump() == "-42");
	CHECK(Json(std::int64_t{1} << 40).dump() == "1099511627776");
	CHECK(Json("text").dump() == "\"text\"");
	CHECK(Json(Json::Array{}).dump() == "[]");
	CHECK(Json(Json::Object{}).dump() == "{}");

	// Escapes
	CHECK(Json("a \"b\" \\ c").dump() == R"("a \"b\" \\ c")");
	CHECK(Json("line\nnext\ttab\r").dump() == R"("line\nnext\ttab\r")");
	CHECK(Json(std::string("\x01\x1f\x7f", 3)).dump() == R"("\u0001\u001f\u007f")");
	CHECK(Json(std::string("nul\0byte", 8)).dump() == R"("nul\u0000byte")");

	// Valid UTF-8 is kept, invalid UTF-8 is replaced
	CHECK(Json("café € \U0001f600").dump() == "\"café € \U0001f600\"");
	const auto replacement = std::string(1, '\\') + "ufffd";
	CHECK(Json("bad \xff byte").dump() == "\"bad " + replacement + " byte\"");
	CHECK(Json("cut \xe2\x82").dump() == "\"cut " + replacement + replacement + "\"");
	CHECK(Json("overlong \xc0\xaf").dump() == "\"overlong " + replacement + replacement + "\"");
	CHECK(Json("surrogate \xed\xa0\x80").dump() == "\"surrogate " + replacement + replacement + replacement + "\"");

	// Nesting and indentation
	Json object = Json::Object{{"name", "value"}, {"list", Json::array_of(std::vector<std::string>{"a", "b"})}};
	object.set("empty", Json::Array{});
	CHECK(object.dump() == "{\n  \"name\": \"value\",\n  \"list\": [\n    \"a\",\n    \"b\"\n  ],\n  \"empty\": []\n}");

	return check_result();
}

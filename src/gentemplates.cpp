/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

// Generates C++ source code that embeds the given files as an array of EmbeddedFile.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <string>

int main(int argc, char *argv[]) {
	if (argc < 2) {
		std::println(std::cerr, "Usage: {} array-name [input...]", argv[0]);
		return 1;
	}

	static constexpr std::string_view delimiter = "__EMBED__";

	std::println("// This is an automatically generated file, DO NOT EDIT!\n");
	std::println("static constexpr std::array<LightBTS::EmbeddedFile, {}> {} = {{{{", argc - 2, argv[1]);

	for (int i = 2; i < argc; i++) {
		std::ifstream in(argv[i], std::ios::binary);
		std::stringstream contents;
		contents << in.rdbuf();
		if (!in) {
			std::println(std::cerr, "Could not read {}", argv[i]);
			return 1;
		}

		if (contents.view().contains(std::string(")") + std::string(delimiter) + "\"")) {
			std::println(std::cerr, "{} contains the raw string delimiter", argv[i]);
			return 1;
		}

		auto filename = std::filesystem::path(argv[i]).filename().string();
		std::println("\t{{\"{}\", R\"{}({}){}\"}},", filename, delimiter, contents.view(), delimiter);
	}

	std::println("}}}};");
}

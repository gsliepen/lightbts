/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "edit.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <unistd.h>

#include "process.hpp"
#include "strings.hpp"

namespace fs = std::filesystem;

namespace {

/// A file created with mkstemps() that is removed when it goes out of scope.
class TemporaryFile {
	std::string path;

	public:
	TemporaryFile(std::string pattern, int suffix_length): path(std::move(pattern)) {
		int fd = mkstemps(path.data(), suffix_length);
		if (fd == -1)
			throw std::runtime_error(std::format("Could not create temporary file {}: {}", path, std::strerror(errno)));
		close(fd);
	}

	~TemporaryFile() { unlink(path.c_str()); }

	TemporaryFile(const TemporaryFile &) = delete;
	TemporaryFile &operator=(const TemporaryFile &) = delete;

	const std::string &get_path() const { return path; }
};

std::string get_editor(const LightBTS::Instance &bts) {
	if (auto editor = bts.get_config("cli", "editor"); !editor.empty())
		return editor;
	for (auto variable: {"VISUAL", "EDITOR"})
		if (auto editor = std::getenv(variable); editor && *editor)
			return editor;
	return "vi";
}

fs::path get_temporary_directory() {
	if (auto dir = std::getenv("XDG_RUNTIME_DIR"); dir && *dir)
		return dir;
	return fs::temp_directory_path();
}

std::string read_file(const std::string &path) {
	std::ifstream file(path, std::ios::binary);
	std::stringstream contents;
	contents << file.rdbuf();
	return contents.str();
}

}

bool edit_interactive(const LightBTS::Instance &bts, LightBTS::Message &message, std::string_view hint, bool require_change) {
	auto editor = get_editor(bts);
	TemporaryFile file((get_temporary_directory() / std::format("lightbts-{}-XXXXXX.eml", hint)).string(), 4);

	auto original = message.to_string();
	{
		std::ofstream out(file.get_path(), std::ios::binary);
		out << original;
		out.close();
		if (!out)
			throw std::runtime_error(std::format("Error writing to temporary file {}", file.get_path()));
	}

	// Let the shell split the editor command, so it can contain arguments.
	if (int exit_code = LightBTS::run_process({"sh", "-c", editor + " \"$1\"", "sh", file.get_path()})) {
		std::println(std::cerr, "{} returned with exit code {}", editor, exit_code);
		return false;
	}

	auto edited = read_file(file.get_path());

	if (LightBTS::trim(edited).empty()) {
		std::println(std::cerr, "Message is empty.");
		return false;
	}

	if (require_change && edited == original) {
		std::println(std::cerr, "Message was not edited.");
		return false;
	}

	message.from_string(edited);
	return true;
}

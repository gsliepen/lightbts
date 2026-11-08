#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <filesystem>
#include <map>
#include <string>
#include <string>
#include <vector>

namespace LightBTS {

struct ProcessOptions {
	std::filesystem::path working_directory;           ///< If empty, the current directory is used.
	std::map<std::string, std::string> environment;    ///< Variables to add to or replace in the environment.
	bool discard_stdin_and_stdout = false;
	std::string *output = nullptr;                     ///< If set, stdout is captured into this string instead.
};

/// Runs a program and waits for it to finish. The program is searched for in PATH.
/// \return The exit code, or 128 plus the signal number if it was killed by a signal.
/// \throws std::system_error if the program could not be started.
int run_process(const std::vector<std::string> &arguments, const ProcessOptions &options = {});

}

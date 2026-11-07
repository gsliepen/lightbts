/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "process.hpp"

#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <string_view>
#include <sys/wait.h>
#include <system_error>

extern char **environ;

namespace LightBTS {

static std::vector<std::string> build_environment(const std::map<std::string, std::string> &overrides) {
	std::vector<std::string> environment;

	for (char **variable = environ; *variable; ++variable) {
		std::string_view entry = *variable;
		auto name = entry.substr(0, entry.find('='));
		if (!overrides.contains(std::string(name)))
			environment.emplace_back(entry);
	}

	for (auto &[name, value]: overrides)
		environment.push_back(name + "=" + value);

	return environment;
}

static std::vector<char *> to_argv(std::vector<std::string> &strings) {
	std::vector<char *> pointers;
	for (auto &string: strings)
		pointers.push_back(string.data());
	pointers.push_back(nullptr);
	return pointers;
}

class SpawnFileActions {
	posix_spawn_file_actions_t actions;

	public:
	SpawnFileActions() { posix_spawn_file_actions_init(&actions); }
	~SpawnFileActions() { posix_spawn_file_actions_destroy(&actions); }
	SpawnFileActions(const SpawnFileActions &) = delete;
	SpawnFileActions &operator=(const SpawnFileActions &) = delete;
	posix_spawn_file_actions_t *get() { return &actions; }
};

int run_process(const std::vector<std::string> &arguments, const ProcessOptions &options) {
	auto argument_strings = arguments;
	auto environment_strings = build_environment(options.environment);
	auto argv = to_argv(argument_strings);
	auto envp = to_argv(environment_strings);

	SpawnFileActions actions;
	if (!options.working_directory.empty())
		posix_spawn_file_actions_addchdir_np(actions.get(), options.working_directory.c_str());
	if (options.discard_stdin_and_stdout) {
		posix_spawn_file_actions_addopen(actions.get(), 0, "/dev/null", O_RDONLY, 0);
		posix_spawn_file_actions_addopen(actions.get(), 1, "/dev/null", O_WRONLY, 0);
	}

	pid_t pid;
	if (int error = posix_spawnp(&pid, argv[0], actions.get(), nullptr, argv.data(), envp.data()))
		throw std::system_error(error, std::generic_category(), "Could not run " + arguments.front());

	int status;
	while (waitpid(pid, &status, 0) == -1)
		if (errno != EINTR)
			throw std::system_error(errno, std::generic_category(), "Could not wait for " + arguments.front());

	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);
	return WEXITSTATUS(status);
}

}

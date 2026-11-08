/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

// Storing LightBTS in git, as described in doc/git.mdwn.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <print>

#include "commands.hpp"
#include "process.hpp"
#include "strings.hpp"

namespace fs = std::filesystem;

namespace {

constexpr std::string_view branch = "lightbts";

constexpr std::string_view gitignore =
	"# Only the messages and the shared configuration are stored in git.\n"
	"/index\n"
	"/index-*\n"
	"/hooks/\n"
	"/templates/\n"
	"/config.local\n"
	"*.new\n";

/// Runs git in a directory, and returns its exit code. Its output is stored in output, if set.
int git(const fs::path &directory, const std::vector<std::string> &arguments, std::string *output = nullptr) {
	std::vector<std::string> command = {"git"};
	command.insert(command.end(), arguments.begin(), arguments.end());
	return LightBTS::run_process(command, {.working_directory = directory, .discard_stdin_and_stdout = true, .output = output});
}

/// Makes the main worktree ignore .lightbts, without changing the project's own .gitignore.
void exclude_from_main_worktree(const fs::path &top) {
	if (git(top, {"check-ignore", "--quiet", ".lightbts"}) == 0)
		return;

	std::string common_dir;
	if (git(top, {"rev-parse", "--git-common-dir"}, &common_dir) != 0)
		throw std::runtime_error("Could not find the git directory");

	auto info = top / LightBTS::trim(common_dir) / "info";
	fs::create_directories(info);
	std::ofstream exclude(info / "exclude", std::ios::app);
	exclude << "/.lightbts\n";
	if (!exclude)
		throw std::runtime_error(std::format("Could not write {}", (info / "exclude").string()));
}

/// Moves the files of an existing instance into the new worktree.
void move_instance(const fs::path &from, const fs::path &to) {
	for (auto &entry: fs::directory_iterator(from))
		fs::rename(entry.path(), to / entry.path().filename());
	fs::remove(from);
}

std::size_t count_lines(std::string_view text) {
	return std::ranges::count(text, '\n');
}

}

int do_init_git(const Options &options, const fs::path &start) {
	std::string top_output;
	if (git(start.empty() ? fs::current_path() : start, {"rev-parse", "--show-toplevel"}, &top_output) != 0) {
		std::println(std::cerr, "Not inside a git repository.");
		return 1;
	}

	fs::path top(LightBTS::trim(top_output));
	auto base = top / ".lightbts";

	if (fs::exists(base / ".git")) {
		std::println(std::cerr, "{} is already a git worktree.", base.string());
		return 1;
	}

	bool has_branch = git(top, {"rev-parse", "--verify", "--quiet", std::format("refs/heads/{}", branch)}) == 0
		|| git(top, {"rev-parse", "--verify", "--quiet", std::format("refs/remotes/origin/{}", branch)}) == 0;
	bool has_instance = fs::exists(base / "config");

	if (has_branch && has_instance) {
		std::println(std::cerr, "There is already a {} branch, and also an instance in {} that is not stored in it.", branch, base.string());
		std::println(std::cerr, "Move {} away, run lbts init --git again, and add its messages with lbts import.", base.string());
		return 1;
	}

	// An existing branch, for example in a new clone, is checked out, and the index is built from its messages.
	if (has_branch) {
		if (git(top, {"worktree", "add", "--quiet", ".lightbts", std::string(branch)}) != 0) {
			std::println(std::cerr, "Could not check out the {} branch in {}.", branch, base.string());
			return 1;
		}
		exclude_from_main_worktree(top);

		auto clone_options = options;
		clone_options.data_dir = top;
		auto bts = clone_options.open_instance();
		auto tickets = bts.list(LightBTS::parse_filter(bts.get_fields(), std::vector<std::string>{"all"}));
		std::println(std::cerr, "Checked out the {} branch in {}, with {} tickets.", branch, base.string(), tickets.size());
		return 0;
	}

	// Otherwise, a new orphan branch is created, and an existing instance is moved into it.
	auto old = top / ".lightbts.old";
	if (has_instance) {
		if (fs::exists(old)) {
			std::println(std::cerr, "{} is in the way.", old.string());
			return 1;
		}
		fs::rename(base, old);
	}

	if (git(top, {"worktree", "add", "--quiet", "--orphan", "-b", std::string(branch), ".lightbts"}) != 0) {
		std::println(std::cerr, "Could not create the {} branch in {}.", branch, base.string());
		if (has_instance)
			fs::rename(old, base);
		return 1;
	}

	exclude_from_main_worktree(top);

	if (has_instance)
		move_instance(old, base);

	auto init_options = options;
	init_options.data_dir = top;
	auto bts = init_options.open_instance(has_instance ? LightBTS::Instance::Mode::open : LightBTS::Instance::Mode::create);
	bts.set_config("core", "git", "true");
	bts.save_config();

	std::ofstream(base / ".gitignore") << gitignore;

	std::vector<fs::path> paths = {"config", ".gitignore"};
	if (fs::exists(base / "messages") && !fs::is_empty(base / "messages"))
		paths.push_back("messages");
	if (!bts.commit(paths, has_instance ? "Store the LightBTS instance in git" : "Initialize LightBTS"))
		return 1;

	std::println(std::cerr, "{} LightBTS in the {} branch, checked out in {}.", has_instance ? "Moved" : "Created", branch, base.string());
	return 0;
}

int do_sync(const Options &options, const Arguments &args) {
	if (!args.empty()) {
		std::println(std::cerr, "Too many arguments");
		return 1;
	}

	std::size_t new_messages = 0;
	bool has_upstream;

	{
		auto bts = options.open_instance();
		if (!bts.uses_git()) {
			std::println(std::cerr, "This instance is not stored in git, see lbts init --git.");
			return 1;
		}

		if (!bts.commit({"."}, "Commit changes that were not committed yet"))
			return 1;

		std::string upstream_output;
		bts.run_git({"for-each-ref", "--format=%(upstream:short)", std::format("refs/heads/{}", branch)}, &upstream_output);
		auto upstream = LightBTS::trim(upstream_output);
		has_upstream = !upstream.empty();

		if (has_upstream) {
			std::string before;
			bts.run_git({"rev-parse", "HEAD"}, &before);

			if (bts.run_git({"pull", "--rebase", "--quiet"}) != 0) {
				std::println(std::cerr, "Pulling from {} failed. Resolve the problem in {} with git, then run lbts sync again.", upstream, bts.get_base_dir().string());
				return 1;
			}

			std::string added;
			bts.run_git({"diff", "--name-only", "--diff-filter=A", std::string(LightBTS::trim(before)), "HEAD", "--", "messages"}, &added);
			new_messages = count_lines(added);
		}
	}

	// Open the instance again, since the configuration may have changed, which rebuilds the index if necessary.
	auto bts = options.open_instance();
	if (new_messages) {
		auto result = bts.reindex();
		for (auto &error: result.errors)
			std::println(std::cerr, "Error: {}", error);
	}

	if (has_upstream)
		std::println(std::cerr, "Received {} new message{}.", new_messages, new_messages == 1 ? "" : "s");

	if (options.no_push)
		return 0;

	if (has_upstream) {
		std::string commits;
		bts.run_git({"rev-list", "--count", "@{upstream}..HEAD"}, &commits);
		if (LightBTS::trim(commits) == "0")
			return 0;
		if (bts.run_git({"push", "--quiet"}) != 0) {
			std::println(std::cerr, "Pushing failed.");
			return 1;
		}
		std::println(std::cerr, "Sent {} commit{}.", LightBTS::trim(commits), LightBTS::trim(commits) == "1" ? "" : "s");
		return 0;
	}

	if (bts.run_git({"remote", "get-url", "origin"}) != 0) {
		std::println(std::cerr, "There is no remote repository to synchronize with.");
		return 1;
	}

	if (bts.run_git({"push", "--quiet", "--set-upstream", "origin", std::string(branch)}) != 0) {
		std::println(std::cerr, "Pushing to origin failed.");
		return 1;
	}

	std::println(std::cerr, "Sent the {} branch to origin.", branch);
	return 0;
}

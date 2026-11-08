/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <algorithm>
#include <cctype>
#include <getopt.h>
#include <iostream>
#include <print>
#include <ranges>
#include <unistd.h>

#include "commands.hpp"
#include "process.hpp"
#include "strings.hpp"
#include "version.hpp"

namespace {

enum LongOnlyOption {
	batch_option = 256,
	no_email_option,
	no_hooks_option,
	json_option,
	link_option,
	sort_option,
	depth_option,
	markdown_option,
	git_option,
	local_option,
	no_push_option,
	version_option,
};

const struct option long_options[] = {
	{"help", no_argument, nullptr, 'h'},
	{"verbose", no_argument, nullptr, 'v'},
	{"batch", no_argument, nullptr, batch_option},
	{"no-email", no_argument, nullptr, no_email_option},
	{"no-hooks", no_argument, nullptr, no_hooks_option},
	{"json", no_argument, nullptr, json_option},
	{"link", required_argument, nullptr, link_option},
	{"sort", required_argument, nullptr, sort_option},
	{"depth", required_argument, nullptr, depth_option},
	{"markdown", no_argument, nullptr, markdown_option},
	{"git", no_argument, nullptr, git_option},
	{"local", no_argument, nullptr, local_option},
	{"no-push", no_argument, nullptr, no_push_option},
	{"data-dir", required_argument, nullptr, 'd'},
	{"version", no_argument, nullptr, version_option},
	{"message", required_argument, nullptr, 'm'},
	{"tag", required_argument, nullptr, 'T'},
	{"severity", required_argument, nullptr, 'S'},
	{"field", required_argument, nullptr, 'F'},
	{"attach", required_argument, nullptr, 'A'},
	{},
};

void show_version() {
	std::print(
		"LightBTS version {}\n"
		"Copyright © 2017-2026 Guus Sliepen\n"
		"\n"
		"License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n",
		lightbts_version);
}

void show_help(std::ostream &out, std::string_view program_name) {
	std::print(out,
		"Usage: {} [options] command [arguments]\n"
		"\n"
		"Options:\n"
		"  -h, --help         Show a help message.\n"
		"  -v, --verbose      Show more details.\n"
		"  --batch            No interactive input.\n"
		"  --no-email         Do not send email messages.\n"
		"  --no-hooks         Do not call hooks.\n"
		"  --json             Write the output of list, show, fields and tree as JSON.\n"
		"  -d, --data-dir=DIR Directory where LightBTS stores its data.\n"
		"  -m, --message=TEXT Use TEXT as the message instead of invoking an editor.\n"
		"  -F, --field=F=V    Set field F to V in a new ticket or reply. Can be given multiple times.\n"
		"  -S, --severity=SEV Set the severity of a new ticket or reply.\n"
		"  -T, --tag=TAG      Add a tag. Can be given multiple times.\n"
		"  -V VERSION         Record a version. Can be given multiple times.\n"
		"  -A, --attach=FILE  Attach a file. Can be given multiple times.\n"
		"  --link=TYPE:ID     Link a new ticket or reply to ticket ID. Can be given multiple times.\n"
		"  --sort=FIELD       Sort tickets by a field, title or number; -FIELD sorts in descending order.\n"
		"  --link=TYPE        For tree, follow links of this type instead of depends.\n"
		"  --depth=N          For tree, show at most N levels below the given ticket.\n"
		"  --markdown         For tree, write Markdown.\n"
		"  --git              For init, store the instance in the lightbts branch of the git repository.\n"
		"  --local            For config, change config.local instead of the shared configuration.\n"
		"  --no-push          For sync, only receive changes.\n"
		"  --version          Show version information.\n"
		"\n"
		"Commands:\n"
		"  help        Show a help message.\n"
		"  init        Initialize a LightBTS instance.\n"
		"  config      Get/set a configuration option.\n"
		"  import      Import messages.\n"
		"  list        List tickets.\n"
		"  tree        Show tickets and the tickets they depend on, recursively.\n"
		"  search      Search the text of all messages.\n"
		"  fields      Show the definitions of the fields of tickets.\n"
		"  show        Show bug or message details.\n"
		"  get         Print the value of a field of a ticket.\n"
		"  create      Create a new bug.\n"
		"  reply       Reply to an existing bug.\n"
		"  close       Close a ticket.\n"
		"  reopen      Reopen a ticket.\n"
		"  retitle     Change the title of a ticket.\n"
		"  describe    Change the description of a ticket.\n"
		"  found       Record versions where a problem is found.\n"
		"  notfound    Remove records of where a problem is found.\n"
		"  fixed       Record versions where a problem is fixed.\n"
		"  notfixed    Remove records of where a problem is fixed.\n"
		"  set         Change a field of a ticket.\n"
		"  unset       Unset a field of a ticket.\n"
		"  severity    Change the severity of a ticket.\n"
		"  link        Add a link between two tickets.\n"
		"  unlink      Remove a link between two tickets.\n"
		"  tags        Add or remove tags.\n"
		"  owner       Change the owner of a ticket.\n"
		"  noowner     Remove ownership of a ticket.\n"
		"  progress    Change the progress level of a ticket.\n"
		"  milestone   Change the milestone of a ticket.\n"
		"  deadline    Change the deadline of a ticket.\n"
		"  reindex     Rebuild the index from the stored messages.\n"
		"  fsck        Check the messages and the index for problems.\n"
		"  sync        Synchronize with the upstream of the lightbts branch in git.\n"
		"  version     Show version information.\n",
		program_name);
}

int do_version(const Options &, const Arguments &) {
	show_version();
	return 0;
}

int do_help(const Options &options, const Arguments &args) {
	if (args.empty()) {
		show_help(std::cout, options.program_name);
		return 0;
	}

	auto &command = args[0];
	if (command.empty() || !std::ranges::all_of(command, [](unsigned char c) { return std::isalnum(c) || c == '-'; })) {
		std::println(std::cerr, "{}: invalid command '{}'", options.program_name, command);
		show_help(std::cerr, options.program_name);
		return 1;
	}

	return LightBTS::run_process({"man", "lbts-" + command});
}

int do_init(const Options &options, const Arguments &args) {
	if (args.size() > 1) {
		std::println(std::cerr, "Too many arguments");
		return 1;
	}

	auto init_options = options;
	if (!args.empty())
		init_options.data_dir = args[0];

	if (options.git)
		return do_init_git(init_options, init_options.data_dir);

	init_options.open_instance(LightBTS::Instance::Mode::create);
	return 0;
}

int do_config(const Options &options, const Arguments &args) {
	if (args.empty()) {
		std::println(std::cerr, "Not enough arguments");
		return 1;
	}

	auto dot = args[0].rfind('.');
	if (dot == std::string::npos) {
		std::println(std::cerr, "Invalid option name");
		return 1;
	}

	auto section = args[0].substr(0, dot);
	auto variable = args[0].substr(dot + 1);

	// Invalid field definitions must not prevent fixing them, and changing them shouldn't rebuild the index every time.
	auto bts = options.open_instance(LightBTS::Instance::Mode::open, {.configuration_only = true});

	if (args.size() > 1) {
		auto value = args | std::views::drop(1) | std::views::join_with(' ') | std::ranges::to<std::string>();
		bts.set_config(section, variable, value, options.local);
		bts.save_config();
		if (!options.local)
			bts.commit({"config"}, std::format("Set {} to \"{}\"", args[0], value));
		try {
			bts.get_fields();
		} catch (const std::runtime_error &error) {
			std::println(std::cerr, "Warning: {}", error.what());
		}
		return 0;
	}

	if (!bts.has_config(section, variable))
		return 1;

	std::println("{}", bts.get_config(section, variable));
	return 0;
}

using CommandFunction = int (*)(const Options &, const Arguments &);

struct Command {
	std::string_view name;
	CommandFunction function;
};

constexpr Command commands[] = {
	{"close", do_close},
	{"config", do_config},
	{"create", do_create},
	{"deadline", do_deadline},
	{"describe", do_describe},
	{"fields", do_fields},
	{"fixed", do_fixed},
	{"found", do_found},
	{"fsck", do_fsck},
	{"get", do_get},
	{"help", do_help},
	{"import", do_import},
	{"init", do_init},
	{"link", do_link},
	{"list", do_list},
	{"milestone", do_milestone},
	{"noowner", do_noowner},
	{"notfixed", do_notfixed},
	{"notfound", do_notfound},
	{"owner", do_owner},
	{"progress", do_progress},
	{"reindex", do_reindex},
	{"reopen", do_reopen},
	{"reply", do_reply},
	{"retitle", do_retitle},
	{"search", do_search},
	{"set", do_set},
	{"severity", do_severity},
	{"show", do_show},
	{"subject", do_retitle},
	{"sync", do_sync},
	{"tag", do_tags},
	{"tags", do_tags},
	{"title", do_retitle},
	{"tree", do_tree},
	{"unlink", do_unlink},
	{"unset", do_unset},
	{"version", do_version},
};

}

int main(int argc, char *argv[]) {
	Options options;
	options.program_name = argv[0];

	std::string command;
	Arguments args;

	int option;
	while ((option = getopt_long(argc, argv, "-hvd:m:V:T:S:F:A:", long_options, nullptr)) != -1) {
		switch (option) {
		case 'h': options.help = true; break;
		case 'v': options.verbose = true; break;
		case batch_option: options.batch = true; break;
		case no_email_option: options.no_email = true; break;
		case no_hooks_option: options.no_hooks = true; break;
		case json_option: options.json = true; break;
		case link_option: options.links.push_back(optarg); break;
		case sort_option: options.sort = optarg; break;
		case markdown_option: options.markdown = true; break;
		case git_option: options.git = true; break;
		case local_option: options.local = true; break;
		case no_push_option: options.no_push = true; break;
		case depth_option:
			if (auto depth = LightBTS::parse_integer(optarg); depth && *depth >= 0) {
				options.depth = static_cast<int>(*depth);
				break;
			}
			std::println(std::cerr, "Invalid depth {}", optarg);
			return 1;
		case 'd': options.data_dir = optarg; break;
		case 'm': options.message = optarg; break;
		case 'V': options.versions.push_back(optarg); break;
		case 'T': options.tags.push_back(optarg); break;
		case 'S': options.severity = optarg; break;
		case 'F': options.fields.push_back(optarg); break;
		case 'A': options.attachments.push_back(optarg); break;
		case version_option: show_version(); return 0;

		case 1:
			if (command.empty())
				command = optarg;
			else
				args.push_back(optarg);
			break;

		default:
			std::println(std::cerr, "Try '{} --help' for more information.", options.program_name);
			return 1;
		}
	}

	// Arguments after "--" are never options.
	if (optind < argc && std::string_view(argv[optind - 1]) == "--") {
		options.separator = args.size();
		for (int i = optind; i < argc; i++) {
			if (command.empty())
				command = argv[i];
			else
				args.push_back(argv[i]);
		}
	}

	if (command.empty()) {
		if (options.help) {
			show_help(std::cout, options.program_name);
			return 0;
		}
		std::println(std::cerr, "{0}: missing command\nTry '{0} --help' for more information.", options.program_name);
		return 1;
	}

	if (options.help) {
		args.insert(args.begin(), command);
		command = "help";
	}

	if (!isatty(0) || !isatty(1))
		options.batch = true;

	auto match = std::ranges::find(commands, command, &Command::name);
	if (match == std::end(commands)) {
		std::println(std::cerr, "{0}: unrecognized command '{1}'\nTry '{0} --help' for more information.", options.program_name, command);
		return 1;
	}

	try {
		return match->function(options, args);
	} catch (const std::exception &error) {
		std::println(std::cerr, "{}: error: {}", options.program_name, error.what());
		return 1;
	}
}

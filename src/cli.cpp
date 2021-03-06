/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "cli.hpp"
#include "lightbts.hpp"

#include <algorithm>
#include <iostream>
#include <getopt.h>
#include <vector>
#include <map>

#include <fmt/ostream.h>

#include "action.hpp"
#include "config.hpp"
#include "create.hpp"
#include "import.hpp"
#include "list.hpp"
#include "reply.hpp"
#include "show.hpp"

using namespace std;
using namespace fmt;

#ifdef VERSION
const string lightbts_version = VERSION;
#else
const string lightbts_version = "unknown";
#endif

bool help;
bool verbose;
bool no_hooks;
bool no_email;
bool batch;

string severity;
string data_dir;
string cl_message;

vector<string> tags;
vector<string> versions;
vector<string> attachments;

static void show_version() {
	print(
			"LightBTS version {}\n"
			"Copyright © 2017-2018 Guus Sliepen\n"
			"\n"
			"License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
			"This is free software: you are free to change and redistribute it.\n"
			"There is NO WARRANTY, to the extent permitted by law.\n",
			lightbts_version
		   );
}

static int do_version(const char *argv0, const vector<string> &args) {
	show_version();
	return 0;
}

static int do_init(const char *argv0, const vector<string> &args) {
	if (args.size() > 1) {
		print(cerr, "Too many arguments\n");
		return 1;
	}

	if (!args.empty())
		data_dir = args[0];

	LightBTS::Instance bts(data_dir, LightBTS::Instance::Flags::INIT);
	return 0;
}

static int do_config(const char *argv0, const vector<string> &args) {
	if (args.empty()) {
		print(cerr, "Not enough arguments\n");
		return 1;
	}

	auto dot = args[0].find('.');
	if (dot == string::npos) {
		print(cerr, "Invalid option name\n");
		return 1;
	}

	auto section = args[0].substr(0, dot);
	auto variable = args[0].substr(dot + 1);

	LightBTS::Instance bts(data_dir);

	if (args.size() > 1) {
		auto value = args[1];
		for (size_t i = 2; i < args.size(); i++) {
			value.push_back(' ');
			value.append(args[i]);
		}

		bts.set_config(section, variable, value);
		bts.save_config();
		return 0;
	}

	print("{}\n", bts.get_config(section, variable));
	return 0;
}

static const struct option long_options[] = {
	{"help", no_argument, nullptr, 'h'},
	{"verbose", no_argument, nullptr, 'v'},
	{"batch", no_argument, nullptr, 2},
	{"no-email", no_argument, nullptr, 3},
	{"no-hooks", no_argument, nullptr, 4},
	{"data-dir", required_argument, nullptr, 'd'},
	{"version", no_argument, nullptr, 'V'},
	{"tag", no_argument, nullptr, 'T'},
	{"severity", no_argument, nullptr, 'S'},
	{"attach", no_argument, nullptr, 'A'},
};

struct cli_function {
	const char *name;
	int (*function)(const char *, const vector<string> &);
	friend bool operator<(const struct cli_function &a, const char *b) {
		return strcmp(a.name, b) < 0;
	}
};

static void show_help(ostream &out, const char *argv0) {
	print(out,
			"Usage: {} [options] command [arguments]\n"
			"\n"
			"Options:\n"
			"  -h, --help      Show a help message.\n"
			"  --batch         No interactive input.\n"
			"  --no-email      Do not send email messages.\n"
			"  --no-hooks      Do not call hooks.\n"
			"  --data-dir=DIR  Directory where LightBTS stores its data.\n"
			"\n"
			"Commands:\n"
			"  help        Show a help message.\n"
			"  init        Initialize a LightBTS instance.\n"
			"  config      Get/set a configuration option.\n"
			"  list        List bugs.\n"
			"  show        Show bug or message details.\n"
			"  search      Search bugs.\n"
			"  create      Create a new bug.\n"
			"  reply       Reply to an existing bug.\n"
			"  close       Close a ticket.\n"
			"  reopen      Reopen a ticket.\n"
			"  attach      Attach files to a ticket.\n"
			"  save        Save attachments to disk.\n"
			"  retitle     Change the title of a ticket.\n"
			"  found       Record versions where a problem is found.\n"
			"  notfound    Remove records of where a problem is found.\n"
			"  fixed       Record versions where a problem is fixed.\n"
			"  notfixed    Remove records of where a problem is fixed.\n"
			"  severity    Change the severity of a ticket.\n"
			"  link        Add a link between two tickets.\n"
			"  unlink      Remove a link between two tickets.\n"
			"  tags        Add or remove tags.\n"
			"  owner       Change the owner of a ticket.\n"
			"  noowner     Remove ownership of a ticket.\n"
			"  spam        Mark a message as spam.\n"
			"  nospam      Mark a message as not being spam.\n"
			"  progress    Change the progress level of a ticket.\n"
			"  milestone   Change the milestone of a ticket.\n"
			"  deadline    Change the deadline of a ticket.\n"
			"  index       Update the index for a given message file.\n"
			"  fsck        Perform an integrity check.\n"
			, argv0);
}

static int do_help(const char *argv0, const vector<string> &args) {
	if (args.empty()) {
		show_help(cout, argv0);
		return 0;
	}


	for (auto c: args[0]) {
		if (!isalnum(c) && c != '-') {
			print(cerr, "{0}: invalid command '{1}'\n", argv0, args[0]);
			show_help(cerr, argv0);
			return 1;
		}
	}

	string command = "/usr/bin/man lbts-" + args[0];
	return system(command.c_str());
}

// Keep the following list sorted at all times.
static const cli_function functions[] = {
	{"close", do_close},
	{"config", do_config},
	{"create", do_create},
	{"deadline", do_deadline},
	{"fixed", do_fixed},
	{"found", do_found},
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
	{"reopen", do_reopen},
	{"reply", do_reply},
	{"retitle", do_retitle},
	{"severity", do_severity},
	{"show", do_show},
	{"subject", do_retitle},
	{"tag", do_tags},
	{"tags", do_tags},
	{"title", do_retitle},
	{"unlink", do_unlink},
	{"version", do_version},
};

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		show_help(cerr, argv[0]);
		return 1;
	}

	if (!strcmp(argv[1], "--version")) {
		show_version();
		return 0;
	}

	string command;
	vector<string> args;

	int r;
	while((r = getopt_long(argc, argv, "-hvd:m:V:T:S:A:", long_options, nullptr)) != EOF) {
		switch (r) {
		case 'h':
			help = true;
			break;

		case 'v':
			verbose = true;
			break;

		case 2:
			batch = true;
			break;

		case 3:
			no_email = true;
			break;

		case 4:
			no_hooks = true;
			break;

		case 'd':
			data_dir = optarg;
			break;

		case 'm':
			cl_message = optarg;
			batch = true;
			break;

		case 'V':
			versions.push_back(optarg);
			break;

		case 'T':
			tags.push_back(optarg);
			break;

		case 'S':
			severity = optarg;
			break;

		case 'A':
			attachments.push_back(optarg);
			break;

		case '?':
			print(cerr, "Try '{0} --help' for more information.\n", argv[0]);
			return 1;

		default:
			if (command.empty()) {
				command = optarg;
			} else {
				args.push_back(optarg);
			}
			break;
		}
	}

	if (command.empty()) {
		if (help) {
			show_help(cout, argv[0]);
			return 0;
		} else {
			print(cerr, "{0}: missing command\nTry '{0} --help' for more information.\n", argv[0]);
			return 1;
		}
	}

	if (help) {
		args.insert(args.begin(), command);
		command = "help";
	}

	if (!isatty(0) || !isatty(1))
		batch = true;

	auto match = lower_bound(begin(functions), end(functions), command.c_str());
	if (match != end(functions) && command == match->name) {
		return match->function(argv[0], args);
	} else {
		print(cerr, "{0}: unrecognized command '{1}'\nTry '{0} --help' for more information.\n", argv[0], command);
		return 1;
	}
}

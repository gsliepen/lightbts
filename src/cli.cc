#include <getopt.h>
#include <vector>
#include <map>

#include <fmt/format.h>

using namespace std;

#ifdef VERSION
static const string lightbts_version = VERSION;
#else
static const string lightbts_version = "unknown";
#endif

bool help;
bool verbose;
bool no_hooks;
bool no_email;
bool batch;

string severity;
string data_dir;

vector<string> tags;
vector<string> versions;
vector<string> attachments;

static void show_version() {
	fmt::print(
			"LightBTS version {}\n"
			"Copyright © 2017 Guus Sliepen\n"
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

static int do_help(const char *argv0, const vector<string> &args) {
	fmt::print(
			"Usage: {} help [<command>]\n"
			"\n"
			"Shows a help message for the given <command>. If no <command> is given,\n"
			"shows a list of possible commands.\n",
			argv0
		  );
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

static const map<string, int (*)(const char *, const vector<string> &)> functions = {
	{"help", do_help},
	{"version", do_version},
};

static void show_help(FILE *out, const char *argv0) {
	fmt::print(out,
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

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		show_help(stderr, argv[0]);
		return 1;
	}
	
	if (!strcmp(argv[1], "--version")) {
		show_version();
		return 0;
	}

	string command;
	vector<string> args;

	int r;
	while((r = getopt_long(argc, argv, "-hvd:V:T:S:A:", long_options, nullptr)) != EOF) {
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
			fmt::print(stderr, "Try '{0} --help' for more information.\n", argv[0]);
			return 1;

		default:
			if (command.empty()) {
				command = optarg;
				if (!help && command == "help") {
					help = true;
					command.clear();
				}
			} else {
				args.push_back(optarg);
			}
			break;
		}
	}
	
	if (command.empty()) {
		if (help) {
			show_help(stdout, argv[0]);
			return 0;
		} else {
			fmt::print(stderr, "{0}: missing command\nTry '{0} --help' for more information.\n", argv[0]);
			return 1;
		}
	}

	try {
		return functions.at(command)(argv[0], args);
	} catch (out_of_range &e) {
		fmt::print(stderr, "{0}: unrecognized command '{1}'\nTry '{0} --help' for more information.\n", argv[0], command);
		return 1;
	}
}

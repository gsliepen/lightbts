/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <optional>
#include <print>

#include "frontends.hpp"
#include "lightbts.hpp"
#include "version.hpp"

namespace {

enum LongOnlyOption {
	fastcgi_option = 256,
	version_option,
};

const struct option long_options[] = {
	{"help", no_argument, nullptr, 'h'},
	{"data-dir", required_argument, nullptr, 'd'},
	{"listen", required_argument, nullptr, 'l'},
	{"fastcgi", optional_argument, nullptr, fastcgi_option},
	{"version", no_argument, nullptr, version_option},
	{},
};

void show_help(std::ostream &out, std::string_view program_name) {
	std::print(out,
		"Usage: {} [options]\n"
		"\n"
		"Serves a read-only web interface for a LightBTS instance.\n"
		"\n"
		"Options:\n"
		"  -h, --help             Show this help message.\n"
		"  -d, --data-dir=DIR     Directory where LightBTS stores its data.\n"
		"  -l, --listen=HOST:PORT Serve HTTP on the given address (default 127.0.0.1:8080).\n"
		"  --fastcgi[=SOCKET]     Serve FastCGI on stdin, or on SOCKET (a path or :port).\n"
		"  --version              Show version information.\n"
		"\n"
		"When started by a web server via CGI or FastCGI, this is detected automatically.\n",
		program_name);
}

}

int main(int argc, char *argv[]) {
	std::filesystem::path data_dir;
	std::string listen = "127.0.0.1:8080";
	std::optional<std::string> fastcgi_socket;

	int option;
	while ((option = getopt_long(argc, argv, "hd:l:", long_options, nullptr)) != -1) {
		switch (option) {
		case 'h':
			show_help(std::cout, argv[0]);
			return 0;
		case 'd':
			data_dir = optarg;
			break;
		case 'l':
			listen = optarg;
			break;
		case fastcgi_option:
			fastcgi_socket = optarg ? optarg : "";
			break;
		case version_option:
			std::println("LightBTS web server version {}", lightbts_version);
			return 0;
		default:
			std::println(std::cerr, "Try '{} --help' for more information.", argv[0]);
			return 1;
		}
	}

	if (optind < argc) {
		std::println(std::cerr, "{}: unexpected argument '{}'", argv[0], argv[optind]);
		return 1;
	}

	try {
		LightBTS::Web::Handler handler(data_dir);

		if (fastcgi_socket)
			return LightBTS::Web::run_fastcgi(handler, *fastcgi_socket);
		if (LightBTS::Web::is_fastcgi())
			return LightBTS::Web::run_fastcgi(handler, {});
		if (std::getenv("GATEWAY_INTERFACE"))
			return LightBTS::Web::run_cgi(handler);

		// Fail early if there is no usable instance.
		LightBTS::Instance(data_dir, LightBTS::Instance::Mode::read_only);
		return LightBTS::Web::run_http(handler, listen);
	} catch (const std::exception &error) {
		std::println(std::cerr, "{}: error: {}", argv[0], error.what());
		return 1;
	}
}

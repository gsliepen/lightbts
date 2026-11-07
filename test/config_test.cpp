/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

#include "check.hpp"
#include "config.hpp"

namespace fs = std::filesystem;

int main() {
	auto dir = fs::temp_directory_path() / ("lightbts-config-test-" + std::to_string(getpid()));
	fs::create_directories(dir);
	auto path = dir / "config";

	{
		std::ofstream file(path);
		file << "# A comment\n"
		     << " [core]  \n"
		     << "index=other-index\n"
		     << "  project   =  My  project  \n"
		     << "flag = yes\n"
		     << "broken = maybe\n"
		     << "empty =\n"
		     << "[web]\n"
		     << "root = /bugs/\n";
	}

	LightBTS::Config config;
	config.load(path);

	CHECK(config.get("core", "index") == "other-index");
	CHECK(config.get("core", "project") == "My  project");
	CHECK(config.get("web", "root") == "/bugs/");
	CHECK(config.exists("core", "empty"));
	CHECK(config.get("core", "empty").empty());
	CHECK(!config.exists("core", "missing"));
	CHECK(config.get("core", "missing", "fallback") == "fallback");
	CHECK(!config.exists("core", "missing"));
	CHECK(config.get_bool("core", "flag", false));
	CHECK(!config.get_bool("core", "missing", false));
	CHECK(throws([&] { config.get_bool("core", "broken", false); }));

	config.set("new", "value", "42");
	config.save(path);
	CHECK(!fs::exists(dir / "config.new"));

	LightBTS::Config reloaded;
	reloaded.load(path);
	CHECK(reloaded.get("new", "value") == "42");
	CHECK(reloaded.get("core", "project") == "My  project");

	CHECK(throws([&] { LightBTS::Config().load(dir / "nonexistent"); }));

	fs::remove_all(dir);
	return check_result();
}

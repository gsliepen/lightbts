/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017 Guus Sliepen <guus@lightbts.info>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <unistd.h>
#include <boost/filesystem.hpp>
#include <fmt/ostream.h>

#include "lightbts.hpp"

using namespace std;
using namespace fmt;
namespace fs = boost::filesystem;

namespace LightBTS {

Instance::Instance(const string &path, Flags flags) {
	init(path);
}

Instance::~Instance() {
}

string Instance::get_config(const string &section, const string &variable) {
	return config.get(section, variable);
}

void Instance::set_config(const string &section, const string &variable, const string &value) {
	return config.set(section, variable, value);
}

void Instance::init_index(const fs::path &filename) {
	db.open(filename.string());
	db.execute("PRAGMA foreign_key = on");
	auto version = db.execute("PRAGMA user_version").get_int(0);
	auto appid_record = db.execute("PRAGMA application_id");
	int appid = 0;
	if (appid_record)
		appid = appid_record.get_int(0);

	if (appid || version)
		if (appid != 0x4c425453)
			throw runtime_error("Index is a SQLite database not created by LightBTS!");

	if (!appid)
		db.execute("PRAGMA application_id=0x4c425453");

	if (!version) {
		print(cerr, "Creating index...");

		auto tx = db.begin();
		db.execute("CREATE TABLE bugs (id INTEGER PRIMARY KEY AUTOINCREMENT, status INTEGER NOT NULL DEFAULT 1, severity INTEGER NOT NULL DEFAULT 2, title TEXT, owner TEXT, submitter TEXT, date INTEGER, deadline INTEGER, progress INTEGER NOT NULL DEFAULT 0, milestone TEXT)");
		db.execute("CREATE TABLE links (a INTEGER, b INTEGER, type INTEGER, PRIMARY KEY(a, b), FOREIGN KEY(a) REFERENCES bugs(id), FOREIGN KEY(b) REFERENCES bugs(id))");
		db.execute("CREATE INDEX links_a_index ON links (a)");
		db.execute("CREATE INDEX links_b_index ON links (b)");
		db.execute("CREATE TABLE messages (msgid PRIMARY KEY, bug INTEGER, spam INTEGER NOT NULL DEFAULT 0, date INTEGER, FOREIGN KEY(bug) REFERENCES bugs(id))");
		db.execute("CREATE TABLE recipients (bug INTEGER, address TEXT, PRIMARY KEY(bug, address), FOREIGN KEY(bug) REFERENCES bugs(id))");
		db.execute("CREATE INDEX recipients_bug_index ON recipients (bug)");
		db.execute("CREATE INDEX recipients_address_index ON recipients (address)");
		db.execute("CREATE TABLE tags (bug INTEGER, tag TEXT, PRIMARY KEY(bug, tag), FOREIGN KEY(bug) REFERENCES bugs(id))");
		db.execute("CREATE INDEX tags_bug_index ON tags (bug)");
		db.execute("CREATE INDEX tags_tag_index ON tags (tag)");
		db.execute("CREATE TABLE versions (bug INTEGER, version TEXT, status INTEGER NOT NULL DEFAULT 1, PRIMARY KEY(bug, version))");
		db.execute("CREATE INDEX versions_bug_index ON versions (bug)");
		db.execute("CREATE INDEX versions_version_index ON versions (version)");
		db.execute("PRAGMA user_version=4");

		version = 4;
	}

	if (version < 0 || version > 4)
		throw runtime_error(format("Unknown index version {}", version));

	if (version < 4) {
		print(cerr, "Old index, use the Python prototype of LightBTS to upgrade to version 4!");
		throw runtime_error(format("Unsupported index version {}", version));
	}
}

void Instance::init(const fs::path &start_dir) {
	fs::path dir = start_dir;

	if (dir.empty()) {
		char *env_dir = getenv("LIGHTBTS_DIR");
		if (env_dir)
			dir = env_dir;
	}

	if (dir.empty())
		dir = fs::current_path();

	while (true) {
		if (fs::exists(dir / ".lightbts" / "config"))
			break;

		auto parent = dir.parent_path();
		if (parent == dir)
			throw runtime_error("No LightBTS instance found");
		dir = parent;
	};

	base_dir = dir / ".lightbts";

	config.load(base_dir / "config");

	// Core configuration
	dbfile = fs::canonical(config.get("core", "index", "index"), base_dir).string();
	maildir = fs::canonical(config.get("core", "messages", "messages"), base_dir).string();
	hookdir = fs::canonical(config.get("core", "hooks", "hooks"), base_dir).string();
	project = config.get("core", "project");
	admin = config.get("core", "admin");
	respond_to_new = config.get_bool("core", "respond-to-new", true);
	respond_to_reply = config.get_bool("core", "respond-to-reply", true);

	// Email configuration
	emailaddress = config.get("email", "address");
	emailname = config.get("email", "name");
	emailtemplates = config.get("email", "templates");
	smtphost = config.get("email", "smtphost");

	// Web configuration
	webroot = config.get("web", "root");
	staticroot = config.get("web", "static-root");
	webtemplates = config.get("web", "templates");

	// Create directories if necessary
	boost::system::error_code ec;
	fs::create_directories(base_dir);
	fs::create_directories(maildir);
	fs::create_directories(hookdir, ec);

	init_index(dbfile);
}

vector<Ticket> Instance::list() {
	string cmd = "SELECT id, title, status, severity FROM bugs WHERE 1";

	// apply filters

	vector<Ticket> tickets;
	for(auto &&row: db.execute(cmd))
		tickets.emplace_back(row.get_string(0), row.get_string(1), static_cast<Status>(row.get_int(2)), static_cast<Severity>(row.get_int(3)));

	return tickets;
}

Ticket Instance::get_ticket(const string &id) {
	auto row = db.execute("SELECT id, title, status, severity FROM bugs WHERE id=?", id);
	return Ticket(row.get_string(0), row.get_string(1), static_cast<Status>(row.get_int(2)), static_cast<Severity>(row.get_int(3)));
}

set<string> Instance::get_tags(const Ticket &ticket) {
	set<string> tags;
	for (auto &&row: db.execute("SELECT tag FROM tags WHERE bug=?", ticket.id))
		tags.insert(row.get_string(0));
	return tags;
}

string Instance::get_milestone(const Ticket &ticket) {
	return db.execute("SELECT milestone FROM bugs WHERE id=?", ticket.id).get_string(0);
}

}

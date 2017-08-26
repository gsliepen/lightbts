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
#include <fmt/ostream.h>

#include "lightbts.hpp"
#include "sqlite3.hpp"

using namespace std;
using namespace fmt;

static const int current_version = 4;

void LightBTS::init_index(const string &path) {
	db.open(path);

	auto result = db.execute("PRAGMA application_id");
	if (result) {
		if (result.get_int(0) != 0x4c425453) {
			print(cerr, "Index is a SQLite database not created by LightBTS!\n");
			abort();
		}
	} else {
		db.execute("PRAGMA application_id=0x4c425453");
	}

	db.execute("PRAGMA foreign_key = on");
	auto version = db.execute("PRAGMA user_version").get_int(0);

	if (version == current_version)
		return;

	if (version < 0 || version > current_version) {
		print(cerr, "Unknown database version {}\n", version);
		abort();
	}

	if (!version) {
		// Fresh start, create everything.
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
		db.execute("CREATE TABLE versions (bug INTEGER, version TEXT, status INTEGER NOT NULL DEFAULT 1, PRIMARY KEY(bug, version))");
		db.execute("CREATE INDEX versions_bug_index ON versions (bug)");
		db.execute("CREATE INDEX versions_version_index ON versions (version)");
		db.execute("PRAGMA user_version=4");
		tx.commit();
		return;
	}

	// Versions before 4 are not supported.
	if (version < 4) {
		print(cerr, "Unsupported database version {}\n", version);
		abort();
	}
}

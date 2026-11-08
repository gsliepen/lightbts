/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <algorithm>
#include <filesystem>
#include <unistd.h>

#include "check.hpp"
#include "lightbts.hpp"

namespace fs = std::filesystem;
using namespace LightBTS;

static Message make_message(std::string_view subject, std::string_view message_id, std::string_view text) {
	Message message;
	message.set_crlf(false);
	message.set_header("From", "Test <test@example.org>");
	message.set_header("Subject", subject);
	message.set_header("Message-ID", message_id);
	message.set_plain(text);
	return message;
}

static bool contains(const std::vector<std::string> &problems, std::string_view text) {
	return std::ranges::any_of(problems, [&](const std::string &problem) { return problem.contains(text); });
}

/// Changes the index directly, bypassing LightBTS.
static void modify_index(const fs::path &dir, std::string_view sql) {
	SQLite3::database db((dir / ".lightbts" / "index").string());
	db.execute(sql);
}

int main() {
	auto dir = fs::temp_directory_path() / ("lightbts-fsck-test-" + std::to_string(getpid()));
	fs::create_directories(dir);

	{
		Instance bts(dir, Instance::Mode::create);
		bts.import(make_message("First", "<1@test>", "Tags: foo\n\nThe first bug.\n"));
		bts.import(make_message("Second", "<2@test>", "Blocks: 1\n\nThe second bug.\n"));
		CHECK(bts.fsck().ok());
	}

	// Changed columns are shown by name
	modify_index(dir, "UPDATE fields SET value = 'closed' WHERE bug = 1 AND name = 'status'");
	modify_index(dir, "UPDATE bugs SET title = 'Changed' WHERE id = 1");
	{
		Instance bts(dir);
		auto result = bts.fsck();
		CHECK(!result.ok());
		CHECK(result.database_problems.empty());
		CHECK(contains(result.index_problems, "Ticket 1: status is closed in the index, but open according to the messages"));
		CHECK(contains(result.index_problems, "Ticket 1: title is \"Changed\" in the index, but \"First\" according to the messages"));

		// fsck changes nothing
		CHECK(bts.get_ticket(1).title == "Changed");
	}

	// Rows that are missing or extra
	modify_index(dir, "DELETE FROM fields WHERE name = 'tags'");
	modify_index(dir, "INSERT INTO fields (bug, name, value) VALUES (2, 'tags', 'bogus')");
	modify_index(dir, "INSERT INTO fields (bug, name, value) VALUES (2, 'severity', 'minor')");
	{
		Instance bts(dir);
		auto result = bts.fsck();
		CHECK(contains(result.index_problems, "Ticket 1: tags \"foo\" is missing from the index"));
		CHECK(contains(result.index_problems, "Ticket 2: tags \"bogus\" is in the index, but the messages do not produce it"));
		CHECK(contains(result.index_problems, "Ticket 2: severity is minor normal in the index, but normal according to the messages"));
	}

	// Indexes built with other field definitions are reported, but not rebuilt
	modify_index(dir, "UPDATE metadata SET value = 'other' WHERE name = 'fields'");
	{
		Instance bts(dir, Instance::Mode::open, {.update_index = false});
		CHECK(bts.is_index_outdated());
		CHECK(contains(bts.fsck().index_problems, "The index was built with other field definitions"));
	}

	// Unless they are opened normally
	{
		Instance bts(dir);
		CHECK(!bts.is_index_outdated());
		CHECK(bts.fsck().ok());
		modify_index(dir, "UPDATE bugs SET title = 'Changed' WHERE id = 1");
	}

	// Foreign key violations are found by SQLite's own checks
	modify_index(dir, "INSERT INTO links (a, b, type) VALUES (2, 99, 0)");
	{
		Instance bts(dir);
		auto result = bts.fsck();
		CHECK(contains(result.database_problems, "table links refers to a missing row in table bugs"));
	}

	// Reindexing fixes everything
	{
		Instance bts(dir);
		bts.reindex();
		CHECK(bts.fsck().ok());
		CHECK(bts.get_ticket(1).title == "First");
	}

	// An index of version 4 is upgraded, keeping the ticket numbers
	fs::remove(dir / ".lightbts" / "index");
	{
		SQLite3::database db((dir / ".lightbts" / "index").string());
		db.execute("CREATE TABLE bugs (id INTEGER PRIMARY KEY AUTOINCREMENT, status INTEGER NOT NULL DEFAULT 1, severity INTEGER NOT NULL DEFAULT 2, title TEXT, owner TEXT, submitter TEXT, date INTEGER, deadline INTEGER, progress INTEGER NOT NULL DEFAULT 0, milestone TEXT)");
		db.execute("CREATE TABLE links (a INTEGER, b INTEGER, type INTEGER, PRIMARY KEY(a, b), FOREIGN KEY(a) REFERENCES bugs(id), FOREIGN KEY(b) REFERENCES bugs(id))");
		db.execute("CREATE TABLE messages (msgid PRIMARY KEY, bug INTEGER, spam INTEGER NOT NULL DEFAULT 0, date INTEGER, FOREIGN KEY(bug) REFERENCES bugs(id))");
		db.execute("CREATE TABLE recipients (bug INTEGER, address TEXT, PRIMARY KEY(bug, address), FOREIGN KEY(bug) REFERENCES bugs(id))");
		db.execute("CREATE TABLE tags (bug INTEGER, tag TEXT, PRIMARY KEY(bug, tag), FOREIGN KEY(bug) REFERENCES bugs(id))");
		db.execute("CREATE TABLE versions (bug INTEGER, version TEXT, status INTEGER NOT NULL DEFAULT 1, PRIMARY KEY(bug, version))");
		db.execute("INSERT INTO bugs (id, title) VALUES (7, 'First'), (8, 'Second')");
		db.execute("INSERT INTO messages (msgid, bug, date) VALUES ('1@test', 7, 1), ('2@test', 8, 2)");
		db.execute("INSERT INTO tags (bug, tag) VALUES (7, 'foo')");
		db.execute("PRAGMA application_id = 1279415379");
		db.execute("PRAGMA user_version = 4");
	}
	CHECK(throws([&] { Instance(dir, Instance::Mode::read_only); }));
	CHECK(throws([&] { Instance(dir, Instance::Mode::open, {.update_index = false}); }));
	{
		Instance bts(dir);
		CHECK(bts.get_ticket(7).title == "First");
		CHECK(bts.get_ticket(7).get("tags") == "foo");
		CHECK(bts.get_ticket(8).get("status") == "open");
		CHECK(bts.list().size() == 2);
		CHECK(bts.fsck().ok());
	}
	CHECK(SQLite3::database((dir / ".lightbts" / "index").string()).execute("PRAGMA user_version").get_int(0) == 5);
	CHECK(!SQLite3::database((dir / ".lightbts" / "index").string()).execute("SELECT 1 FROM sqlite_master WHERE name = 'tags'"));

	fs::remove_all(dir);
	return check_result();
}

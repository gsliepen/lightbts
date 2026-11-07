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
	modify_index(dir, "UPDATE bugs SET status = 0, title = 'Changed' WHERE id = 1");
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
	modify_index(dir, "DELETE FROM tags");
	modify_index(dir, "INSERT INTO tags (bug, tag) VALUES (2, 'bogus')");
	{
		Instance bts(dir);
		auto result = bts.fsck();
		CHECK(contains(result.index_problems, "The row tags (bug = 1, tag = \"foo\") is missing from the index"));
		CHECK(contains(result.index_problems, "The row tags (bug = 2, tag = \"bogus\") is in the index, but the messages do not produce it"));
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

	fs::remove_all(dir);
	return check_result();
}

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <type_traits>

#include "check.hpp"
#include "sqlite3.hpp"

static_assert(!std::is_copy_constructible_v<SQLite3::database>);
static_assert(!std::is_copy_constructible_v<SQLite3::statement>);
static_assert(!std::is_convertible_v<SQLite3::result, int>);

int main() {
	SQLite3::database db(":memory:");
	db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");

	{
		auto transaction = db.begin();
		db.execute("INSERT INTO t (name) VALUES (?)", "a");
		db.execute("INSERT INTO t (name) VALUES (?)", std::string("b"));
		db.execute("INSERT INTO t (name) VALUES (?)", nullptr);
		transaction.commit();
	}

	{
		auto transaction = db.begin();
		db.execute("INSERT INTO t (name) VALUES ('rolled back')");
	}

	CHECK(db.execute("SELECT count(*) FROM t").get_int(0) == 3);

	int rows = 0;
	for (auto &&row: db.execute("SELECT id, name FROM t ORDER BY id"))
		rows += row.get_int64(0) > 0;
	CHECK(rows == 3);

	auto missing = db.execute("SELECT name FROM t WHERE id = ?", 42);
	CHECK(!missing);
	CHECK(throws([&] { missing.get_string(0); }));
	CHECK(missing.begin() == missing.end());

	try {
		db.execute("INSERT INTO t (id, name) VALUES (1, 'duplicate')");
		CHECK(false);
	} catch (const SQLite3::error &error) {
		CHECK(error.code == SQLITE_CONSTRAINT_PRIMARYKEY);
	}

	auto statement = db.prepare("SELECT name FROM t WHERE id = ?");
	statement.bind(std::int64_t{2});
	CHECK(statement.step() == SQLITE_ROW);
	CHECK(statement.column_string(0) == "b");
	statement.reset();
	statement.bind(3);
	CHECK(statement.step() == SQLITE_ROW);
	CHECK(statement.column_c_str(0) == nullptr);

	auto moved = std::move(statement);
	CHECK(moved.has_row());

	CHECK(throws([&] { db.prepare("SELECT 1; SELECT 2"); }));
	CHECK(throws([&] { db.execute("SELECT * FROM nonexistent"); }));
	CHECK(throws([] { SQLite3::database bad("/nonexistent/directory/database"); }));

	return check_result();
}

#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <sqlite3.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace SQLite3 {
	struct error: std::runtime_error {
		int code;
		error(::sqlite3 *db): std::runtime_error(sqlite3_errmsg(db)), code(sqlite3_extended_errcode(db)) {}
		error(int result): std::runtime_error(sqlite3_errstr(result)), code(result) {}
		error(const std::string &what): std::runtime_error(what), code(SQLITE_ERROR) {}
	};

	inline void check(int result) {
		if (result != SQLITE_OK)
			throw error(result);
	}

	inline void check(::sqlite3 *db, int result) {
		if (result != SQLITE_OK)
			throw error(db);
	}

	/* An sqlite3_stmt handle is kind of an overloaded thing in SQLite3.
	 * It acts as a prepared statement, a filled in statement,
	 * and a row of results.
	 */
	class statement {
		::sqlite3_stmt *stmt = nullptr;
		int parameter = 0;
		int state = SQLITE_OK;

		void check_row() const {
			if (state != SQLITE_ROW)
				throw error("no row available");
		}

		public:
		/* Constructors */
		statement() = default;
		statement(const statement &other) = delete;
		statement &operator=(const statement &other) = delete;

		statement(statement &&other) noexcept:
			stmt(std::exchange(other.stmt, nullptr)),
			parameter(other.parameter),
			state(other.state) {}

		statement &operator=(statement &&other) noexcept {
			std::swap(stmt, other.stmt);
			std::swap(parameter, other.parameter);
			std::swap(state, other.state);
			return *this;
		}

		statement(::sqlite3 *db, std::string_view sql) {
			const char *tail;
			check(db, sqlite3_prepare_v2(db, sql.data(), sql.size(), &stmt, &tail));

			if (tail != sql.data() + sql.size()) {
				sqlite3_finalize(stmt);
				throw error("statement was not fully processed");
			}
		}

		/* Destructor */
		~statement() { sqlite3_finalize(stmt); }

		/* Binding arguments */
		statement &bind(const char *arg) { return arg ? bind(std::string_view(arg)) : bind(nullptr); }
		statement &bind(std::string_view arg) { check(sqlite3_bind_text64(stmt, ++parameter, arg.data(), arg.size(), SQLITE_TRANSIENT, SQLITE_UTF8)); return *this; }
		statement &bind(int arg) { check(sqlite3_bind_int(stmt, ++parameter, arg)); return *this; }
		statement &bind(std::int64_t arg) { check(sqlite3_bind_int64(stmt, ++parameter, arg)); return *this; }
		statement &bind(double arg) { check(sqlite3_bind_double(stmt, ++parameter, arg)); return *this; }
		statement &bind(std::nullptr_t) { check(sqlite3_bind_null(stmt, ++parameter)); return *this; }

		template<typename... Ts>
		statement &bind_all(const Ts &...args) { (bind(args), ...); return *this; }

		/* Step and reset */
		int step() {
			state = sqlite3_step(stmt);
			if (state != SQLITE_ROW && state != SQLITE_DONE)
				throw error(sqlite3_db_handle(stmt));
			return state;
		}

		void reset() { check(sqlite3_reset(stmt)); parameter = 0; state = SQLITE_OK; }

		bool has_row() const { return state == SQLITE_ROW; }

		/* Get column values */
		int column_int(int col) const { check_row(); return sqlite3_column_int(stmt, col); }
		std::int64_t column_int64(int col) const { check_row(); return sqlite3_column_int64(stmt, col); }
		const char *column_c_str(int col) const { check_row(); return reinterpret_cast<const char *>(sqlite3_column_text(stmt, col)); }
		std::string column_string(int col) const { auto str = column_c_str(col); return str ? str : ""; }
		double column_double(int col) const { check_row(); return sqlite3_column_double(stmt, col); }
		int column_type(int col) const { check_row(); return sqlite3_column_type(stmt, col); }
		std::string column_name(int col) const { return sqlite3_column_name(stmt, col); }
		int column_count() const { return sqlite3_column_count(stmt); }
	};

	class row {
		const statement *stmt;

		public:
		row(const statement *stmt): stmt(stmt) {}

		int get_int(int col) const { return stmt->column_int(col); }
		std::int64_t get_int64(int col) const { return stmt->column_int64(col); }
		const char *get_c_str(int col) const { return stmt->column_c_str(col); }
		std::string get_string(int col) const { return stmt->column_string(col); }
		double get_double(int col) const { return stmt->column_double(col); }
		int get_type(int col) const { return stmt->column_type(col); }
		std::string get_name(int col) const { return stmt->column_name(col); }
		int count() const { return stmt->column_count(); }
	};

	class row_iterator {
		statement *stmt = nullptr;

		public:
		row_iterator() = default;
		row_iterator(statement &stmt): stmt(&stmt) {}

		row_iterator &operator++() { if (stmt->step() != SQLITE_ROW) stmt = nullptr; return *this; }
		bool operator==(const row_iterator &other) const { return stmt == other.stmt; }
		bool operator!=(const row_iterator &other) const { return stmt != other.stmt; }
		row operator*() const { return row(stmt); }
	};

	/* This class is basically a statement AFTER it has executed. */
	class result {
		statement stmt;

		public:
		template<typename... Ts>
		result(::sqlite3 *db, std::string_view sql, const Ts &...args): stmt(db, sql) {
			stmt.bind_all(args...);
			stmt.step();
		}

		explicit operator bool() const { return stmt.has_row(); }

		row_iterator begin() { return stmt.has_row() ? row_iterator(stmt) : row_iterator(); }
		row_iterator end() { return row_iterator(); }

		/* Get current column values */
		bool next() { return stmt.step() == SQLITE_ROW; }
		int get_int(int col) const { return stmt.column_int(col); }
		std::int64_t get_int64(int col) const { return stmt.column_int64(col); }
		const char *get_c_str(int col) const { return stmt.column_c_str(col); }
		std::string get_string(int col) const { return stmt.column_string(col); }
		double get_double(int col) const { return stmt.column_double(col); }
		int get_type(int col) const { return stmt.column_type(col); }
		std::string get_name(int col) const { return stmt.column_name(col); }
		int column_count() const { return stmt.column_count(); }
	};

	/* Rolls back automatically unless commit() succeeded. */
	class transaction {
		::sqlite3 *db;
		bool finished = false;

		public:
		transaction(const transaction &other) = delete;
		transaction &operator=(const transaction &other) = delete;

		/// \param immediate Whether to lock the database for writing right away, instead of at the first write.
		transaction(::sqlite3 *db, bool immediate = false): db(db) {
			check(db, sqlite3_exec(db, immediate ? "BEGIN IMMEDIATE" : "BEGIN", nullptr, nullptr, nullptr));
		}

		~transaction() { abort(); }

		void commit() {
			if (finished)
				throw error("Trying to commit to an already finished transaction");
			check(db, sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr));
			finished = true;
		}

		void abort() noexcept {
			if (!finished) {
				sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
				finished = true;
			}
		}
	};

	class database {
		::sqlite3 *db = nullptr;

		public:
		/// How long to wait for another process that holds a lock on the database.
		static constexpr int busy_timeout_milliseconds = 5000;

		database() = default;
		database(const database &other) = delete;
		database &operator=(const database &other) = delete;

		database(const std::string &filename, bool read_only = false) {
			open(filename, read_only);
		}

		~database() { close(); }

		void open(const std::string &filename, bool read_only = false) {
			close();
			int flags = read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			if (sqlite3_open_v2(filename.c_str(), &db, flags, nullptr) != SQLITE_OK) {
				error failure(db ? error(db) : error(SQLITE_NOMEM));
				close();
				throw failure;
			}
			sqlite3_busy_timeout(db, busy_timeout_milliseconds);
		}

		void close() {
			sqlite3_close_v2(db);
			db = nullptr;
		}

		statement prepare(std::string_view sql) {
			return statement(db, sql);
		}

		template<typename... Ts>
		result execute(std::string_view sql, const Ts &...args) {
			return result(db, sql, args...);
		}

		transaction begin(bool immediate = false) {
			return transaction(db, immediate);
		}

		std::int64_t last_insert_rowid() const { return sqlite3_last_insert_rowid(db); }
		int changes() const { return sqlite3_changes(db); }
		int total_changes() const { return sqlite3_total_changes(db); }
	};
}

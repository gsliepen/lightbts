#pragma once

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

#include <sqlite3.h>
#include <stdexcept>
#include <utility>

namespace SQLite3 {
	struct error: std::runtime_error {
		error(::sqlite3 *db): std::runtime_error(sqlite3_errmsg(db)) {}
		error(int result): std::runtime_error(sqlite3_errstr(result)) {}
		error(const std::string &what): std::runtime_error(what) {}
	};

	void check(int result) {
		if (result)
			throw error(result);
	}

	void check(::sqlite3 *db, int result) {
		if (result)
			throw error(db);
	}

	/* An sqlite3_stmt handle is kind of an overloaded thing in SQLite3.
	 * It acts as a prepared statement, a filled in statement,
	 * and a row of results.
	 */
	class statement {
		friend class result;
		::sqlite3_stmt *stmt;
		int p;
		int state;

		public:
		/* Constructors */
		statement(const statement &other) = delete;

		statement(statement &&other) {
			stmt = other.stmt;
			p = other.p;
			other.stmt = nullptr;
		}

		statement(): stmt(nullptr) {}

		statement(::sqlite3 *db, const std::string &sql): p(0) {
			const char *str = sql.c_str();
			size_t size = sql.size();
			const char *tail;

			check(db, sqlite3_prepare(db, str, size + 1, &stmt, &tail));

			if (tail != str + size)
				throw error("statement was not fully processed");
		}

		/* Destructor */
		~statement() { sqlite3_finalize(stmt); }

		/* Binding arguments */
		statement &bind(const char *arg) { if (arg) check(sqlite3_bind_text(stmt, ++p, arg, -1, nullptr)); else check(sqlite3_bind_null(stmt, ++p)); return *this;  }
		statement &bind(const std::string &arg) { check(sqlite3_bind_text(stmt, ++p, arg.data(), arg.size(), nullptr)); return *this;  }
		statement &bind(int arg) { check(sqlite3_bind_int(stmt, ++p, arg)); return *this;  }
		statement &bind(int64_t arg) { check(sqlite3_bind_int64(stmt, ++p, arg)); return *this;  }
		statement &bind(double arg) { check(sqlite3_bind_double(stmt, ++p, arg)); return *this;  }
		statement &bind(std::nullptr_t arg) { check(sqlite3_bind_null(stmt, ++p)); return *this;  }

		template<typename T, typename... Ts>
		statement &bind(T arg, Ts... rest) { bind(arg); bind(rest...); return *this;  }
		statement &bind() { return *this; }

		/* Step and reset */
		int step() { return state = sqlite3_step(stmt); }
		void reset() { check(sqlite3_reset(stmt)); state = 0; }

		/* Get column values */
		int column_int(int col) { return sqlite3_column_int(stmt, col); }
		int64_t column_int64(int col) { return sqlite3_column_int64(stmt, col); }
		const char *column_c_str(int col) { return (const char *)sqlite3_column_text(stmt, col); }
		std::string column_string(int col) { return (const char *)sqlite3_column_text(stmt, col); }
		double column_double(int col) { return sqlite3_column_double(stmt, col); }
		int column_type(int col) { return sqlite3_column_type(stmt, col); }
		std::string column_name(int col) { return sqlite3_column_name(stmt, col); }
		int column_count() { return sqlite3_column_count(stmt); }
	};

	class row {
		statement *stmt;

		public:
		row(statement *stmt): stmt(stmt) {}

		int get_int(int col) { return stmt->column_int(col); }
		int64_t get_int64(int col) { return stmt->column_int64(col); }
		const char *get_c_str(int col) { return stmt->column_c_str(col); }
		std::string get_string(int col) { return stmt->column_string(col); }
		double get_double(int col) { return stmt->column_double(col); }
		int get_type(int col) { return stmt->column_type(col); }
		std::string get_name(int col) { return stmt->column_name(col); }
		int count() { return stmt->column_count(); }
	};

	class row_iterator {
		statement *stmt;

		public:
		row_iterator(): stmt(nullptr) {}
		row_iterator(statement &stmt): stmt(&stmt) {}

		row_iterator &operator++() { if (stmt->step() != SQLITE_ROW) stmt = nullptr; return *this; }
		bool operator!=(row_iterator &other) { return stmt != other.stmt; }
		row operator*() { return row(stmt); }
	};

	/* This class is basically a statement AFTER it has executed. */
	class result {
		statement stmt;

		public:
		template<typename... Ts>
		result(::sqlite3 *db, const std::string &sql, Ts... args): stmt(db, sql) {
			stmt.bind(args...);
			stmt.step();
		}

		result(result &other) = delete;
		result(result &&other): stmt(std::move(other.stmt)) {}

		operator bool() { return stmt.state == SQLITE_ROW; }

		row_iterator begin() { if (stmt.state == SQLITE_ROW) return row_iterator(stmt); else return row_iterator(); }
		row_iterator end() { return row_iterator(); }

		/* Get current column values */
		int next() { return stmt.step() == SQLITE_ROW; }
		int column_int(int col) { return sqlite3_column_int(stmt, col); }
		int64_t column_int64(int col) { return sqlite3_column_int64(stmt, col); }
		const char *column_c_str(int col) { return (const char *)sqlite3_column_text(stmt, col); }
		std::string column_string(int col) { return (const char *)sqlite3_column_text(stmt, col); }
		double column_double(int col) { return sqlite3_column_double(stmt, col); }
		int column_type(int col) { return sqlite3_column_type(stmt, col); }
		std::string column_name(int col) { return sqlite3_column_name(stmt, col); }
		int column_count() { return sqlite3_column_count(stmt); }
	};

	class transaction {
		::sqlite3 *db;
		bool finished;

		public:
		transaction(::sqlite3 *db): db(db), finished(false) {
			statement(db, "BEGIN").step();
		}
		~transaction() {
			if (!finished)
				abort();
		}

		bool commit() {
			if (finished)
				throw error("Trying to commit to an already finished transaction");
			if (statement(db, "COMMIT").step() == SQLITE_DONE)
				finished = true;
			return finished;
		}

		void abort() {
			if (!finished) {
				statement(db, "ROLLBACK").step();
				finished = true;
			}
		}
	};

	class database {
		::sqlite3 *db;

		public:
		database(): db(nullptr) {}

		void open(const std::string &filename) {
			if (sqlite3_open(filename.c_str(), &db))
				throw error("could not open database");
		}

		database(const std::string &filename) {
			open(filename);
		}

		void close() {
			sqlite3_close(db);
			db = nullptr;
		}

		~database() {
			close();
		}

		statement prepare(const std::string &sql) {
			return std::move(statement(db, sql));
		}

		template<typename... Ts>
		result execute(const std::string &sql, Ts... args) {
			return result(db, sql, args...);
		}

		transaction begin() {
			return transaction(db);
		}
	};

	database open(const std::string &filename) {
		return database(filename);
	}
}

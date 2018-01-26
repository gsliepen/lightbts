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
#include <blake2.h>
#include <cstdio>

#include "lightbts.hpp"
#include "templates.inl"

using namespace std;
using namespace fmt;
namespace fs = boost::filesystem;

namespace LightBTS {

bool is_valid_status(const string &name) {
	for (auto &&status: status_names)
		if (status == name)
			return true;
	return false;
}

bool is_valid_severity(const string &name) {
	for (auto &&severity: severity_names)
		if (severity == name)
			return true;
	return false;
}

int status_index(const string &name) {
	int i = 0;
	for (auto &&status: status_names)
		if (status == name)
			return i;
		else
			i++;
	throw runtime_error("Invalid status name");
}

int severity_index(const string &name) {
	int i = 0;
	for (auto &&severity: severity_names)
		if (severity == name)
			return i;
		else
			i++;
	throw runtime_error("Invalid severity name");
}

Instance::Instance(const string &path, Flags flags) {
	init(path, flags == Flags::INIT);
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
		print(cerr, "Creating index...\n");

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

void Instance::init(const fs::path &start_dir, bool create) {
	fs::path dir = start_dir;

	if (dir.empty()) {
		char *env_dir = getenv("LIGHTBTS_DIR");
		if (env_dir)
			dir = env_dir;
	}

	if (dir.empty())
		dir = fs::current_path();

	if (create) {
		if (fs::exists(dir / ".lightbts" / "config"))
			throw runtime_error("LightBTS instance already exists");
	} else {
		while (true) {
			if (fs::exists(dir / ".lightbts" / "config"))
				break;

			auto parent = dir.parent_path();
			if (parent == dir)
				throw runtime_error("No LightBTS instance found");
			dir = parent;
		};
	}

	base_dir = dir / ".lightbts";

	config.load(base_dir / "config");

	// Core configuration
	dbfile = fs::absolute(config.get("core", "index", "index"), base_dir);
	maildir = fs::absolute(config.get("core", "messages", "messages"), base_dir);
	hookdir = fs::absolute(config.get("core", "hooks", "hooks"), base_dir);
	templatedir = fs::absolute(config.get("core", "templates", "templates"), base_dir);
	project = config.get("core", "project");
	admin = config.get("core", "admin");
	respond_to_new = config.get_bool("core", "respond-to-new", true);
	respond_to_reply = config.get_bool("core", "respond-to-reply", true);

	// Email configuration
	emailaddress = config.get("email", "address");
	emailname = config.get("email", "name");
	smtphost = config.get("email", "smtphost");

	// Web configuration
	webroot = config.get("web", "root");
	staticroot = config.get("web", "static-root");

	// Create directories if necessary
	fs::create_directories(base_dir);
	fs::create_directories(maildir);
	if (create) {
		fs::create_directories(hookdir);
		fs::create_directories(templatedir);
	}

	// Install templates
	if (create) {
		for (auto &&tmpl: templates) {
			auto path = templatedir / tmpl.filename;
			if (!fs::exists(path))
				ofstream(path.string()) << tmpl.data;
		}
	}

	// Initialize the index
	init_index(dbfile);
}

vector<Ticket> Instance::list(const vector<string> &args, size_t len) {
	string cmd = "SELECT id, title, status, severity FROM bugs WHERE 1";

	// apply filters
	int status = 1;
	vector<int> severities;
	vector<string> tags;

	for (size_t i = 0; i < len; i++) {
		if (args[i] == "all") {
			status = -1;
		} else if (args[i] == "closed") {
			status = 0;
		} else if (args[i] == "open") {
			status = 1;
		} else if (is_valid_severity(args[i])) {
			severities.push_back(severity_index(args[i]));
		} else {
			tags.push_back(args[i]);
		}
	}

	if (status != -1)
		cmd += " AND status=?";

	if (!severities.empty()) {
		cmd += " AND (0";
		for (auto &&severity: severities)
			cmd += " OR severity=?";
		cmd += ")";
	}

	if (!tags.empty()) {
		cmd += " AND (0";
		for (auto &&tag: tags)
			cmd += " OR EXISTS (SELECT 1 FROM tags WHERE bug=id AND tag=?)";
		cmd += ")";
	}

	auto stmt = db.prepare(cmd);
	if (status != -1)
		stmt.bind(status);
	for (auto &&severity: severities)
		stmt.bind(severity);
	for (auto &&tag: tags)
		stmt.bind(tag);

	vector<Ticket> tickets;
	while(stmt.step() == SQLITE_ROW)
		tickets.emplace_back(stmt.column_string(0), stmt.column_string(1), static_cast<Status>(stmt.column_int(2)), static_cast<Severity>(stmt.column_int(3)));

	return tickets;
}

Ticket Instance::get_ticket(const string &id) {
	auto row = db.execute("SELECT id, title, status, severity FROM bugs WHERE id=?", id);
	return Ticket(row.get_string(0), row.get_string(1), static_cast<Status>(row.get_int(2)), static_cast<Severity>(row.get_int(3)));
}

Ticket Instance::get_ticket_from_message_id(const string &id) {
	return get_ticket(db.execute("SELECT bug FROM messages WHERE msgid=?", id).get_string(0));
}

static string hash_msgid(const string &id) {
	if (id.size() < 3)
		throw runtime_error("Invalid Message-ID value");

	uint8_t id_hash[24];
	int ec;

	if (id[0] == '<' && id[id.size() - 1] == '>')
		ec = blake2b(id_hash, id.data() + 1, nullptr, sizeof id_hash, id.size() - 2, 0);
	else
		ec = blake2b(id_hash, id.data(), nullptr, sizeof id_hash, id.size(), 0);

	if (ec)
		throw runtime_error("Hash function failed");

	static const char hexdigits[] = "0123456789abcdef";
	string result(sizeof id_hash * 2, '\0');

	for (size_t i = 0; i < sizeof id_hash; i++) {
		result[i * 2] = hexdigits[id_hash[i] >> 4];
		result[i * 2 + 1] = hexdigits[id_hash[i] & 0xf];
	}

	return result;
}

Message Instance::get_message(const string &id) {
	string hash = hash_msgid(id);

	fs::path filename = fs::path(maildir) / hash.substr(0, 2) / hash.substr(2);

	Message message;
	message.load(filename.string());

	return message;
}

fs::path Instance::store(const Message &msg) {
	string hash = hash_msgid(msg["Message-ID"]);
	fs::path filename = fs::path(maildir) / hash.substr(0, 2) / hash.substr(2);
	msg.save(filename.string());
	return filename;
}

set<string> Instance::get_tags(const Ticket &ticket) {
	set<string> tags;
	for (auto &&row: db.execute("SELECT tag FROM tags WHERE bug=?", stol(ticket.id)))
		tags.insert(row.get_string(0));
	return tags;
}

string Instance::get_milestone(const Ticket &ticket) {
	return db.execute("SELECT milestone FROM bugs WHERE id=?", stol(ticket.id)).get_string(0);
}

vector<string> Instance::get_message_ids(const Ticket &ticket) {
	vector<string> result;

	for (auto &&row: db.execute("SELECT msgid FROM messages WHERE bug=?", stol(ticket.id)))
		result.push_back(row.get_string(0));

	return result;
}

bool Instance::run_hook(const string &name, const fs::path &path, const string &id) {
	if (no_hooks)
		return true;

	fs::path hook = hookdir / name;
	if (!access(hook.string().c_str(), X_OK))
		return true;

	// TODO:
	fs::current_path(base_dir);
	string cmd = format("LIGHTBTS_DIR=\"{}\" MESSAGE_FILE=\"{}\" BUG_ID=\"{}\" \"{}\"", base_dir, path, id, hook);
	FILE *fd = popen(cmd.c_str(), "w");
	if (!fd) {
		print(cerr, "Failed to execute {} hook: {}\n", name, strerror(errno));
		return false;
	}
	int result = pclose(fd);
	if (result) {
		print(cerr, "Error while executing {} hook, exit code {}\n", name, WEXITSTATUS(result));
		return false;
	}

	return true;
}

void Instance::parse_metadata(const string &id, const Message &msg) {
	// Metadata variables to extract
	string status;
	string severity;
	string title;
	vector<string> tags;
	string version;
	vector<string> found;
	vector<string> notfound;
	vector<string> fixed;
	vector<string> notfixed;
	string owner;
	string progress;
	string milestone;
	string deadline;

	// Warnings and error messages
	string log;

	// Parse email headers
	status = msg["X-LightBTS-Status"];

	string tag = msg["X-LightBTS-Tag"];
	if (!tag.empty())
		tags.push_back(tag);

	// Parse the body as if it is a MIME part.
	Mimesis::Part body;
	body.from_string(msg.get_text());

	auto set_and_check = [&log](const string &name, string &variable, const string &value) -> bool {
		if (!variable.empty() && value != variable)
			log += "Duplicate " + name + " field.\n";
		variable = value;
	};

	for (auto &&header: body.get_headers()) {
		auto &key = header.first;
		auto &value = header.second;

		if (key == "status") {
			set_and_check("status", status, value);
		} else if (key == "severity") {
			set_and_check("severity", severity, value);
		} else if (key == "tags" || key == "tag") {
			tags.push_back(value);
		} else if (key == "version") {
			set_and_check("version", version, value);
		} else if(key == "found") {
			found.push_back(value);
		} else if(key == "notfound") {
			notfound.push_back(value);
		} else if(key == "fixed") {
			fixed.push_back(value);
		} else if(key == "notfixed") {
			notfixed.push_back(value);
		} else if(key == "owner") {
			set_and_check("owner", owner, value);
		} else if(key == "progress") {
			set_and_check("progress", progress, value);
		} else if(key == "milestone") {
			set_and_check("milestone", milestone, value);
		} else if(key == "deadline") {
			set_and_check("deadline", deadline, value);
		} else if(key == "title" || key == "topic") {
			set_and_check("title", title, value);
		} else {
			log += "Unknown header field \"" + key + "\".\n";
		}
	}

	// Set any variables found
	if (!status.empty())
		db.execute("UPDATE bugs SET status=? WHERE id=?", status, id);
	if (!severity.empty())
		db.execute("UPDATE bugs SET severity=? WHERE id=?", severity, id);
	if (!owner.empty())
		db.execute("UPDATE bugs SET owner=? WHERE id=?", owner, id);
	if (!progress.empty())
		db.execute("UPDATE bugs SET progress=? WHERE id=?", progress, id);
	if (!milestone.empty())
		db.execute("UPDATE bugs SET milestone=? WHERE id=?", milestone, id);
	if (!deadline.empty())
		db.execute("UPDATE bugs SET deadline=? WHERE id=?", deadline, id);
	if (!title.empty())
		db.execute("UPDATE bugs SET title=? WHERE id=?", title, id);
}

bool Instance::import(const Message &in) {
	// Don't allow messages with the X-LightBTS-Control header set
	if (!in["X-LightBTS-Control"].empty())
		throw runtime_error("Denying import of message with X-LightBTS-Control header");

	Message msg = in;

	// Handle missing Message-ID
	if (msg["Message-ID"].empty())
		msg.generate_msgid("LightBTS");

	// Add a Received: header
	msg.add_received("by localhost (LightBTS)");

	// Handle missing Date
	if (msg["Date"].empty())
		msg.set_date();

	// Save message
	string msgid = msg["Message-ID"];
	string parent = msg["In-Reply-To"];
	string subject = msg["Subject"];
	fs::path filename = store(msg);

	// Run the pre-index hook
	if (!run_hook("pre-index", filename))
		return false;

	// Store the message in the database
	try {
		db.execute("INSERT INTO messages (msgid, bug) values (?,?)", msgid, 0);
	} catch (SQLite3::error &err) {
		// Ignore duplicates
		if (err.code == SQLITE_CONSTRAINT_UNIQUE) {
			print(cerr, "Ignoring duplicate message from {} with Message-ID {}\n", msg["From"], msgid);
			return false;
		} else {
			throw runtime_error("Database error");
		}
	}

	// Can we match the message to an existing bug?
	string id;
	bool is_new = false;

	if (!parent.empty()) {
		auto result = db.execute("SELECT bug FROM messages WHERE msgid=?", parent);
		if (result)
			id = result.get_string(0);
	}

	if (id.empty()) {
		auto result = db.execute("SELECT bug FROM messages WHERE title LIKE ?", "%" + parent);
		if (result)
			id = result.get_string(0);
	}

	if (id.empty()) {
		db.execute("INSERT INTO bugs (title) VALUES (?)", subject);
		id = to_string(db.last_insert_rowid());
		is_new = true;
	}

	db.execute("UPDATE messages SET bug=? WHERE msgid=?", id, msgid);
	db.execute("INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)", id, msg["From"]);

	// Handle metadata
	parse_metadata(id, msg);

	//Run the post-index hook
	run_hook("post-index", filename, id);
	return is_new;
}

}

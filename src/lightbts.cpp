/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "lightbts.hpp"

#include <blake2.h>
#include <charconv>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <pwd.h>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

#include "embedded.hpp"
#include "process.hpp"
#include "pseudoheaders.hpp"
#include "strings.hpp"
#include "templates.inl"

namespace LightBTS {

std::string unquote_message_id(std::string_view message_id) {
	message_id = trim(message_id);
	if (message_id.size() >= 2 && message_id.front() == '<' && message_id.back() == '>')
		message_id = message_id.substr(1, message_id.size() - 2);
	return std::string(message_id);
}

namespace {

constexpr int application_id = 0x4c425453;
constexpr int index_version = 4;

std::string hash_message_id(std::string_view message_id) {
	auto unquoted = unquote_message_id(message_id);
	if (unquoted.empty())
		throw std::runtime_error("Invalid Message-ID");

	std::array<std::uint8_t, 24> hash;
	if (blake2b(hash.data(), unquoted.data(), nullptr, hash.size(), unquoted.size(), 0))
		throw std::runtime_error("Hash function failed");

	std::string result;
	for (auto byte: hash)
		result += std::format("{:02x}", byte);
	return result;
}

std::int64_t current_unix_time() {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::optional<std::int64_t> parse_integer(std::string_view text) {
	std::int64_t value;
	auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc() || end != text.data() + text.size())
		return std::nullopt;
	return value;
}

bool is_valid_date(std::string_view text) {
	std::chrono::year_month_day date;
	std::istringstream in{std::string(text)};
	in >> std::chrono::parse("%F", date);
	return in && in.peek() == std::char_traits<char>::eof() && date.ok();
}

std::string read_file(const fs::path &path) {
	std::ifstream file(path, std::ios::binary);
	std::stringstream contents;
	contents << file.rdbuf();
	if (!file)
		throw std::runtime_error(std::format("Could not read {}", path.string()));
	return contents.str();
}

Ticket ticket_from_row(SQLite3::row row) {
	return {
		.id = row.get_int64(0),
		.title = row.get_string(1),
		.status = static_cast<Status>(row.get_int(2)),
		.severity = static_cast<Severity>(row.get_int(3)),
	};
}

std::string placeholders(std::size_t count) {
	std::string result;
	for (std::size_t i = 0; i < count; i++)
		result += i ? ", ?" : "?";
	return result;
}

}

Instance::Instance(const fs::path &start_dir, Mode mode, InstanceOptions options): no_hooks(options.no_hooks) {
	auto dir = start_dir;

	if (dir.empty())
		if (auto env_dir = std::getenv("LIGHTBTS_DIR"))
			dir = env_dir;

	if (dir.empty())
		dir = fs::current_path();

	dir = fs::absolute(dir).lexically_normal();
	if (!dir.has_filename())
		dir = dir.parent_path();

	if (mode == Mode::create) {
		if (fs::exists(dir / ".lightbts" / "config"))
			throw std::runtime_error("LightBTS instance already exists");
	} else {
		while (!fs::exists(dir / ".lightbts" / "config")) {
			auto parent = dir.parent_path();
			if (parent == dir)
				throw std::runtime_error("No LightBTS instance found");
			dir = parent;
		}
	}

	project_dir = dir;
	base_dir = dir / ".lightbts";

	if (mode == Mode::create)
		set_default_config();
	else
		config.load(base_dir / "config");

	// Relative paths are relative to the .lightbts directory, absolute paths replace it.
	dbfile = base_dir / config.get("core", "index", "index");
	maildir = base_dir / config.get("core", "messages", "messages");
	hookdir = base_dir / config.get("core", "hooks", "hooks");
	templatedir = base_dir / config.get("core", "templates", "templates");

	if (mode != Mode::read_only) {
		fs::create_directories(base_dir);
		fs::create_directories(maildir);
	}

	if (mode == Mode::create) {
		fs::create_directories(hookdir);
		fs::create_directories(templatedir);
		install_templates();
	}

	init_index(mode);

	if (mode == Mode::create)
		save_config();
}

void Instance::set_default_config() {
	config.set("core", "index", "index");
	config.set("core", "messages", "messages");
	config.set("core", "hooks", "hooks");
	config.set("core", "templates", "templates");
	config.set("core", "project", "");
	config.set("core", "admin", "");
	config.set_bool("core", "respond-to-new", true);
	config.set_bool("core", "respond-to-reply", true);

	config.set("email", "address", "");
	config.set("email", "name", "");
	config.set("email", "smtphost", "");

	config.set("web", "root", "");
	config.set("web", "static-root", "");
}

void Instance::install_templates() const {
	for (auto &[filename, data]: embedded_templates) {
		auto path = templatedir / filename;
		if (fs::exists(path))
			continue;

		std::ofstream out(path, std::ios::binary);
		out << data;
		out.close();
		if (!out)
			throw std::runtime_error(std::format("Could not write template {}", path.string()));
	}
}

void Instance::init_index(Mode mode) {
	db.open(dbfile.string(), mode == Mode::read_only);

	auto version = db.execute("PRAGMA user_version").get_int(0);
	auto appid = db.execute("PRAGMA application_id").get_int(0);

	if ((appid || version) && appid != application_id)
		throw std::runtime_error("Index is a SQLite database not created by LightBTS!");

	if (!version) {
		if (mode == Mode::read_only)
			throw std::runtime_error("The index has not been created yet");

		std::println(std::cerr, "Creating index...");

		auto transaction = db.begin();
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
		db.execute(std::format("PRAGMA application_id = {}", application_id));
		db.execute(std::format("PRAGMA user_version = {}", index_version));
		transaction.commit();
	} else if (version < index_version) {
		throw std::runtime_error(std::format("Unsupported index version {}, use the Python prototype of LightBTS to upgrade it to version {}", version, index_version));
	} else if (version > index_version) {
		throw std::runtime_error(std::format("Unknown index version {}", version));
	}

	db.execute("PRAGMA foreign_keys = ON");
}

bool Instance::has_config(const std::string &section, const std::string &variable) const {
	return config.exists(section, variable);
}

std::string Instance::get_config(const std::string &section, const std::string &variable) const {
	return config.get(section, variable);
}

void Instance::set_config(const std::string &section, const std::string &variable, const std::string &value) {
	config.set(section, variable, value);
}

void Instance::save_config() const {
	config.save(base_dir / "config");
}

std::string Instance::get_template(std::string_view name) const {
	if (name.contains('/'))
		throw std::invalid_argument("Invalid template name");

	auto path = templatedir / name;
	if (fs::exists(path))
		return read_file(path);

	for (auto &[filename, data]: embedded_templates)
		if (filename == name)
			return std::string(data);

	throw std::runtime_error(std::format("No template named {}", name));
}

std::string Instance::get_local_email_address() const {
	if (auto admin = config.get("core", "admin"); !admin.empty())
		return admin;

	std::string address;
	if (auto email = std::getenv("EMAIL"))
		address = email;

	if (address.contains('<'))
		return address;

	auto user = getpwuid(getuid());

	if (address.empty()) {
		std::string login;
		if (user)
			login = user->pw_name;
		else if (auto name = std::getenv("USER"))
			login = name;
		else
			throw std::runtime_error("Could not determine login name");

		std::array<char, HOST_NAME_MAX + 1> host_name{};
		if (gethostname(host_name.data(), host_name.size() - 1) != 0)
			throw std::runtime_error("Could not determine host name");

		address = std::format("{}@{}", login, host_name.data());
	}

	std::string full_name;
	if (auto name = std::getenv("FULLNAME"))
		full_name = name;
	else if (user && user->pw_gecos)
		full_name = std::string_view(user->pw_gecos).substr(0, std::string_view(user->pw_gecos).find(','));

	if (trim(full_name).empty())
		return address;

	return std::format("{} <{}>", trim(full_name), address);
}

std::vector<Ticket> Instance::list(const Filter &filter) {
	std::string sql = "SELECT id, title, status, severity FROM bugs WHERE 1";

	if (filter.status)
		sql += " AND status = ?";

	if (!filter.severities.empty())
		sql += std::format(" AND severity IN ({})", placeholders(filter.severities.size()));

	if (!filter.tags.empty())
		sql += std::format(" AND EXISTS (SELECT 1 FROM tags WHERE bug = id AND tag IN ({}))", placeholders(filter.tags.size()));

	sql += " ORDER BY id";

	auto statement = db.prepare(sql);
	if (filter.status)
		statement.bind(std::to_underlying(*filter.status));
	for (auto severity: filter.severities)
		statement.bind(std::to_underlying(severity));
	for (auto &tag: filter.tags)
		statement.bind(to_lower(tag));

	std::vector<Ticket> tickets;
	while (statement.step() == SQLITE_ROW)
		tickets.push_back(ticket_from_row(&statement));

	return tickets;
}

std::optional<Ticket> Instance::find_ticket(std::int64_t id) {
	auto result = db.execute("SELECT id, title, status, severity FROM bugs WHERE id = ?", id);
	if (!result)
		return std::nullopt;
	return ticket_from_row(*result.begin());
}

Ticket Instance::get_ticket(std::int64_t id) {
	if (auto ticket = find_ticket(id))
		return *ticket;
	throw std::runtime_error(std::format("No ticket with number {}", id));
}

Ticket Instance::get_ticket(std::string_view id) {
	if (id.contains('@')) {
		auto result = db.execute("SELECT bug FROM messages WHERE msgid = ?", unquote_message_id(id));
		if (!result)
			throw std::runtime_error(std::format("No message with ID {}", id));
		return get_ticket(result.get_int64(0));
	}

	auto number = parse_integer(id.starts_with('#') ? id.substr(1) : id);
	if (!number)
		throw std::runtime_error(std::format("Invalid ticket number {}", id));
	return get_ticket(*number);
}

TicketDetails Instance::get_details(const Ticket &ticket) {
	TicketDetails details{.ticket = ticket};

	auto row = db.execute("SELECT owner, submitter, date, deadline, progress, milestone FROM bugs WHERE id = ?", ticket.id);
	details.owner = row.get_string(0);
	details.submitter = row.get_string(1);
	details.date = row.get_int64(2);
	details.deadline = row.get_string(3);
	details.progress = row.get_int(4);
	details.milestone = row.get_string(5);

	details.tags = get_tags(ticket);

	for (auto &&version: db.execute("SELECT version, status FROM versions WHERE bug = ? ORDER BY version", ticket.id))
		(version.get_int(1) ? details.found_versions : details.fixed_versions).push_back(version.get_string(0));

	for (auto &&link: db.execute("SELECT links.b, links.type, bugs.title FROM links JOIN bugs ON bugs.id = links.b WHERE links.a = ? ORDER BY links.b", ticket.id))
		details.links.push_back({static_cast<LinkType>(link.get_int(1)), link.get_int64(0), link.get_string(2)});

	for (auto &&link: db.execute("SELECT links.a, links.type, bugs.title FROM links JOIN bugs ON bugs.id = links.a WHERE links.b = ? ORDER BY links.a", ticket.id))
		details.reverse_links.push_back({static_cast<LinkType>(link.get_int(1)), link.get_int64(0), link.get_string(2)});

	return details;
}

std::set<std::string> Instance::get_tags(const Ticket &ticket) {
	std::set<std::string> tags;
	for (auto &&row: db.execute("SELECT tag FROM tags WHERE bug = ?", ticket.id))
		tags.insert(row.get_string(0));
	return tags;
}

std::string Instance::get_milestone(const Ticket &ticket) {
	return db.execute("SELECT milestone FROM bugs WHERE id = ?", ticket.id).get_string(0);
}

std::vector<std::string> Instance::get_message_ids(const Ticket &ticket) {
	std::vector<std::string> result;
	for (auto &&row: db.execute("SELECT msgid FROM messages WHERE bug = ? ORDER BY date, rowid", ticket.id))
		result.push_back(row.get_string(0));
	return result;
}

std::string Instance::get_first_message_id(const Ticket &ticket) {
	auto result = db.execute("SELECT msgid FROM messages WHERE bug = ? ORDER BY date, rowid LIMIT 1", ticket.id);
	return result ? result.get_string(0) : std::string{};
}

fs::path Instance::message_path(std::string_view message_id) const {
	auto hash = hash_message_id(message_id);
	return maildir / hash.substr(0, 2) / hash.substr(2);
}

Message Instance::get_message(std::string_view message_id) const {
	Message message;
	message.load(message_path(message_id));
	return message;
}

fs::path Instance::store(const Message &message) const {
	auto path = message_path(message.get_header("Message-ID"));
	fs::create_directories(path.parent_path());
	message.save(path);
	return path;
}

bool Instance::run_hook(std::string_view name, const fs::path &message_file, std::int64_t ticket) {
	if (no_hooks)
		return true;

	auto hook = hookdir / name;
	if (access(hook.c_str(), X_OK) != 0)
		return true;

	ProcessOptions options{
		.working_directory = base_dir,
		.environment = {
			{"LIGHTBTS_DIR", project_dir.string()},
			{"MESSAGE_FILE", message_file.string()},
			{"BUG_ID", ticket ? std::to_string(ticket) : std::string{}},
		},
		.discard_stdin_and_stdout = true,
	};

	try {
		if (int exit_code = run_process({hook.string()}, options)) {
			std::println(std::cerr, "Error while executing {} hook, exit code {}", name, exit_code);
			return false;
		}
	} catch (const std::system_error &error) {
		std::println(std::cerr, "Failed to execute {} hook: {}", name, error.what());
		return false;
	}

	return true;
}

bool Instance::ticket_exists(std::int64_t id) {
	return static_cast<bool>(db.execute("SELECT 1 FROM bugs WHERE id = ?", id));
}

void Instance::set_versions(std::int64_t ticket, std::string_view versions, int status) {
	for (auto &version: split(versions))
		db.execute("INSERT OR REPLACE INTO versions (bug, version, status) VALUES (?, ?, ?)", ticket, version, status);
}

void Instance::remove_versions(std::int64_t ticket, std::string_view versions, int status) {
	for (auto &version: split(versions))
		db.execute("DELETE FROM versions WHERE bug = ? AND version = ? AND status = ?", ticket, version, status);
}

void Instance::apply_tags(std::int64_t ticket, std::string_view tags) {
	bool add = true;

	for (auto tag: split(tags)) {
		if (tag.front() == '-') {
			add = false;
			tag.erase(0, 1);
		} else if (tag.front() == '+') {
			add = true;
			tag.erase(0, 1);
		} else if (tag.front() == '=') {
			add = true;
			db.execute("DELETE FROM tags WHERE bug = ?", ticket);
			tag.erase(0, 1);
		}

		if (tag.empty())
			continue;

		if (add)
			db.execute("INSERT OR IGNORE INTO tags (bug, tag) VALUES (?, ?)", ticket, to_lower(tag));
		else
			db.execute("DELETE FROM tags WHERE bug = ? AND tag = ?", ticket, to_lower(tag));
	}
}

void Instance::apply_links(std::int64_t ticket, LinkType type, std::string_view targets, bool add, std::vector<std::string> &warnings) {
	for (auto &target: split(targets)) {
		auto number = parse_integer(target.starts_with('#') ? std::string_view(target).substr(1) : target);
		if (!number || *number == ticket || !ticket_exists(*number)) {
			warnings.push_back(std::format("Invalid ticket number \"{}\" for {} link", target, name(type)));
			continue;
		}

		if (add)
			db.execute("INSERT OR REPLACE INTO links (a, b, type) VALUES (?, ?, ?)", ticket, *number, std::to_underlying(type));
		else
			db.execute("DELETE FROM links WHERE a = ? AND b = ? AND type = ?", ticket, *number, std::to_underlying(type));
	}
}

void Instance::apply_metadata(std::int64_t ticket, const Message &message, std::vector<std::string> &warnings) {
	std::optional<std::string> status;
	std::optional<std::string> severity;
	std::optional<std::string> title;
	std::optional<std::string> owner;
	std::optional<std::string> progress;
	std::optional<std::string> milestone;
	std::optional<std::string> deadline;
	std::vector<std::string> tags;
	std::vector<std::string> versions;
	std::vector<std::string> found;
	std::vector<std::string> notfound;
	std::vector<std::string> fixed;
	std::vector<std::string> notfixed;
	std::vector<std::pair<LinkType, std::string>> links;
	std::vector<std::pair<LinkType, std::string>> unlinks;

	auto set_once = [&](std::optional<std::string> &variable, std::string_view key, const std::string &value) {
		if (variable && *variable != value)
			warnings.push_back(std::format("Duplicate {} field", key));
		variable = value;
	};

	if (auto header = message.get_header_value("X-LightBTS-Status"); !trim(header).empty())
		status = trim(header);

	if (auto header = message.get_header_value("X-LightBTS-Tag"); !trim(header).empty())
		tags.emplace_back(trim(header));

	for (auto &[key, value]: parse_pseudo_headers(message.get_text())) {
		if (key == "status")
			set_once(status, key, value);
		else if (key == "severity")
			set_once(severity, key, value);
		else if (key == "title" || key == "topic")
			set_once(title, "title", value);
		else if (key == "owner")
			set_once(owner, key, value);
		else if (key == "progress")
			set_once(progress, key, value);
		else if (key == "milestone")
			set_once(milestone, key, value);
		else if (key == "deadline")
			set_once(deadline, key, value);
		else if (key == "tag" || key == "tags")
			tags.push_back(value);
		else if (key == "version")
			versions.push_back(value);
		else if (key == "found")
			found.push_back(value);
		else if (key == "notfound")
			notfound.push_back(value);
		else if (key == "fixed")
			fixed.push_back(value);
		else if (key == "notfixed")
			notfixed.push_back(value);
		else if (auto type = parse_link_type(key))
			links.emplace_back(*type, value);
		else if (auto untype = key.starts_with("un") ? parse_link_type(std::string_view(key).substr(2)) : std::nullopt)
			unlinks.emplace_back(*untype, value);
		else
			warnings.push_back(std::format("Unknown pseudo-header field \"{}\"", key));
	}

	auto set_column = [&](std::string_view column, const std::string &value) {
		auto sql = std::format("UPDATE bugs SET {} = ? WHERE id = ?", column);
		if (value == "-")
			db.execute(sql, nullptr, ticket);
		else
			db.execute(sql, value, ticket);
	};

	if (status) {
		if (auto value = parse_status(to_lower(*status)))
			db.execute("UPDATE bugs SET status = ? WHERE id = ?", std::to_underlying(*value), ticket);
		else
			warnings.push_back(std::format("Invalid status \"{}\"", *status));
	}

	if (severity) {
		if (auto value = parse_severity(to_lower(*severity)))
			db.execute("UPDATE bugs SET severity = ? WHERE id = ?", std::to_underlying(*value), ticket);
		else
			warnings.push_back(std::format("Invalid severity \"{}\"", *severity));
	}

	if (title) {
		if (title->empty())
			warnings.push_back("Empty title");
		else
			db.execute("UPDATE bugs SET title = ? WHERE id = ?", *title, ticket);
	}

	auto ticket_status = db.execute("SELECT status FROM bugs WHERE id = ?", ticket).get_int(0);
	for (auto &version: versions)
		set_versions(ticket, version, ticket_status);
	for (auto &version: notfound)
		remove_versions(ticket, version, 1);
	for (auto &version: found)
		set_versions(ticket, version, 1);
	for (auto &version: notfixed)
		remove_versions(ticket, version, 0);
	for (auto &version: fixed)
		set_versions(ticket, version, 0);

	if (progress) {
		std::string_view text = *progress;
		if (text.ends_with('%'))
			text.remove_suffix(1);
		if (auto value = parse_integer(trim(text)); value && *value >= 0 && *value <= 100)
			db.execute("UPDATE bugs SET progress = ? WHERE id = ?", *value, ticket);
		else
			warnings.push_back(std::format("Invalid progress \"{}\"", *progress));
	}

	if (milestone)
		set_column("milestone", *milestone);

	if (deadline) {
		if (*deadline == "-" || is_valid_date(*deadline))
			set_column("deadline", *deadline);
		else
			warnings.push_back(std::format("Invalid deadline \"{}\", expected YYYY-MM-DD", *deadline));
	}

	if (owner)
		set_column("owner", *owner);

	for (auto &value: tags)
		apply_tags(ticket, value);

	for (auto &[type, targets]: links)
		apply_links(ticket, type, targets, true, warnings);

	for (auto &[type, targets]: unlinks)
		apply_links(ticket, type, targets, false, warnings);
}

ImportResult Instance::import(const Message &incoming) {
	// Don't allow messages with the X-LightBTS-Control header set
	if (!incoming.get_header("X-LightBTS-Control").empty())
		throw std::runtime_error("Denying import of message with X-LightBTS-Control header");

	Message message = incoming;

	if (message.get_header("Message-ID").empty())
		message.generate_msgid("LightBTS");

	message.add_received("by localhost (LightBTS)");

	if (message.get_header("Date").empty())
		message.set_date();

	ImportResult result{.kind = ImportResult::Kind::duplicate, .message_id = unquote_message_id(message.get_header("Message-ID"))};
	auto parent = unquote_message_id(message.get_header("In-Reply-To"));

	// Check for duplicates before storing anything, so the original message is never overwritten.
	if (db.execute("SELECT 1 FROM messages WHERE msgid = ?", result.message_id))
		return result;

	auto filename = store(message);

	if (!run_hook("pre-index", filename)) {
		fs::remove(filename);
		result.kind = ImportResult::Kind::rejected;
		return result;
	}

	auto transaction = db.begin();
	auto now = current_unix_time();
	auto from = message.get_header_value("From");

	if (!parent.empty())
		if (auto row = db.execute("SELECT bug FROM messages WHERE msgid = ?", parent))
			result.ticket = row.get_int64(0);

	if (result.ticket) {
		result.kind = ImportResult::Kind::appended;
	} else {
		db.execute("INSERT INTO bugs (title, submitter, date) VALUES (?, ?, ?)", message.get_header_value("Subject"), from, now);
		result.ticket = db.last_insert_rowid();
		result.kind = ImportResult::Kind::created;
	}

	db.execute("INSERT INTO messages (msgid, bug, date) VALUES (?, ?, ?)", result.message_id, result.ticket, now);
	db.execute("INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)", result.ticket, from);

	apply_metadata(result.ticket, message, result.warnings);

	transaction.commit();

	run_hook("post-index", filename, result.ticket);
	return result;
}

}

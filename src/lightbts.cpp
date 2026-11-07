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
#include <deque>
#include <iostream>
#include <iterator>
#include <map>
#include <print>
#include <pwd.h>
#include <sstream>
#include <stdexcept>
#include <tuple>
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

/// The start of the Received header that LightBTS adds to every message it imports.
constexpr std::string_view received_text = "by localhost (LightBTS)";

/// A header with the precise time a message was imported, which is needed to replay messages in the
/// right order when reindexing. The Received header only has a resolution of one second.
constexpr std::string_view received_time_header = "X-LightBTS-Received";

using Microseconds = std::chrono::sys_time<std::chrono::microseconds>;

std::int64_t to_unix_time(std::chrono::system_clock::time_point time) {
	return std::chrono::floor<std::chrono::seconds>(time).time_since_epoch().count();
}

/// Parses an RFC 5322 date, like "Sat, 07 Nov 2026 22:42:03 +0000".
std::optional<Microseconds> parse_date(std::string_view text) {
	std::string date(trim(text.substr(0, text.find('('))));

	if (auto comma = date.find(','); comma != date.npos)
		date = trim(std::string_view(date).substr(comma + 1));

	for (auto zone: {" GMT", " UT", " UTC", " Z"}) {
		if (date.ends_with(zone)) {
			date.resize(date.size() - std::string_view(zone).size());
			date += " +0000";
		}
	}

	for (auto format: {"%d %b %Y %H:%M:%S %z", "%d %b %Y %H:%M %z"}) {
		std::chrono::sys_seconds time;
		std::istringstream in(date);
		in >> std::chrono::parse(format, time);
		if (in)
			return time;
	}

	return std::nullopt;
}

std::string format_precise_time(std::chrono::system_clock::time_point time) {
	return std::format("{:%FT%T}Z", std::chrono::floor<std::chrono::microseconds>(time));
}

std::optional<Microseconds> parse_precise_time(std::string_view text) {
	Microseconds time;
	std::istringstream in{std::string(trim(text))};
	in >> std::chrono::parse("%FT%TZ", time);
	if (!in)
		return std::nullopt;
	return time;
}

/// The time at which LightBTS imported a message, from the headers it added.
std::optional<Microseconds> get_received_time(const Message &message) {
	if (auto precise = parse_precise_time(message.get_header(received_time_header)))
		return precise;

	for (auto &[field, value]: message.get_headers()) {
		if (to_lower(field) != "received" || !trim(value).starts_with(received_text))
			continue;
		auto semicolon = value.rfind(';');
		if (semicolon == value.npos)
			return std::nullopt;
		return parse_date(std::string_view(value).substr(semicolon + 1));
	}

	return std::nullopt;
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
		create_schema();
		transaction.commit();
	} else if (version < index_version) {
		throw std::runtime_error(std::format("Unsupported index version {}, use the Python prototype of LightBTS to upgrade it to version {}", version, index_version));
	} else if (version > index_version) {
		throw std::runtime_error(std::format("Unknown index version {}", version));
	}

	db.execute("PRAGMA foreign_keys = ON");
}

void Instance::create_schema() {
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
}

namespace {

struct IndexTable {
	std::string_view name;
	std::size_t key_columns; ///< The number of leading columns that form the primary key.
};

/// All tables in the index. Tables referring to other tables come first, because of the foreign keys.
constexpr IndexTable index_tables[] = {
	{"links", 2},
	{"messages", 1},
	{"recipients", 2},
	{"tags", 2},
	{"versions", 2},
	{"bugs", 1},
};

}

void Instance::drop_schema() {
	for (auto &table: index_tables)
		db.execute(std::format("DROP TABLE IF EXISTS {}", table.name));
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

std::set<std::string> Instance::get_all_tags() {
	std::set<std::string> tags;
	for (auto &&row: db.execute("SELECT DISTINCT tag FROM tags"))
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

bool Instance::has_message(std::string_view message_id) {
	return static_cast<bool>(db.execute("SELECT 1 FROM messages WHERE msgid = ?", unquote_message_id(message_id)));
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

namespace {

struct StoredMessage {
	Message message;
	std::string id;
	std::string parent;
	Microseconds received;
	std::size_t previous_position; ///< The position in the previous index, used to order messages received in the same second.
};

}

ReindexResult Instance::reindex() {
	auto transaction = db.begin(true);
	auto result = rebuild_index();
	transaction.commit();
	return result;
}

ReindexResult Instance::rebuild_index() {
	ReindexResult result;

	// Remember the order of the messages, and which ticket they belonged to in the current index.
	std::map<std::string, std::size_t> previous_positions;
	std::map<std::string, std::int64_t> previous_tickets;
	std::set<std::int64_t> previous_ticket_numbers;
	std::int64_t next_ticket_number = 1;

	for (auto &&row: db.execute("SELECT msgid, bug FROM messages ORDER BY date, rowid")) {
		auto message_id = row.get_string(0);
		previous_positions.emplace(message_id, previous_positions.size());
		previous_tickets.emplace(message_id, row.get_int64(1));
		previous_ticket_numbers.insert(row.get_int64(1));
	}

	for (auto &&row: db.execute("SELECT id FROM bugs"))
		previous_ticket_numbers.insert(row.get_int64(0));

	// Numbers of tickets that were removed by an earlier reindex are only remembered in sqlite_sequence.
	if (auto row = db.execute("SELECT max(id) FROM bugs"))
		next_ticket_number = std::max(next_ticket_number, row.get_int64(0) + 1);
	if (auto row = db.execute("SELECT seq FROM sqlite_sequence WHERE name = 'bugs'"))
		next_ticket_number = std::max(next_ticket_number, row.get_int64(0) + 1);

	// Load all messages.
	std::vector<StoredMessage> messages;
	std::error_code error;

	for (fs::recursive_directory_iterator it(maildir, error), end; it != end; it.increment(error)) {
		auto &path = it->path();
		if (path.filename().string().starts_with('.')) {
			if (it->is_directory())
				it.disable_recursion_pending();
			continue;
		}

		if (!it->is_regular_file())
			continue;

		StoredMessage stored;
		try {
			stored.message.load(path);
		} catch (const std::exception &load_error) {
			result.errors.push_back(std::format("{}: {}", path.string(), load_error.what()));
			continue;
		}

		stored.id = unquote_message_id(stored.message.get_header("Message-ID"));
		if (stored.id.empty()) {
			result.errors.push_back(std::format("{}: no Message-ID", path.string()));
			continue;
		}

		if (message_path(stored.id) != path) {
			result.errors.push_back(std::format("{}: should be stored as {} according to its Message-ID", path.string(), message_path(stored.id).string()));
			continue;
		}

		auto received = get_received_time(stored.message);
		if (!received)
			received = parse_date(stored.message.get_header_value("Date"));
		if (!received) {
			result.errors.push_back(std::format("{}: unknown date", path.string()));
			continue;
		}

		stored.received = *received;
		stored.parent = unquote_message_id(stored.message.get_header("In-Reply-To"));
		auto position = previous_positions.find(stored.id);
		stored.previous_position = position == previous_positions.end() ? previous_positions.size() : position->second;
		messages.push_back(std::move(stored));
	}

	if (error)
		throw std::runtime_error(std::format("Could not read {}: {}", maildir.string(), error.message()));

	std::ranges::sort(messages, {}, [](const StoredMessage &stored) {
		return std::tie(stored.received, stored.previous_position, stored.id);
	});

	drop_schema();
	create_schema();

	// Index all messages, but replies only after the message they reply to.
	std::set<std::string> all_ids;
	for (auto &stored: messages)
		all_ids.insert(stored.id);

	std::set<std::string> indexed_ids;
	std::multimap<std::string, std::size_t> waiting_for_parent;
	std::set<std::int64_t> used_ticket_numbers;
	std::vector<bool> done(messages.size());

	auto index_one = [&](std::size_t i) {
		// A message that creates a ticket gets the number of the ticket it belonged to, if that is still free.
		auto &stored = messages[i];
		auto previous_ticket = previous_tickets.find(stored.id);
		auto number = next_ticket_number;
		if (previous_ticket != previous_tickets.end() && !used_ticket_numbers.contains(previous_ticket->second))
			number = previous_ticket->second;

		ImportResult indexed{.kind = ImportResult::Kind::created, .message_id = stored.id};
		index_message(stored.message, to_unix_time(stored.received), number, indexed);

		result.messages++;
		for (auto &warning: indexed.warnings)
			result.warnings.push_back(std::format("{}: {}", stored.id, warning));

		if (indexed.kind == ImportResult::Kind::created) {
			result.tickets++;
			used_ticket_numbers.insert(indexed.ticket);
			if (indexed.ticket == next_ticket_number)
				next_ticket_number++;
		}
	};

	// Indexes a message, followed by the replies that were waiting for it.
	auto index = [&](std::size_t first) {
		std::deque<std::size_t> queue{first};
		while (!queue.empty()) {
			auto i = queue.front();
			queue.pop_front();
			if (done[i])
				continue;
			done[i] = true;
			index_one(i);
			indexed_ids.insert(messages[i].id);

			auto [begin, end] = waiting_for_parent.equal_range(messages[i].id);
			for (auto it = begin; it != end; ++it)
				queue.push_back(it->second);
			waiting_for_parent.erase(begin, end);
		}
	};

	for (std::size_t i = 0; i < messages.size(); i++) {
		auto &parent = messages[i].parent;
		if (!parent.empty() && parent != messages[i].id && all_ids.contains(parent) && !indexed_ids.contains(parent))
			waiting_for_parent.emplace(parent, i);
		else
			index(i);
	}

	// Only messages whose replies form a cycle can be left over.
	for (std::size_t i = 0; i < messages.size(); i++)
		index(i);

	// Never reuse the numbers of tickets that no longer exist.
	db.execute("DELETE FROM sqlite_sequence WHERE name = 'bugs'");
	db.execute("INSERT INTO sqlite_sequence (name, seq) VALUES ('bugs', ?)", next_ticket_number - 1);

	std::ranges::set_difference(previous_ticket_numbers, used_ticket_numbers, std::back_inserter(result.removed_tickets));

	return result;
}

namespace {

struct TableContents {
	std::vector<std::string> columns;
	std::map<std::vector<std::string>, std::vector<std::string>> rows; ///< Rows by their primary key.
};

std::string format_value(const SQLite3::statement &statement, int column) {
	switch (statement.column_type(column)) {
	case SQLITE_NULL:
		return "NULL";
	case SQLITE_INTEGER:
	case SQLITE_FLOAT:
		return statement.column_string(column);
	default:
		return std::format("\"{}\"", statement.column_string(column));
	}
}

TableContents read_table(SQLite3::database &db, const IndexTable &table) {
	auto statement = db.prepare(std::format("SELECT * FROM {}", table.name));

	TableContents contents;
	for (int i = 0; i < statement.column_count(); i++)
		contents.columns.push_back(statement.column_name(i));

	while (statement.step() == SQLITE_ROW) {
		std::vector<std::string> row;
		for (int i = 0; i < statement.column_count(); i++)
			row.push_back(format_value(statement, i));
		std::vector<std::string> key(row.begin(), row.begin() + table.key_columns);
		contents.rows.emplace(std::move(key), std::move(row));
	}

	return contents;
}

std::string without_quotes(std::string_view value) {
	if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
		value = value.substr(1, value.size() - 2);
	return std::string(value);
}

/// Shows enumerations by their names instead of their numbers.
std::string describe_value(std::string_view table, std::string_view column, const std::string &value) {
	auto number = parse_integer(value);
	if (table == "bugs" && column == "status" && number && *number >= 0 && *number < std::ssize(status_names))
		return std::string(status_names[*number]);
	if (table == "bugs" && column == "severity" && number && *number >= 0 && *number < std::ssize(severity_names))
		return std::string(severity_names[*number]);
	if (table == "links" && column == "type" && number && *number >= 0 && *number < std::ssize(link_names))
		return std::string(link_names[*number]);
	return value;
}

std::string describe_row(const IndexTable &table, const TableContents &contents, const std::vector<std::string> &row) {
	std::string text;
	for (std::size_t i = 0; i < row.size(); i++)
		text += std::format("{}{} = {}", i ? ", " : "", contents.columns[i], describe_value(table.name, contents.columns[i], row[i]));
	return std::format("{} ({})", table.name, text);
}

/// What a row is about: a ticket or a message for the bugs and messages tables, otherwise the whole row.
std::string describe_subject(const IndexTable &table, const TableContents &contents, const std::vector<std::string> &row) {
	if (table.name == "bugs")
		return std::format("Ticket {}", row[0]);
	if (table.name == "messages")
		return std::format("Message {}", without_quotes(row[0]));
	return std::format("The row {}", describe_row(table, contents, row));
}

std::string_view not_produced_reason(const IndexTable &table) {
	if (table.name == "bugs")
		return "no message creates it";
	if (table.name == "messages")
		return "there is no file for it";
	return "the messages do not produce it";
}

void compare_table(const IndexTable &table, const TableContents &index, const TableContents &expected, std::vector<std::string> &problems) {
	for (auto &[key, row]: index.rows) {
		auto subject = describe_subject(table, index, row);
		auto other = expected.rows.find(key);

		if (other == expected.rows.end()) {
			problems.push_back(std::format("{} is in the index, but {}", subject, not_produced_reason(table)));
			continue;
		}

		for (std::size_t i = table.key_columns; i < row.size(); i++) {
			auto &column = index.columns[i];
			if (row[i] != other->second[i])
				problems.push_back(std::format("{}: {} is {} in the index, but {} according to the messages", subject, column, describe_value(table.name, column, row[i]), describe_value(table.name, column, other->second[i])));
		}
	}

	for (auto &[key, row]: expected.rows)
		if (!index.rows.contains(key))
			problems.push_back(std::format("{} is missing from the index", describe_subject(table, expected, row)));
}

}

FsckResult Instance::fsck() {
	FsckResult result;

	for (auto &&row: db.execute("PRAGMA integrity_check"))
		if (auto message = row.get_string(0); message != "ok")
			result.database_problems.push_back(message);

	for (auto &&row: db.execute("PRAGMA foreign_key_check"))
		result.database_problems.push_back(std::format("Row {} of table {} refers to a missing row in table {}", row.get_int64(1), row.get_string(0), row.get_string(2)));

	// Rebuild the index inside a transaction that is rolled back, and compare it with the current one.
	auto transaction = db.begin(true);

	std::vector<TableContents> index;
	for (auto &table: index_tables)
		index.push_back(read_table(db, table));

	auto rebuilt = rebuild_index();
	result.file_problems = rebuilt.errors;

	for (std::size_t i = 0; i < std::size(index_tables); i++)
		compare_table(index_tables[i], index[i], read_table(db, index_tables[i]), result.index_problems);

	transaction.abort();
	return result;
}

void Instance::index_message(const Message &message, std::int64_t received, std::optional<std::int64_t> new_ticket_number, ImportResult &result) {
	auto parent = unquote_message_id(message.get_header("In-Reply-To"));
	auto from = message.get_header_value("From");

	result.ticket = 0;
	if (!parent.empty())
		if (auto row = db.execute("SELECT bug FROM messages WHERE msgid = ?", parent))
			result.ticket = row.get_int64(0);

	if (result.ticket) {
		result.kind = ImportResult::Kind::appended;
	} else {
		auto title = message.get_header_value("Subject");
		if (new_ticket_number)
			db.execute("INSERT INTO bugs (id, title, submitter, date) VALUES (?, ?, ?, ?)", *new_ticket_number, title, from, received);
		else
			db.execute("INSERT INTO bugs (title, submitter, date) VALUES (?, ?, ?)", title, from, received);
		result.ticket = db.last_insert_rowid();
		result.kind = ImportResult::Kind::created;
	}

	db.execute("INSERT INTO messages (msgid, bug, date) VALUES (?, ?, ?)", result.message_id, result.ticket, received);
	db.execute("INSERT OR IGNORE INTO recipients (bug, address) VALUES (?, ?)", result.ticket, from);

	apply_metadata(result.ticket, message, result.warnings);
}

ImportResult Instance::import(const Message &incoming) {
	// Don't allow messages with the X-LightBTS-Control header set
	if (!incoming.get_header("X-LightBTS-Control").empty())
		throw std::runtime_error("Denying import of message with X-LightBTS-Control header");

	Message message = incoming;

	if (message.get_header("Message-ID").empty())
		message.generate_msgid("LightBTS");

	auto now = std::chrono::system_clock::now();
	message.add_received(received_text, now);
	message.erase_header(received_time_header);
	message.prepend_header(received_time_header, format_precise_time(now));

	if (message.get_header("Date").empty())
		message.set_date();

	ImportResult result{.kind = ImportResult::Kind::duplicate, .message_id = unquote_message_id(message.get_header("Message-ID"))};

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
	index_message(message, to_unix_time(now), std::nullopt, result);
	transaction.commit();

	run_hook("post-index", filename, result.ticket);
	return result;
}

}

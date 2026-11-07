#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <mimesis.hpp>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "config.hpp"
#include "sqlite3.hpp"

namespace LightBTS {

using Mimesis::Message;
namespace fs = std::filesystem;

enum class Status {
	closed,
	open,
};

enum class Severity {
	wishlist,
	minor,
	normal,
	important,
	serious,
	critical,
	grave,
};

enum class LinkType {
	relates,
	duplicates,
	depends,
	blocks,
};

inline constexpr std::array<std::string_view, 2> status_names = {"closed", "open"};
inline constexpr std::array<std::string_view, 7> severity_names = {"wishlist", "minor", "normal", "important", "serious", "critical", "grave"};
inline constexpr std::array<std::string_view, 4> link_names = {"relates", "duplicates", "depends", "blocks"};
inline constexpr std::array<std::string_view, 4> verbose_link_names = {"relates to", "duplicates", "depends on", "blocks"};
inline constexpr std::array<std::string_view, 4> reverse_link_names = {"related to by", "duplicated by", "depended on by", "blocked by"};

template<typename Enum, std::size_t N>
std::optional<Enum> parse_enum(const std::array<std::string_view, N> &names, std::string_view name) {
	auto it = std::ranges::find(names, name);
	if (it == names.end())
		return std::nullopt;
	return static_cast<Enum>(it - names.begin());
}

inline std::optional<Status> parse_status(std::string_view name) { return parse_enum<Status>(status_names, name); }
inline std::optional<Severity> parse_severity(std::string_view name) { return parse_enum<Severity>(severity_names, name); }
inline std::optional<LinkType> parse_link_type(std::string_view name) { return parse_enum<LinkType>(link_names, name); }

inline std::string_view name(Status status) { return status_names.at(std::to_underlying(status)); }
inline std::string_view name(Severity severity) { return severity_names.at(std::to_underlying(severity)); }
inline std::string_view name(LinkType type) { return link_names.at(std::to_underlying(type)); }

/// Removes the angle brackets around a Message-ID, if present.
std::string unquote_message_id(std::string_view message_id);

struct Ticket {
	std::int64_t id = 0;
	std::string title;
	Status status = Status::open;
	Severity severity = Severity::normal;
};

struct Link {
	LinkType type;
	std::int64_t ticket;
	std::string title;
};

struct TicketDetails {
	Ticket ticket;
	std::string owner;
	std::string submitter;
	std::int64_t date = 0; ///< Seconds since the Unix epoch, or 0 if unknown.
	std::string deadline;
	int progress = 0;
	std::string milestone;
	std::set<std::string> tags;
	std::vector<std::string> found_versions;
	std::vector<std::string> fixed_versions;
	std::vector<Link> links;         ///< Links from this ticket to other tickets.
	std::vector<Link> reverse_links; ///< Links from other tickets to this ticket.
};

/// Which tickets Instance::list() returns. Tickets match if they have any of the given severities and tags.
struct Filter {
	std::optional<Status> status = Status::open; ///< Unset to match any status.
	std::vector<Severity> severities;
	std::vector<std::string> tags;
};

struct ImportResult {
	enum class Kind {
		created,   ///< The message created a new ticket.
		appended,  ///< The message was added to an existing ticket.
		duplicate, ///< A message with the same Message-ID was already imported.
		rejected,  ///< The pre-index hook rejected the message.
	};

	Kind kind;
	std::int64_t ticket = 0;
	std::string message_id;
	std::vector<std::string> warnings; ///< Problems with the pseudo-headers in the message.
};

struct InstanceOptions {
	bool no_hooks = false;
};

/// A LightBTS instance: the .lightbts directory with its configuration, messages and index.
class Instance {
	public:
	enum class Mode {
		open,      ///< Open an existing instance in the given directory or one of its parents.
		create,    ///< Create a new instance in the given directory.
		read_only, ///< Like open, but never modify anything.
	};

	/// \param start_dir The directory to start searching for an instance. If empty, LIGHTBTS_DIR or the current directory is used.
	Instance(const fs::path &start_dir, Mode mode = Mode::open, InstanceOptions options = {});

	Instance(const Instance &) = delete;
	Instance &operator=(const Instance &) = delete;

	const fs::path &get_project_dir() const { return project_dir; }

	bool has_config(const std::string &section, const std::string &variable) const;
	std::string get_config(const std::string &section, const std::string &variable) const;
	void set_config(const std::string &section, const std::string &variable, const std::string &value);
	void save_config() const;

	/// Gets a template from the instance's template directory, or the built-in one if there is no such file.
	std::string get_template(std::string_view name) const;

	std::string get_local_email_address() const;

	std::vector<Ticket> list(const Filter &filter = {});
	std::optional<Ticket> find_ticket(std::int64_t id);
	/// \throws std::runtime_error if there is no such ticket.
	Ticket get_ticket(std::int64_t id);
	/// Gets a ticket by number, or by the Message-ID of one of its messages if the text contains an @.
	/// \throws std::runtime_error if there is no such ticket.
	Ticket get_ticket(std::string_view id);
	TicketDetails get_details(const Ticket &ticket);

	std::set<std::string> get_tags(const Ticket &ticket);
	std::string get_milestone(const Ticket &ticket);
	/// Gets the Message-IDs of all messages of a ticket, in the order they were imported.
	std::vector<std::string> get_message_ids(const Ticket &ticket);
	/// \return The Message-ID of the first message of the ticket, or an empty string if there are none.
	std::string get_first_message_id(const Ticket &ticket);

	Message get_message(std::string_view message_id) const;

	ImportResult import(const Message &message);

	private:
	fs::path project_dir;
	fs::path base_dir;
	fs::path dbfile;
	fs::path maildir;
	fs::path hookdir;
	fs::path templatedir;

	bool no_hooks;

	SQLite3::database db;
	Config config;

	void set_default_config();
	void install_templates() const;
	void init_index(Mode mode);

	fs::path message_path(std::string_view message_id) const;
	fs::path store(const Message &message) const;
	bool run_hook(std::string_view name, const fs::path &message_file, std::int64_t ticket = 0);

	bool ticket_exists(std::int64_t id);
	void apply_metadata(std::int64_t ticket, const Message &message, std::vector<std::string> &warnings);
	void apply_tags(std::int64_t ticket, std::string_view tags);
	void set_versions(std::int64_t ticket, std::string_view versions, int status);
	void remove_versions(std::int64_t ticket, std::string_view versions, int status);
	void apply_links(std::int64_t ticket, LinkType type, std::string_view targets, bool add, std::vector<std::string> &warnings);
};

}

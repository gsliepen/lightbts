#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <mimesis.hpp>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "config.hpp"
#include "fields.hpp"
#include "sqlite3.hpp"

namespace LightBTS {

using Mimesis::Message;
namespace fs = std::filesystem;

enum class LinkType {
	relates,
	duplicates,
	depends,
	blocks,
};

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

inline std::optional<LinkType> parse_link_type(std::string_view name) { return parse_enum<LinkType>(link_names, name); }

inline std::string_view name(LinkType type) { return link_names.at(std::to_underlying(type)); }

/// The link type that means the same in the other direction: A depends on B if B blocks A.
inline std::optional<LinkType> inverse(LinkType type) {
	switch (type) {
	case LinkType::depends: return LinkType::blocks;
	case LinkType::blocks: return LinkType::depends;
	default: return std::nullopt;
	}
}

/// Removes the angle brackets around a Message-ID, if present.
std::string unquote_message_id(std::string_view message_id);

/// The values of the fields of a ticket, by the name of the field. Unset fields are not present.
using FieldValues = std::map<std::string, std::vector<std::string>, std::less<>>;

struct Ticket {
	std::int64_t id = 0;   ///< The number of the ticket, which is only valid in this instance.
	std::string hash;      ///< The ID of the ticket, a hash of the Message-ID of its first message, the same everywhere.
	std::string title;
	FieldValues fields;

	/// The values of a field, separated by spaces, or an empty string if it is not set.
	std::string get(std::string_view field) const;
};

struct Link {
	LinkType type;
	std::int64_t ticket;
	std::string title;
};

struct TicketDetails {
	Ticket ticket;
	std::string description; ///< The current description, without pseudo-headers.
	std::string description_message_id; ///< The message that contains the current description.
	std::string submitter;
	std::int64_t date = 0; ///< Seconds since the Unix epoch, or 0 if unknown.
	std::vector<std::string> found_versions;
	std::vector<std::string> fixed_versions;
	std::vector<Link> links;         ///< Links from this ticket to other tickets.
	std::vector<Link> reverse_links; ///< Links from other tickets to this ticket.
};

/// Sorts tickets by a field, by "title", or by "number", which is the default if the key is empty.
/// A leading - sorts in descending order. Enum values sort in the order they are defined, numbers numerically,
/// and other values as text. Tickets where the field is unset come last, and ties are sorted by number.
/// \throws std::runtime_error if there is no such field.
void sort_tickets(std::vector<Ticket> &tickets, const FieldDefinitions &definitions, std::string_view key);

struct SearchMatch {
	std::string message_id;
	std::string snippet; ///< Part of the message, with the matching words between [ and ].
};

struct SearchResult {
	Ticket ticket;
	std::vector<SearchMatch> matches; ///< The matching messages, best match first.
};

/// Converts words given on the command line to an FTS5 query. Words that are not FTS5 syntax are quoted,
/// so that words like lbts-web match as a phrase instead of being a syntax error. If any word contains a
/// double quote, the words are used as they are.
std::string make_search_query(std::span<const std::string> words);

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

struct ReindexResult {
	std::size_t messages = 0;
	std::size_t tickets = 0;
	std::vector<std::string> errors;            ///< Message files that could not be indexed.
	std::vector<std::string> warnings;          ///< Problems with the pseudo-headers in messages.
	std::vector<std::int64_t> removed_tickets;  ///< Tickets that existed before, but that no message creates anymore.
};

struct FsckResult {
	std::vector<std::string> file_problems;     ///< Message files that cannot be indexed.
	std::vector<std::string> database_problems; ///< Problems found by SQLite's own integrity checks.
	std::vector<std::string> index_problems;    ///< Differences between the index and what the messages produce.

	bool ok() const { return file_problems.empty() && database_problems.empty() && index_problems.empty(); }
};

struct InstanceOptions {
	bool no_hooks = false;
	bool update_index = true;        ///< Whether to rebuild an index that is outdated, unless the instance is opened read-only.
	bool configuration_only = false; ///< Only use the configuration: allow invalid field definitions, and don't open the index.
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
	/// The .lightbts directory.
	const fs::path &get_base_dir() const { return base_dir; }

	/// Whether the instance is stored in git, see doc/git.mdwn.
	bool uses_git() const { return config.get_bool("core", "git", false); }
	/// Runs git in the .lightbts directory.
	/// \param output If set, the standard output of git is stored in it, otherwise it is discarded.
	/// \return The exit code of git.
	int run_git(const std::vector<std::string> &arguments, std::string *output = nullptr) const;
	/// Commits changes to files in the .lightbts directory, if the instance is stored in git.
	/// Failures are reported on stderr, since the next lbts sync commits what is left.
	/// \return Whether committing succeeded, or there was nothing to commit.
	bool commit(const std::vector<fs::path> &paths, const std::string &message) const;

	bool has_config(const std::string &section, const std::string &variable) const;
	std::string get_config(const std::string &section, const std::string &variable) const;
	/// \param local Whether to change the local configuration in config.local, instead of the shared one in config.
	void set_config(const std::string &section, const std::string &variable, const std::string &value, bool local = false);
	void save_config() const;

	/// \throws std::runtime_error if the field definitions in the configuration are invalid.
	const FieldDefinitions &get_fields() const;

	/// Gets a template from the instance's template directory, or the built-in one if there is no such file.
	std::string get_template(std::string_view name) const;

	std::string get_local_email_address() const;

	std::vector<Ticket> list(const Filter &filter = {});
	std::optional<Ticket> find_ticket(std::int64_t id);
	/// \throws std::runtime_error if there is no such ticket.
	Ticket get_ticket(std::int64_t id);
	/// Gets a ticket by its number, a prefix of its ID, or the Message-ID of one of its messages if the text contains an @.
	/// \throws std::runtime_error if there is no such ticket.
	Ticket get_ticket(std::string_view reference);
	/// Finds the number of a ticket by its number, optionally preceded by #, or by a prefix of at least 4 characters of its ID.
	/// Decimal numbers of at least 7 digits are also tried as an ID, if there is no ticket with that number.
	/// \throws std::runtime_error if there is no such ticket, or the prefix is ambiguous.
	std::int64_t find_ticket_number(std::string_view reference);

	/// The ID of a ticket as it is shown: the shortest prefix of the hash that is unique in this instance, but at least 7 characters.
	std::string short_id(const Ticket &ticket);
	/// The ID of a ticket as it is written in messages that refer to it.
	static std::string reference_id(const Ticket &ticket) { return ticket.hash.substr(0, 16); }
	TicketDetails get_details(const Ticket &ticket);
	/// The current description of a ticket: the text of its first message, or of the last message
	/// with the pseudo-header "Description: replace", without the pseudo-headers.
	std::string get_description(const Ticket &ticket);

	/// The tickets that each ticket links to with the given type, or with its inverse in the other direction.
	std::map<std::int64_t, std::vector<std::int64_t>> get_link_graph(LinkType type);

	/// Finds the tickets whose messages match a full-text search, best match first.
	/// \param query A query in the syntax of SQLite's FTS5 extension.
	/// \throws std::runtime_error if the query is not valid.
	std::vector<SearchResult> search(const std::string &query, const Filter &filter);

	/// Gets all values of a field that are used by any ticket.
	std::set<std::string> get_used_values(std::string_view field);
	/// Whether the index was built with other field definitions than the current ones.
	bool is_index_outdated();
	/// Gets the Message-IDs of all messages of a ticket, in the order they were imported.
	std::vector<std::string> get_message_ids(const Ticket &ticket);
	/// \return The Message-ID of the first message of the ticket, or an empty string if there are none.
	std::string get_first_message_id(const Ticket &ticket);

	bool has_message(std::string_view message_id);
	Message get_message(std::string_view message_id) const;

	ImportResult import(const Message &message);

	/// Rebuilds the index from the stored messages, keeping the numbers of existing tickets.
	/// Messages are indexed in the order they were imported. No hooks are run.
	ReindexResult reindex();

	/// Checks the messages and the index, without changing anything.
	FsckResult fsck();

	private:
	fs::path project_dir;
	fs::path base_dir;
	fs::path dbfile;
	fs::path maildir;
	fs::path hookdir;
	fs::path templatedir;

	bool no_hooks;

	SQLite3::database db;
	std::optional<std::size_t> id_length; ///< Cached length of short IDs.
	Config shared_config; ///< The configuration in config, which is shared when the instance is stored in git.
	Config local_config;  ///< The configuration in config.local, which overrides the shared configuration.
	Config config;        ///< The shared and the local configuration together.
	std::expected<FieldDefinitions, std::string> fields;

	void set_default_config();
	void load_fields();
	void merge_config();
	void install_templates() const;
	void init_index(Mode mode, bool update_index);
	void create_schema();
	void drop_schema();
	/// Rebuilds the index, which must be done inside a transaction.
	ReindexResult rebuild_index();

	fs::path message_path(std::string_view message_id) const;
	fs::path store(const Message &message) const;
	bool run_hook(std::string_view name, const fs::path &message_file, std::int64_t ticket = 0);

	/// Adds a message to the index, which must be done inside a transaction.
	/// \param new_ticket_number The number to give the ticket if the message creates one, or nothing to use the next free number.
	void index_message(const Message &message, std::int64_t received, std::optional<std::int64_t> new_ticket_number, ImportResult &result);

	bool ticket_exists(std::int64_t id);
	void load_fields(Ticket &ticket);
	void apply_metadata(std::int64_t ticket, const std::string &message_id, const Message &message, std::vector<std::string> &warnings);
	void set_field_values(std::int64_t ticket, const FieldDefinition &field, const std::vector<std::string> &values);
	void apply_field(std::int64_t ticket, const FieldDefinition &field, std::string_view value, std::vector<std::string> &warnings);
	void apply_set_field(std::int64_t ticket, const FieldDefinition &field, std::string_view changes, std::vector<std::string> &warnings);
	void set_versions(std::int64_t ticket, std::string_view versions, int status);
	void remove_versions(std::int64_t ticket, std::string_view versions, int status);
	void apply_links(std::int64_t ticket, LinkType type, std::string_view targets, bool add, std::vector<std::string> &warnings);
};

}

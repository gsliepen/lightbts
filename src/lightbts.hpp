#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2018 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <boost/filesystem.hpp>
#include <mimesis.hpp>
#include <set>
#include <string>
#include <vector>

#include "config.hpp"
#include "sqlite3.hpp"

namespace LightBTS {

using Mimesis::Message;
using std::set;
using std::string;
using std::vector;
namespace fs = boost::filesystem;

enum class Status {
	CLOSED,
	OPEN,
};

static const char *status_names[] = {
	"closed",
	"open",
};

enum class Severity {
	WISHLIST,
	MINOR,
	NORMAL,
	IMPORTANT,
	SERIOUS,
	CRITICAL,
	GRAVE,
};

static const char *severity_names[] = {
	"wishlist",
	"minor",
	"normal",
	"important",
	"serious",
	"critical",
	"grave",
};

enum class LinkType {
	RELATES,
	DUPLICATES,
	DEPENDS,
	BLOCKS,
};

static const char *link_names[] = {
	"relates",
	"duplicates",
	"depends",
	"blocks",
};

static const char *verbose_link_names[] = {
	"relates to",
	"duplicates",
	"depends on",
	"blocks",
};

static const char *reverse_link_names[] = {
	"related to by",
	"duplicated by",
	"depended on by",
	"blocked by",
};

bool is_valid_status(const string &name);
bool is_valid_severity(const string &name);

int status_index(const string &name);
int severity_index(const string &name);

bool is_valid_link_type(const string &name);
int link_type_index(const string &name);

class Ticket {
	friend class Instance;

	string id;
	string title;
	Status status;
	Severity severity;

	public:
	Ticket(const string &id, const string &title, Status status, Severity severity): id(id), title(title), status(status), severity(severity) {}

	const string &get_id() const { return id; };
	const string &get_title() const { return title; };

	Severity get_severity() const { return severity; }
	string get_severity_name() const { return severity_names[static_cast<int>(severity)]; }
	Status get_status() const { return status; }
	string get_status_name() const { return status_names[static_cast<int>(status)]; }
};


class Instance {
	fs::path base_dir;

	fs::path dbfile;
	fs::path maildir;
	fs::path hookdir;
	fs::path templatedir;

	string project;
	string admin;
	string emailaddress;
	string emailname;
	string smtphost;
	string webroot;
	string staticroot;

	bool quiet;
	bool no_hooks;
	bool no_email;
	bool respond_to_new;
	bool respond_to_reply;

	SQLite3::database db;
	Config config;

	void init(const fs::path &path, bool create = false);
	void init_index(const fs::path &path);

	fs::path store(const Message &msg);

	bool run_hook(const string &name, const fs::path &path, const string &id = {});
	void parse_versions(const string &id, const string &str, int status);
	void parse_tags(const string &id, const string &str);
	void parse_metadata(const string &id, const Message &msg);

	public:
	enum Flags {
		NONE = 0,
		INIT = 1 << 0,
	};

	Instance(const string &path, Flags flags = NONE);
	~Instance();

	string get_config(const string &section, const string &variable);
	void set_config(const string &section, const string &variable, const string &value);
	void save_config();
	string get_local_email_address();
	vector<Ticket> list(const vector<string> &args = {}, size_t len = 0);
	Ticket get_ticket_from_ticket_id(const string &id);
	Ticket get_ticket_from_message_id(const string &id);
	Ticket get_ticket(const string &id);
	Message get_message(const string &id);

	set<string> get_tags(const Ticket &ticket);
	string get_milestone(const Ticket &ticket);
	vector<string> get_message_ids(const Ticket &ticket);
	string get_first_message_id(const Ticket &ticket);

	bool import(const Message &msg);
};

}

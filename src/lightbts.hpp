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

#include <boost/filesystem.hpp>
#include <mimesis.hpp>
#include <set>
#include <string>
#include <vector>

#include "config.hpp"
#include "sqlite3.hpp"

namespace LightBTS {

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

class Ticket {
	friend class Instance;

	std::string id;
	std::string title;
	Status status;
	Severity severity;

	public:
	Ticket(const std::string &id, const std::string &title, Status status, Severity severity): id(id), title(title), status(status), severity(severity) {}

	const std::string &get_id() { return id; };
	const std::string &get_title() { return title; };

	std::vector<std::string> get_tags();
	std::string get_milestone();

	Severity get_severity() { return severity; }
	std::string get_severity_name() { return severity_names[static_cast<int>(severity)]; }
	Status get_status() { return status; }
	std::string get_status_name() { return status_names[static_cast<int>(status)]; }
};


class Instance {
	boost::filesystem::path base_dir;

	std::string dbfile;
	std::string maildir;
	std::string hookdir;
	std::string project;
	std::string admin;

	std::string emailaddress;
	std::string emailname;
	std::string emailtemplates;
	std::string smtphost;
	std::string webroot;
	std::string staticroot;
	std::string webtemplates;

	bool quiet;
	bool no_hooks;
	bool no_email;
	bool respond_to_new;
	bool respond_to_reply;

	SQLite3::database db;
	Config config;

	void init(const boost::filesystem::path &path);
	void init_index(const boost::filesystem::path &path);

	public:
	enum Flags {
		NONE = 0,
		INIT = 1 << 0,
	};

	Instance(const std::string &path, Flags flags = NONE);
	~Instance();

	std::string get_config(const std::string &section, const std::string &variable);
	void set_config(const std::string &section, const std::string &variable, const std::string &value);
	std::vector<Ticket> list();
	Ticket get_ticket(const std::string &id);
	Ticket get_ticket_from_message_id(const std::string &id);
	Mimesis::Message get_message(const std::string &id);

	std::set<std::string> get_tags(const Ticket &ticket);
	std::string get_milestone(const Ticket &ticket);
	std::vector<std::string> get_message_ids(const Ticket &ticket);
};

}

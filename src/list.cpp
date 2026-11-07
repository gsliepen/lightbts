/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <print>
#include <span>
#include <set>

#include "commands.hpp"
#include "pager.hpp"

namespace {

enum class What {
	tickets,
	tags,
	milestones,
};

LightBTS::Filter parse_filter(std::span<const std::string> words) {
	LightBTS::Filter filter;

	for (auto &word: words) {
		if (word == "all")
			filter.status.reset();
		else if (auto status = LightBTS::parse_status(word))
			filter.status = status;
		else if (auto severity = LightBTS::parse_severity(word))
			filter.severities.push_back(*severity);
		else
			filter.tags.push_back(word);
	}

	return filter;
}

}

int do_list(const Options &options, const Arguments &args) {
	auto bts = options.open_instance();

	std::span<const std::string> words = args;
	auto what = What::tickets;

	if (!words.empty()) {
		if (words.back() == "tags")
			what = What::tags;
		else if (words.back() == "milestones")
			what = What::milestones;

		if (what != What::tickets || words.back() == "bugs")
			words = words.first(words.size() - 1);
	}

	auto tickets = bts.list(parse_filter(words));
	Pager pager(options, bts.get_config("cli", "pager"));

	switch (what) {
	case What::tickets:
		for (auto &ticket: tickets)
			std::print(pager, "{:>6} {:6} {:9}  {}\n", ticket.id, name(ticket.status), name(ticket.severity), ticket.title);
		break;

	case What::tags: {
		std::set<std::string> tags;
		for (auto &ticket: tickets)
			for (auto &tag: bts.get_tags(ticket))
				tags.insert(tag);
		for (auto &tag: tags)
			std::print(pager, "{}\n", tag);
		break;
	}

	case What::milestones: {
		std::set<std::string> milestones;
		for (auto &ticket: tickets)
			if (auto milestone = bts.get_milestone(ticket); !milestone.empty())
				milestones.insert(milestone);
		for (auto &milestone: milestones)
			std::print(pager, "{}\n", milestone);
		break;
	}
	}

	return 0;
}

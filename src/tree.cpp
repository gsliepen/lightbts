/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <algorithm>
#include <cctype>
#include <iostream>
#include <print>
#include <ranges>
#include <set>
#include <span>

#include "commands.hpp"
#include "pager.hpp"
#include "strings.hpp"
#include "ticket-json.hpp"

namespace {

using LightBTS::Ticket;

/// Whether an argument can only be meant as a ticket, not as a selector.
bool is_ticket_reference(std::string_view argument) {
	return argument.starts_with('#') || argument.contains('@') || (!argument.empty() && std::ranges::all_of(argument, [](unsigned char c) { return std::isdigit(c); }));
}

std::vector<std::string> lines(std::string_view text) {
	std::vector<std::string> result;
	for (auto &&line: std::views::split(LightBTS::trim(text), '\n'))
		result.emplace_back(std::string_view(line.begin(), line.end()));
	return result;
}

class Tree {
	public:
	Tree(const Options &options, LightBTS::Instance &bts, LightBTS::LinkType type, std::span<const std::string> selectors):
		options(options),
		bts(bts),
		definitions(bts.get_fields()),
		graph(bts.get_link_graph(type)),
		sort(options.sort.empty() ? bts.get_config("cli", "sort") : options.sort)
	{
		// Selectors only hide tickets, so by default, tickets with any status are shown.
		std::vector<std::string> all_selectors = {"all"};
		all_selectors.insert(all_selectors.end(), selectors.begin(), selectors.end());
		for (auto &ticket: bts.list(LightBTS::parse_filter(definitions, all_selectors)))
			shown.insert(ticket.id);
	}

	/// The tickets that link to others but are not linked to, and that match the selectors.
	/// Tickets that can only be reached through a cycle get the one with the lowest number as an extra root.
	std::vector<Ticket> roots() {
		std::set<std::int64_t> linked;
		for (auto &[ticket, others]: graph)
			linked.insert(others.begin(), others.end());

		std::vector<std::int64_t> root_ids;
		for (auto &[ticket, others]: graph)
			if (!linked.contains(ticket))
				root_ids.push_back(ticket);

		std::set<std::int64_t> reached;
		for (auto root: root_ids)
			reach(root, reached);
		for (auto &[ticket, others]: graph) {
			if (!reached.contains(ticket)) {
				root_ids.push_back(ticket);
				reach(ticket, reached);
			}
		}

		std::vector<Ticket> result;
		for (auto root: root_ids)
			if (shown.contains(root))
				result.push_back(bts.get_ticket(root));
		LightBTS::sort_tickets(result, definitions, sort);
		return result;
	}

	void print_text(FILE *out, const Ticket &ticket, int depth = 0) {
		std::string indent(4 * depth, ' ');
		if (std::ranges::contains(path, ticket.id)) {
			std::print(out, "{}{} (see above)\n", indent, header(ticket));
			return;
		}

		std::print(out, "{}{}\n", indent, header(ticket));
		if (shows_description(ticket)) {
			auto description = lines(bts.get_description(ticket));
			for (auto &line: description)
				std::print(out, "{}\n", line.empty() ? "" : indent + "    " + line);
			if (!description.empty())
				std::print(out, "\n");
		}

		path.push_back(ticket.id);
		for (auto &child: children(ticket, depth))
			print_text(out, child, depth + 1);
		path.pop_back();
	}

	void print_markdown(FILE *out, const Ticket &ticket, int depth = 0) {
		if (std::ranges::contains(path, ticket.id)) {
			std::print(out, "- {} (see above)\n\n", ticket.title);
			return;
		}

		std::print(out, "{} {}\n\n", std::string(std::min(depth + 1, 6), '#'), ticket.title);
		std::print(out, "*{}*\n\n", summary(ticket));
		if (shows_description(ticket)) {
			auto description = bts.get_description(ticket);
			if (auto text = LightBTS::trim(description); !text.empty())
				std::print(out, "{}\n\n", text);
		}

		path.push_back(ticket.id);
		for (auto &child: children(ticket, depth))
			print_markdown(out, child, depth + 1);
		path.pop_back();
	}

	LightBTS::Json to_json(const Ticket &ticket, int depth = 0) {
		auto json = LightBTS::ticket_to_json(bts, ticket);
		if (std::ranges::contains(path, ticket.id)) {
			json.set("see_above", true);
			return json;
		}

		json.set("description", bts.get_description(ticket));

		path.push_back(ticket.id);
		LightBTS::Json json_children = LightBTS::Json::Array{};
		for (auto &child: children(ticket, depth))
			json_children.push_back(to_json(child, depth + 1));
		path.pop_back();

		json.set("children", json_children);
		return json;
	}

	private:
	const Options &options;
	LightBTS::Instance &bts;
	const LightBTS::FieldDefinitions &definitions;
	std::map<std::int64_t, std::vector<std::int64_t>> graph;
	std::string sort;
	std::set<std::int64_t> shown;  ///< The tickets that match the selectors.
	std::vector<std::int64_t> path; ///< The tickets above the one that is being shown, to detect cycles.

	/// Adds a ticket and all tickets that can be reached from it to reached.
	void reach(std::int64_t ticket, std::set<std::int64_t> &reached) const {
		if (!reached.insert(ticket).second)
			return;
		if (auto others = graph.find(ticket); others != graph.end())
			for (auto other: others->second)
				reach(other, reached);
	}

	bool is_done(const Ticket &ticket) const {
		return definitions.status.category(ticket.get("status")) == LightBTS::StatusCategory::done;
	}

	bool shows_description(const Ticket &ticket) const {
		return !is_done(ticket) || options.verbose;
	}

	/// The children of a ticket that are shown at this depth, sorted.
	std::vector<Ticket> children(const Ticket &ticket, int depth) {
		std::vector<Ticket> result;
		if (options.depth >= 0 && depth >= options.depth)
			return result;
		if (auto others = graph.find(ticket.id); others != graph.end())
			for (auto other: others->second)
				if (shown.contains(other))
					result.push_back(bts.get_ticket(other));
		LightBTS::sort_tickets(result, definitions, sort);
		return result;
	}

	/// How many of the tickets this ticket links to are done, like "2/5 done", or nothing if it doesn't link to any.
	std::string progress(const Ticket &ticket) {
		auto others = graph.find(ticket.id);
		if (others == graph.end())
			return {};
		auto done = std::ranges::count_if(others->second, [&](std::int64_t other) { return is_done(bts.get_ticket(other)); });
		return std::format("{}/{} done", done, others->second.size());
	}

	std::string header(const Ticket &ticket) {
		auto text = std::format("#{} {} [{}] {}", ticket.id, bts.short_id(ticket), ticket.get("status"), ticket.title);
		if (auto done = progress(ticket); !done.empty())
			text += std::format(" ({})", done);
		return text;
	}

	std::string summary(const Ticket &ticket) {
		auto text = std::format("#{} {}, {}", ticket.id, bts.short_id(ticket), ticket.get("status"));
		if (auto done = progress(ticket); !done.empty())
			text += ", " + done;
		return text;
	}
};

}

int do_tree(const Options &options, const Arguments &args) {
	auto bts = options.open_instance();

	if (options.links.size() > 1) {
		std::println(std::cerr, "Only one link type can be followed.");
		return 1;
	}

	auto type_name = options.links.empty() ? std::string("depends") : options.links.front();
	auto type = LightBTS::parse_link_type(type_name);
	if (!type) {
		std::println(std::cerr, "Invalid link type \"{}\"", type_name);
		return 1;
	}

	// The first argument is the ticket to start from, if it refers to one.
	std::span<const std::string> selectors = args;
	std::optional<Ticket> start;
	if (!args.empty()) {
		if (is_ticket_reference(args[0])) {
			start = bts.get_ticket(args[0]);
		} else {
			try {
				start = bts.get_ticket(args[0]);
			} catch (const std::runtime_error &) {
			}
		}
		if (start)
			selectors = selectors.subspan(1);
	}

	Tree tree(options, bts, *type, selectors);
	auto roots = start ? std::vector<Ticket>{*start} : tree.roots();
	Pager pager(options, bts.get_config("cli", "pager"));

	// A single tree is an object, otherwise there is an array of trees.
	if (options.json) {
		LightBTS::Json json = LightBTS::Json::Array{};
		for (auto &root: roots)
			json.push_back(tree.to_json(root));
		std::print(pager, "{}\n", start ? tree.to_json(*start).dump() : json.dump());
		return 0;
	}

	for (std::size_t i = 0; i < roots.size(); i++) {
		if (options.markdown) {
			tree.print_markdown(pager, roots[i]);
		} else {
			if (i)
				std::print(pager, "\n");
			tree.print_text(pager, roots[i]);
		}
	}

	return 0;
}

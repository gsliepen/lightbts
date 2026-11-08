/* LightBTS -- a lightweight issue tracking system
   Copyright © 2018-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <iostream>
#include <print>
#include <ranges>
#include <span>

#include "commands.hpp"
#include "compose.hpp"

namespace {

std::string join_words(const Arguments &args, std::size_t first) {
	return args | std::views::drop(first) | std::views::join_with(' ') | std::ranges::to<std::string>();
}

bool check_argument_count(const Arguments &args, std::size_t minimum, std::size_t maximum) {
	if (args.size() >= minimum && args.size() <= maximum)
		return true;
	std::println(std::cerr, "Invalid number of arguments");
	return false;
}

constexpr auto unlimited = static_cast<std::size_t>(-1);

int do_action(const Options &options, LightBTS::Instance &bts, const std::string &id, const std::string &pseudo_headers) {
	auto ticket = bts.get_ticket(id);
	auto first_message_id = bts.get_first_message_id(ticket);

	if (first_message_id.empty()) {
		std::println(std::cerr, "No message ID found for ticket {}!", ticket.id);
		return 1;
	}

	auto message = compose_message(bts, options, {
		.subject = ticket.title,
		.pseudo_headers = pseudo_headers,
		.in_reply_to = first_message_id,
		.editor_hint = "action",
		.is_action = true,
	});

	if (!message) {
		std::println(std::cerr, "Aborting action.");
		return 1;
	}

	auto result = import_composed(bts, *message);
	if (result.kind != LightBTS::ImportResult::Kind::appended) {
		std::println(std::cerr, "Action was not recorded for ticket {}", ticket.id);
		return 1;
	}

	std::println(std::cerr, "Action recorded for bug number {}", ticket.id);
	return 0;
}

int do_action(const Options &options, const std::string &id, const std::string &pseudo_headers) {
	auto bts = options.open_instance();
	return do_action(options, bts, id, pseudo_headers);
}

std::string version_headers(const Options &options, std::string_view key) {
	std::string headers;
	for (auto &version: options.versions)
		headers += pseudo_header(key, version);
	return headers;
}

std::string tag_headers(const LightBTS::Instance &bts, const Options &options) {
	std::string headers;
	for (auto &tag: options.tags)
		headers += pseudo_header(bts.get_fields().get("tags").header_name(), "+" + tag);
	return headers;
}

int do_status_action(const Options &options, const Arguments &args, bool close) {
	if (!check_argument_count(args, 1, 1))
		return 1;

	auto bts = options.open_instance();
	auto &status = bts.get_fields().status;
	auto headers = pseudo_header("Status", close ? status.close : status.reopen) + version_headers(options, close ? "Fixed" : "Found") + tag_headers(bts, options);
	return do_action(options, bts, args[0], headers);
}

int set_field(const Options &options, const std::string &id, std::string_view field_name, std::span<const std::string> words) {
	auto bts = options.open_instance();
	auto &field = bts.get_fields().get(field_name);

	using enum LightBTS::FieldType;
	bool single_word = field.type == enumeration || field.type == date || field.type == number;
	if (words.empty() || (single_word && words.size() > 1)) {
		std::println(std::cerr, "Invalid number of arguments");
		return 1;
	}

	auto value = words | std::views::join_with(' ') | std::ranges::to<std::string>();
	return do_action(options, bts, id, pseudo_header(field.header_name(), value));
}

int unset_field(const Options &options, const std::string &id, std::string_view field_name) {
	auto bts = options.open_instance();
	auto &field = bts.get_fields().get(field_name);
	return do_action(options, bts, id, pseudo_header(field.header_name(), field.has_multiple_values() ? "=" : "-"));
}

/// A shortcut for lbts set, like "lbts severity 1 minor".
int do_field_shortcut(const Options &options, const Arguments &args, std::string_view field_name) {
	if (!check_argument_count(args, 2, unlimited))
		return 1;
	return set_field(options, args[0], field_name, std::span(args).subspan(1));
}

/// An action where all remaining arguments form one value, like "lbts retitle 1 New title".
int do_words_action(const Options &options, const Arguments &args, std::string_view key) {
	if (!check_argument_count(args, 2, unlimited))
		return 1;
	return do_action(options, args[0], pseudo_header(key, join_words(args, 1)));
}

/// An action with a pseudo-header for each remaining argument, like "lbts found 1 1.0 1.1".
int do_multiple_action(const Options &options, const Arguments &args, std::string_view key) {
	if (!check_argument_count(args, 2, unlimited))
		return 1;

	std::string headers;
	for (auto &value: args | std::views::drop(1))
		headers += pseudo_header(key, value);
	return do_action(options, args[0], headers);
}

int do_link_action(const Options &options, const Arguments &args, std::string_view prefix) {
	if (!check_argument_count(args, 3, 3))
		return 1;

	if (!LightBTS::parse_link_type(args[1])) {
		std::println(std::cerr, "Invalid link type");
		return 1;
	}

	return do_action(options, args[0], pseudo_header(std::string(prefix) + args[1], args[2]));
}

}

int do_close(const Options &options, const Arguments &args) {
	return do_status_action(options, args, true);
}

int do_reopen(const Options &options, const Arguments &args) {
	return do_status_action(options, args, false);
}

int do_retitle(const Options &options, const Arguments &args) {
	return do_words_action(options, args, "Title");
}

int do_found(const Options &options, const Arguments &args) {
	return do_multiple_action(options, args, "Found");
}

int do_notfound(const Options &options, const Arguments &args) {
	return do_multiple_action(options, args, "Notfound");
}

int do_fixed(const Options &options, const Arguments &args) {
	return do_multiple_action(options, args, "Fixed");
}

int do_notfixed(const Options &options, const Arguments &args) {
	return do_multiple_action(options, args, "Notfixed");
}

int do_link(const Options &options, const Arguments &args) {
	return do_link_action(options, args, "");
}

int do_unlink(const Options &options, const Arguments &args) {
	return do_link_action(options, args, "Un");
}

int do_set(const Options &options, const Arguments &args) {
	if (!check_argument_count(args, 3, unlimited))
		return 1;
	return set_field(options, args[0], args[1], std::span(args).subspan(2));
}

int do_unset(const Options &options, const Arguments &args) {
	if (!check_argument_count(args, 2, 2))
		return 1;
	return unset_field(options, args[0], args[1]);
}

int do_severity(const Options &options, const Arguments &args) {
	return do_field_shortcut(options, args, "severity");
}

int do_tags(const Options &options, const Arguments &args) {
	return do_field_shortcut(options, args, "tags");
}

int do_owner(const Options &options, const Arguments &args) {
	return do_field_shortcut(options, args, "owner");
}

int do_noowner(const Options &options, const Arguments &args) {
	if (!check_argument_count(args, 1, 1))
		return 1;
	return unset_field(options, args[0], "owner");
}

int do_deadline(const Options &options, const Arguments &args) {
	return do_field_shortcut(options, args, "deadline");
}

int do_milestone(const Options &options, const Arguments &args) {
	return do_field_shortcut(options, args, "milestone");
}

int do_progress(const Options &options, const Arguments &args) {
	return do_field_shortcut(options, args, "progress");
}

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "compose.hpp"

#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <print>
#include <stdexcept>

#include "edit.hpp"
#include "version.hpp"

static std::string read_stdin() {
	std::string text{std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
	if (std::cin.bad())
		throw std::runtime_error("Error while reading input");
	return text;
}

std::string pseudo_header(std::string_view key, std::string_view value) {
	return std::format("{}: {}\n", key, value);
}

std::string option_headers(const LightBTS::Instance &bts, const Options &options) {
	auto &definitions = bts.get_fields();
	std::string headers;

	for (auto &assignment: options.fields) {
		auto equals = assignment.find('=');
		if (equals == assignment.npos)
			throw std::runtime_error(std::format("Invalid field assignment \"{}\", expected <field>=<value>", assignment));
		headers += pseudo_header(definitions.get(assignment.substr(0, equals)).header_name(), assignment.substr(equals + 1));
	}

	if (!options.severity.empty())
		headers += pseudo_header(definitions.get("severity").header_name(), options.severity);
	for (auto &tag: options.tags)
		headers += pseudo_header(definitions.get("tags").header_name(), tag);
	for (auto &version: options.versions)
		headers += pseudo_header("Version", version);
	return headers;
}

static std::string join_body(const std::string &pseudo_headers, std::string text) {
	if (!text.empty() && !text.ends_with('\n'))
		text.push_back('\n');
	if (pseudo_headers.empty())
		return text;
	if (text.empty())
		return pseudo_headers;
	return pseudo_headers + "\n" + text;
}

static void attach_files(LightBTS::Message &message, const std::vector<std::string> &attachments) {
	for (auto &filename: attachments) {
		std::ifstream file(filename, std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error(std::format("Could not open attachment {}", filename));
		message.attach(file, "application/octet-stream", std::filesystem::path(filename).filename().string());
	}
}

std::optional<LightBTS::Message> compose_message(LightBTS::Instance &bts, const Options &options, const Composition &composition) {
	LightBTS::Message message;
	message.set_crlf(false);
	message.set_header_value("From", bts.get_local_email_address());
	message.set_header_value("Subject", composition.subject);

	if (options.message) {
		message.set_plain(join_body(composition.pseudo_headers, *options.message));
	} else if (!options.batch) {
		message.set_plain(join_body(composition.pseudo_headers, composition.editor_text));
		if (!edit_interactive(bts, message, composition.editor_hint, !composition.is_action))
			return std::nullopt;
	} else if (composition.is_action) {
		message.set_plain(composition.pseudo_headers);
	} else {
		message.set_plain(join_body(composition.pseudo_headers, read_stdin()));
	}

	attach_files(message, options.attachments);

	message.set_header("To", "LightBTS");
	message.set_header("User-Agent", std::format("LightBTS/{}", lightbts_version));
	if (!composition.in_reply_to.empty())
		message.set_header("In-Reply-To", std::format("<{}>", composition.in_reply_to));

	return message;
}

LightBTS::ImportResult import_composed(LightBTS::Instance &bts, const LightBTS::Message &message) {
	auto result = bts.import(message);
	for (auto &warning: result.warnings)
		std::println(std::cerr, "Warning: {}", warning);
	return result;
}

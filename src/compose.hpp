#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <optional>
#include <string>

#include "lightbts.hpp"
#include "options.hpp"

/// What lbts create, reply and the actions put in a new message.
struct Composition {
	std::string subject;
	std::string pseudo_headers; ///< Lines of the form "Key: value".
	std::string in_reply_to;    ///< The Message-ID of the parent message, or empty for a new ticket.
	std::string editor_text;    ///< Text to add after the pseudo-headers when invoking an editor.
	std::string editor_hint;    ///< Part of the name of the temporary file for the editor.
	bool is_action = false;     ///< Actions don't need any text, and don't read it from stdin.
};

/// Formats a pseudo-header line, including the trailing newline.
std::string pseudo_header(std::string_view key, std::string_view value);

/// The pseudo-headers for the -S, -V and -T options.
std::string option_headers(const Options &options);

/// Creates a message, getting its text from the -m option, an editor or stdin.
/// \return The message, or nothing if the user aborted editing it.
std::optional<LightBTS::Message> compose_message(LightBTS::Instance &bts, const Options &options, const Composition &composition);

/// Imports a composed message, and prints any warnings about its pseudo-headers.
LightBTS::ImportResult import_composed(LightBTS::Instance &bts, const LightBTS::Message &message);

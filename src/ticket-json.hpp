#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <string_view>

#include "json.hpp"
#include "lightbts.hpp"

namespace LightBTS {

/// The values of the fields of a ticket, in the order they are defined. Set fields are arrays,
/// number fields are integers, and other fields are strings. Unset fields are left out.
Json fields_to_json(const FieldDefinitions &definitions, const Ticket &ticket);

/// The ID, number, title and fields of a ticket.
Json ticket_to_json(Instance &bts, const Ticket &ticket);

/// Everything about a ticket: the summary of ticket_to_json(), plus its submitter, date, versions, links and messages.
/// \param with_text Whether to include the text of each message.
Json details_to_json(Instance &bts, const TicketDetails &details, bool with_text);

Json message_to_json(const Message &message, std::string_view message_id, bool with_text);

Json field_definitions_to_json(const FieldDefinitions &definitions);

}

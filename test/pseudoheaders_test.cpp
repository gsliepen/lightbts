/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "check.hpp"
#include "pseudoheaders.hpp"

using LightBTS::parse_pseudo_headers;
using LightBTS::strip_pseudo_headers;
using LightBTS::PseudoHeader;
using Headers = std::vector<PseudoHeader>;

int main() {
	CHECK(parse_pseudo_headers("").empty());
	CHECK(parse_pseudo_headers("Just some text.\n").empty());
	CHECK(parse_pseudo_headers("Hello there: how are you?\n").empty());

	CHECK(parse_pseudo_headers("Status: closed\n") == Headers{{"status", "closed"}});
	CHECK(parse_pseudo_headers("Status: closed") == Headers{{"status", "closed"}});

	// Line endings, missing or extra whitespace, and case
	CHECK(parse_pseudo_headers("Status: closed\r\nSeverity:minor\r\n") == (Headers{{"status", "closed"}, {"severity", "minor"}}));
	CHECK(parse_pseudo_headers("  TAGS  :   foo bar  \n") == Headers{{"tags", "foo bar"}});

	// Parsing stops at the first empty line or at text, and skips leading empty lines
	CHECK(parse_pseudo_headers("\n\nStatus: open\n\nTitle: not a header\n") == Headers{{"status", "open"}});
	CHECK(parse_pseudo_headers("Status: open\nSome text: with a colon\n") == Headers{{"status", "open"}});

	// Empty values are kept, so they can be reported
	CHECK(parse_pseudo_headers("Owner:\n") == Headers{{"owner", ""}});

	// The text after the pseudo-headers
	CHECK(strip_pseudo_headers("Status: closed\n\nThe text.\n") == "The text.\n");
	CHECK(strip_pseudo_headers("Status: closed\nThe text.\n") == "The text.\n");
	CHECK(strip_pseudo_headers("Status: closed\n") == "");
	CHECK(strip_pseudo_headers("\n\nJust text.\n\nMore: text\n") == "Just text.\n\nMore: text\n");
	CHECK(strip_pseudo_headers("Status: open\n\n\nTwo empty lines.\n") == "\nTwo empty lines.\n");
	CHECK(strip_pseudo_headers("") == "");

	return check_result();
}

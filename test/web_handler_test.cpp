/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <filesystem>
#include <unistd.h>

#include "check.hpp"
#include "handler.hpp"
#include "lightbts.hpp"

namespace fs = std::filesystem;
using namespace LightBTS;
using namespace LightBTS::Web;

static Message make_message(std::string_view subject, std::string_view message_id, std::string_view text, std::string_view in_reply_to = {}) {
	Message message;
	message.set_crlf(false);
	message.set_header("From", "Test <test@example.org>");
	message.set_header("Subject", subject);
	message.set_header("Message-ID", message_id);
	if (!in_reply_to.empty())
		message.set_header("In-Reply-To", in_reply_to);
	message.set_plain(text);
	return message;
}

static Response get(const Handler &handler, std::string path, std::multimap<std::string, std::string> query = {}, std::map<std::string, std::string> headers = {}) {
	return handler.handle({.path = std::move(path), .query = std::move(query), .headers = std::move(headers)});
}

static std::string header(const Response &response, std::string_view name) {
	for (auto &[key, value]: response.headers)
		if (key == name)
			return value;
	return {};
}

int main() {
	auto dir = fs::temp_directory_path() / ("lightbts-web-test-" + std::to_string(getpid()));
	fs::create_directories(dir);

	{
		Instance bts(dir, Instance::Mode::create);
		bts.import(make_message("First <script>alert(1)</script>", "<1@test>", "Tags: foo\nSeverity: important\n\nThe first bug.\n"));
		bts.import(make_message("Second bug", "<2@test>", "Status: closed\n\nThe second bug.\n"));
		bts.import(make_message("Re: First", "<3@test>", "Blocks: 2\n\nA reply.\n", "<1@test>"));
	}

	Handler handler(dir);

	// The ticket list shows open tickets by default, and escapes HTML
	auto list = get(handler, "/");
	CHECK(list.status == 200);
	CHECK(list.body.contains("<!DOCTYPE html>"));
	CHECK(list.body.contains("First &lt;script&gt;"));
	CHECK(!list.body.contains("<script>alert"));
	CHECK(!list.body.contains("Second bug"));
	CHECK(header(list, "Vary") == "HX-Target");
	CHECK(header(list, "Content-Security-Policy").contains("script-src 'self'"));
	CHECK(header(list, "X-Content-Type-Options") == "nosniff");

	// Filters
	CHECK(get(handler, "/", {{"status", "closed"}}).body.contains("Second bug"));
	CHECK(!get(handler, "/", {{"status", "closed"}}).body.contains("First &lt;"));
	CHECK(get(handler, "/", {{"status", "all"}, {"tag", "foo"}}).body.contains("First &lt;"));
	CHECK(!get(handler, "/", {{"status", "all"}, {"tag", "foo"}}).body.contains("Second bug"));
	CHECK(get(handler, "/", {{"severity", "important"}}).body.contains("First &lt;"));
	CHECK(get(handler, "/", {{"status", "bogus"}}).status == 400);
	CHECK(get(handler, "/", {{"severity", "bogus"}}).status == 400);

	// Searching, where the last word is a prefix
	auto search = get(handler, "/", {{"status", "all"}, {"q", "seco"}});
	CHECK(search.body.contains("Second bug"));
	CHECK(!search.body.contains("First &lt;"));
	CHECK(search.body.contains("<input type=\"search\" name=\"q\" value=\"seco\">"));
	CHECK(get(handler, "/", {{"q", "reply"}}).body.contains("First &lt;"));
	CHECK(get(handler, "/", {{"q", "\"unterminated"}}).status == 400);

	// htmx only gets the table rows
	auto rows = get(handler, "/", {{"status", "all"}}, {{"hx-request", "true"}, {"hx-target", "ticket-rows"}});
	CHECK(rows.status == 200);
	CHECK(!rows.body.contains("<html"));
	CHECK(rows.body.contains("<tr class=\"status-closed"));

	// Boosted navigation gets full pages
	CHECK(get(handler, "/", {}, {{"hx-request", "true"}, {"hx-boosted", "true"}}).body.contains("<html"));

	// Ticket pages
	auto ticket = get(handler, "/ticket/1");
	CHECK(ticket.status == 200);
	CHECK(ticket.body.contains("The first bug."));
	CHECK(ticket.body.contains("A reply."));
	CHECK(ticket.body.contains("<section class=\"description\">\n\t<pre class=\"body\">The first bug.\n</pre>"));
	CHECK(ticket.body.contains("Blocks"));
	CHECK(ticket.body.contains("href=\"/ticket/2\""));
	CHECK(ticket.body.contains("message?id=1%40test"));
	CHECK(get(handler, "/ticket/2").body.contains("Blocked by"));
	CHECK(get(handler, "/ticket/2").body.contains("<h3>Depends on</h3>"));
	CHECK(get(handler, "/ticket/2").body.contains("<td class=\"title\"><a href=\"/ticket/1\">First &lt;script&gt;"));
	CHECK(!get(handler, "/ticket/1").body.contains("<h3>Depends on</h3>"));
	auto first_id = Instance(dir, Instance::Mode::read_only).get_ticket(1).hash;
	CHECK(ticket.body.contains(std::format("<span class=\"ticket-id\">{}</span>", first_id.substr(0, 7))));
	CHECK(get(handler, "/ticket/" + first_id.substr(0, 8)).body.contains("The first bug."));
	CHECK(get(handler, "/ticket/ffffffff").status == 404);
	CHECK(get(handler, "/ticket/99").status == 404);
	CHECK(get(handler, "/ticket/abc").status == 404);
	CHECK(get(handler, "/ticket/").status == 404);

	// Raw messages
	auto message = get(handler, "/message", {{"id", "1@test"}});
	CHECK(message.status == 200);
	CHECK(message.content_type.starts_with("text/plain"));
	CHECK(message.body.contains("Message-ID: <1@test>"));
	CHECK(get(handler, "/message", {{"id", "<2@test>"}}).status == 200);
	CHECK(get(handler, "/message", {{"id", "unknown@test"}}).status == 404);
	CHECK(get(handler, "/message").status == 400);

	// Static files and unknown pages
	CHECK(get(handler, "/static/lightbts.css").content_type.starts_with("text/css"));
	CHECK(get(handler, "/nonexistent").status == 404);

	// Only GET and HEAD are allowed
	auto post = handler.handle({.method = "POST"});
	CHECK(post.status == 405);
	CHECK(header(post, "Allow") == "GET, HEAD");
	CHECK(handler.handle({.method = "HEAD"}).status == 200);

	// URLs are relative to the script name or the configured root
	CHECK(handler.handle({.script_name = "/cgi-bin/lbts"}).body.contains("href=\"/cgi-bin/lbts/ticket/1\""));
	{
		Instance bts(dir);
		bts.set_config("web", "root", "/bugs");
		bts.set_config("web", "static-root", "https://static.example.org/lbts");
		bts.save_config();
	}
	auto configured = get(handler, "/");
	CHECK(configured.body.contains("href=\"/bugs/ticket/1\""));
	CHECK(configured.body.contains("href=\"https://static.example.org/lbts/lightbts.css\""));
	CHECK(header(configured, "Content-Security-Policy").contains("style-src 'self' https://static.example.org;"));

	// Custom fields are shown and can be filtered on
	{
		Instance bts(dir);
		bts.set_config("field.priority", "type", "enum");
		bts.set_config("field.priority", "values", "low, very-high");
		bts.set_config("field.priority", "ordered", "true");
		bts.set_config("web", "columns", "priority, status");
		bts.save_config();
	}
	{
		Instance bts(dir);
		bts.import(make_message("Fourth bug", "<4@test>", "Priority: very-high\n\nThe fourth bug.\n"));
	}
	auto custom = get(handler, "/");
	CHECK(custom.body.contains("<th class=\"field field-priority\">Priority</th><th class=\"field field-status\">Status</th>"));
	CHECK(custom.body.contains("<td class=\"field field-priority\">Very high</td>"));
	CHECK(custom.body.contains("class=\"status-open severity-normal priority-very-high category-to-do\""));
	CHECK(custom.body.contains("<select name=\"priority\">"));
	CHECK(!custom.body.contains("optgroup"));
	CHECK(!custom.body.contains("field-severity"));
	CHECK(get(handler, "/", {{"priority", "very-high"}}).body.contains("Fourth bug"));
	CHECK(!get(handler, "/", {{"priority", "very-high"}}).body.contains("First &lt;"));
	CHECK(get(handler, "/", {{"priority", "low"}}).body.contains("No tickets found."));
	CHECK(get(handler, "/", {{"priority", "bogus"}}).status == 400);
	CHECK(get(handler, "/", {{"status", "status=closed"}}).body.contains("Second bug"));
	CHECK(get(handler, "/", {{"status", "in-progress"}}).body.contains("No tickets found."));

	auto fourth = get(handler, "/ticket/3");
	CHECK(fourth.body.contains("<dt>Priority</dt><dd class=\"priority-very-high\"><a href=\"/bugs/?status=all&amp;priority=very-high\">Very high</a> </dd>"));
	CHECK(fourth.body.contains("<a href=\"/bugs/?status=status%3Dopen\">Open</a>"));
	CHECK(get(handler, "/ticket/1").body.contains("<a class=\"tag\" href=\"/bugs/?status=all&amp;tags=foo\">foo</a>"));

	{
		Instance bts(dir);
		bts.set_config("web", "columns", "nonexistent");
		bts.save_config();
	}
	CHECK(get(handler, "/").status == 500);

	// A missing instance results in an error, not a crash
	CHECK(Handler(fs::temp_directory_path() / ("lightbts-missing-" + std::to_string(getpid()))).handle({}).status == 500);

	// Query string parsing
	auto query = parse_query("a=1&b=two+words&c=%41%2f&a=2&empty&bad=%zz");
	CHECK(query.count("a") == 2);
	CHECK(query.find("b")->second == "two words");
	CHECK(query.find("c")->second == "A/");
	CHECK(query.find("empty")->second.empty());
	CHECK(query.find("bad")->second == "%zz");
	CHECK(url_encode("a b@c/<d>") == "a%20b%40c%2F%3Cd%3E");

	fs::remove_all(dir);
	return check_result();
}

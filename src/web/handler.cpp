/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include "handler.hpp"

#include <cctype>
#include <charconv>
#include <chrono>
#include <algorithm>
#include <format>
#include <iostream>
#include <kainjow/mustache.hpp>
#include <print>
#include <ranges>
#include <stdexcept>

#include "embedded.hpp"
#include "lightbts.hpp"
#include "strings.hpp"
#include "version.hpp"

#include "assets.inl"

namespace LightBTS::Web {

namespace km = kainjow::mustache;

namespace {

constexpr bool has_htmx = !embedded_assets.empty();

struct NotFound: std::runtime_error {
	using std::runtime_error::runtime_error;
};

struct BadRequest: std::runtime_error {
	using std::runtime_error::runtime_error;
};

/// Everything a page needs to know about the current request.
struct Context {
	Instance &bts;
	const Request &request;
	std::string root;        ///< The URL of the ticket list, ending with a slash.
	std::string static_root; ///< The URL of the static files, ending with a slash.
};

std::string html_escape(std::string_view text) {
	std::string result;
	for (char c: text) {
		switch (c) {
		case '&': result += "&amp;"; break;
		case '<': result += "&lt;"; break;
		case '>': result += "&gt;"; break;
		case '"': result += "&quot;"; break;
		case '\'': result += "&#39;"; break;
		default: result += c;
		}
	}
	return result;
}

std::string with_trailing_slash(std::string url) {
	if (!url.ends_with('/'))
		url.push_back('/');
	return url;
}

std::string format_unix_time(std::int64_t seconds) {
	return std::format("{:%Y-%m-%d %H:%M} UTC", std::chrono::sys_seconds{std::chrono::seconds{seconds}});
}

std::string join(const auto &range) {
	return range | std::views::join_with(' ') | std::ranges::to<std::string>();
}

/// Sets a value only if it is not empty, since an empty string still counts as true in a mustache section.
void set_if_not_empty(km::data &data, const std::string &name, const std::string &value) {
	if (!value.empty())
		data.set(name, value);
}

/// The origin of a URL like https://static.example.org/lightbts/, or an empty string for relative URLs.
std::string origin_of(std::string_view url) {
	auto scheme_end = url.find("//");
	if (scheme_end == url.npos || (scheme_end != 0 && url[scheme_end - 1] != ':'))
		return {};
	return std::string(url.substr(0, url.find('/', scheme_end + 2)));
}

std::string content_security_policy(const Context &context) {
	auto static_origin = origin_of(context.static_root);
	auto self = static_origin.empty() ? std::string("'self'") : "'self' " + static_origin;
	return std::format("default-src 'none'; script-src {0}; style-src {0}; img-src 'self' data:; connect-src 'self'; form-action 'self'; base-uri 'none'; frame-ancestors 'none'", self);
}

std::string render(const Context &context, std::string_view name, km::data &data) {
	km::mustache tmpl{context.bts.get_template(name)};
	if (!tmpl.is_valid())
		throw std::runtime_error(std::format("Error in template {}: {}", name, tmpl.error_message()));

	for (std::string partial: {"ticket-rows.html", "message.html"})
		data.set(partial, km::partial{[&bts = context.bts, partial] { return bts.get_template(partial); }});

	data.set("root", context.root);
	data.set("static_root", context.static_root);
	return tmpl.render(data);
}

Response html_response(const Context &context, std::string body) {
	return {
		.headers = {{"Content-Security-Policy", content_security_policy(context)}},
		.body = std::move(body),
	};
}

Response page(const Context &context, const std::string &title, std::string_view content_template, km::data &data) {
	auto project = context.bts.get_config("core", "project");
	if (project.empty())
		project = "LightBTS";

	km::data layout;
	layout.set("title", title.empty() ? project : std::format("{} - {}", title, project));
	layout.set("project", project);
	layout.set("version", std::string(lightbts_version));
	layout.set("htmx", has_htmx);
	layout.set("content", render(context, content_template, data));
	return html_response(context, render(context, "layout.html", layout));
}

Response error_page(int status, std::string_view message) {
	auto title = std::format("{} {}", status, status_text(status));
	return {
		.status = status,
		.headers = {{"Content-Security-Policy", "default-src 'none'"}},
		.body = std::format("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>{0}</title>\n</head>\n<body>\n<h1>{0}</h1>\n<p>{1}</p>\n</body>\n</html>\n", title, html_escape(message)),
	};
}

/// Shows a value of an enum field like "in-progress" as "In progress".
std::string display_value(const FieldDefinition &field, std::string_view value) {
	if (field.type != FieldType::enumeration)
		return field.format(value);
	std::string result = capitalize(value);
	std::ranges::replace(result, '-', ' ');
	std::ranges::replace(result, '_', ' ');
	return result;
}

/// The URL of the ticket list, showing tickets that have a value in a field.
std::string filter_url(const Context &context, const FieldDefinition &field, std::string_view value) {
	if (field.name == "status")
		return std::format("{}?status={}", context.root, url_encode(std::format("status={}", value)));
	return std::format("{}?status=all&{}={}", context.root, url_encode(field.name), url_encode(value));
}

/// CSS classes for the values of enum fields and the status category, like "status-closed severity-normal category-done".
std::string css_classes(const FieldDefinitions &definitions, const Ticket &ticket) {
	std::string classes;
	for (auto &field: definitions.fields)
		if (field.type == FieldType::enumeration)
			if (auto value = ticket.get(field.name); !value.empty())
				classes += std::format("{}-{} ", field.name, value);
	auto category = definitions.status.category(ticket.get("status"));
	return classes + std::format("category-{}", status_category_names[std::to_underlying(category)]);
}

km::data ticket_summary(const Context &context, const std::vector<const FieldDefinition *> &columns, const Ticket &ticket) {
	auto &definitions = context.bts.get_fields();

	km::data cells{km::data::type::list};
	for (auto field: columns) {
		km::data cell;
		cell.set("name", field->name);
		cell.set("value", display_value(*field, ticket.get(field->name)));
		cells.push_back(cell);
	}

	// Set fields that are not shown in a column are shown after the title.
	km::data labels{km::data::type::list};
	for (auto &field: definitions.fields) {
		if (!field.has_multiple_values() || std::ranges::contains(columns, &field))
			continue;
		if (auto values = ticket.fields.find(field.name); values != ticket.fields.end())
			for (auto &value: values->second)
				labels.push_back(value);
	}

	km::data data;
	data.set("id", std::to_string(ticket.id));
	data.set("title", ticket.title);
	data.set("classes", css_classes(definitions, ticket));
	data.set("cells", cells);
	data.set("labels", labels);
	return data;
}

km::data options_list(const std::vector<std::pair<std::string, std::string>> &values, const std::string &selected) {
	km::data options{km::data::type::list};
	for (auto &[value, label]: values) {
		km::data option;
		option.set("value", value);
		option.set("label", label);
		option.set("selected", value == selected);
		options.push_back(option);
	}
	return options;
}

/// The values of the query parameters for a field, by its name or one of its aliases.
std::vector<std::string> query_values(const Request &request, const FieldDefinition &field) {
	auto values = request.get_query_values(field.name);
	for (auto &alias: field.aliases)
		for (auto &value: request.get_query_values(alias))
			values.push_back(value);
	return values;
}

bool is_category_selector(std::string_view selector) {
	return selector == "all" || selector == "open" || selector == "closed" || std::ranges::contains(status_category_names, selector);
}

/// Converts the query parameters of the ticket list to selectors, as used by lbts list.
std::vector<std::string> query_selectors(const FieldDefinitions &definitions, const Request &request) {
	std::vector<std::string> selectors;

	auto status = request.get_query_value("status");
	if (is_category_selector(status) || status.contains('='))
		selectors.push_back(status);
	else if (!status.empty())
		selectors.push_back(std::format("status={}", status));

	for (auto &field: definitions.fields) {
		if (field.name == "status")
			continue;
		for (auto &value: query_values(request, field))
			for (auto &word: field.has_multiple_values() ? split(value) : std::vector<std::string>{value})
				if (!trim(word).empty())
					selectors.push_back(std::format("{}={}", field.name, word));
	}

	return selectors;
}

/// The selections and inputs of the filter form, one for each enum and set field.
km::data filter_inputs(const Context &context) {
	auto &definitions = context.bts.get_fields();
	auto &request = context.request;
	km::data inputs{km::data::type::list};

	for (auto &field: definitions.fields) {
		km::data input;
		input.set("name", field.name);
		input.set("label", capitalize(field.label));

		km::data groups{km::data::type::list};
		auto add_group = [&](std::string_view label, const std::vector<std::pair<std::string, std::string>> &choices, const std::string &selected) {
			km::data group;
			group.set("grouped", !label.empty());
			group.set("group_label", std::string(label));
			group.set("choices", options_list(choices, selected));
			groups.push_back(group);
		};

		if (field.name == "status") {
			auto selected = request.get_query_value("status");
			if (selected.empty())
				selected = "open";

			std::vector<std::pair<std::string, std::string>> categories = {{"open", "Open"}, {"closed", "Closed"}, {"all", "All"}};
			if (!definitions.status.in_progress.empty())
				categories.insert(categories.end(), {{"to-do", "To do"}, {"in-progress", "In progress"}, {"done", "Done"}});

			// Statuses named open and closed, as in the default fields, are already covered by the categories.
			std::vector<std::pair<std::string, std::string>> statuses;
			for (auto &value: field.values)
				statuses.emplace_back(std::format("status={}", value), display_value(field, value));

			if (std::ranges::all_of(field.values, is_category_selector)) {
				add_group("", categories, selected);
			} else {
				add_group("Tickets", categories, selected);
				add_group("Status", statuses, selected);
			}
		} else if (field.type == FieldType::enumeration) {
			std::vector<std::pair<std::string, std::string>> choices = {{"", "Any"}};
			for (auto &value: field.values)
				choices.emplace_back(value, display_value(field, value));
			add_group("", choices, to_lower(request.get_query_value(field.name)));
		}

		if (field.type == FieldType::enumeration) {
			input.set("select", true);
			input.set("groups", groups);
		}

		if (field.has_multiple_values()) {
			km::data known{km::data::type::list};
			for (auto &value: field.values.empty() ? context.bts.get_used_values(field.name) : std::set<std::string>(field.values.begin(), field.values.end()))
				known.push_back(value);
			std::vector<std::string> words;
			for (auto &value: query_values(request, field))
				for (auto &word: split(value))
					words.push_back(word);
			input.set("text_input", true);
			input.set("text", join(words));
			input.set("known", known);
		}

		if (field.type == FieldType::enumeration || field.has_multiple_values())
			inputs.push_back(input);
	}

	return inputs;
}

Response list_tickets(const Context &context) {
	auto &request = context.request;
	auto &definitions = context.bts.get_fields();

	Filter filter;
	try {
		filter = parse_filter(definitions, query_selectors(definitions, request));
	} catch (const std::runtime_error &error) {
		throw BadRequest(error.what());
	}

	auto columns = definitions.parse_columns(context.bts.get_config("web", "columns"));

	km::data column_names{km::data::type::list};
	for (auto field: columns) {
		km::data column;
		column.set("name", field->name);
		column.set("label", capitalize(field->label));
		column_names.push_back(column);
	}

	km::data tickets{km::data::type::list};
	for (auto &ticket: context.bts.list(filter))
		tickets.push_back(ticket_summary(context, columns, ticket));

	km::data data;
	data.set("tickets", tickets);
	data.set("columns", column_names);
	data.set("column_count", std::to_string(columns.size() + 2));

	Response response;

	// htmx only asks for the table rows when the filter changes.
	if (request.get_header("hx-target") == "ticket-rows") {
		response = html_response(context, render(context, "ticket-rows.html", data));
	} else {
		data.set("inputs", filter_inputs(context));
		response = page(context, "Tickets", "list.html", data);
	}

	response.headers.emplace_back("Vary", "HX-Target");
	return response;
}

km::data message_data(Instance &bts, const std::string &message_id, std::size_t index, bool open) {
	km::data data;
	data.set("index", std::to_string(index));
	data.set("message_id", message_id);
	data.set("message_id_url", url_encode(message_id));
	data.set("open", open);

	try {
		auto message = bts.get_message(message_id);
		data.set("from", message.get_header_value("From"));
		data.set("to", message.get_header_value("To"));
		data.set("subject", message.get_header_value("Subject"));
		data.set("date", message.get_header_value("Date"));
		data.set("body", message.get_text());

		km::data attachments{km::data::type::list};
		for (auto attachment: message.get_attachments()) {
			km::data item;
			auto filename = attachment->get_header_parameter("Content-Disposition", "filename");
			item.set("name", filename.empty() ? std::string("(unnamed)") : filename);
			item.set("type", attachment->get_mime_type());
			attachments.push_back(item);
		}
		data.set("has_attachments", message.has_attachments());
		data.set("attachments", attachments);
	} catch (const std::exception &error) {
		data.set("body", std::format("This message could not be loaded: {}", error.what()));
	}

	return data;
}

Response show_ticket(const Context &context, std::string_view number) {
	std::int64_t id;
	auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), id);
	if (error != std::errc() || end != number.data() + number.size())
		throw NotFound("No such ticket");

	auto ticket = context.bts.find_ticket(id);
	if (!ticket)
		throw NotFound(std::format("There is no ticket with number {}", id));

	auto details = context.bts.get_details(*ticket);
	auto &definitions = context.bts.get_fields();

	km::data data;
	data.set("id", std::to_string(ticket->id));
	data.set("title", ticket->title);
	set_if_not_empty(data, "submitter", details.submitter);
	set_if_not_empty(data, "found", join(details.found_versions));
	set_if_not_empty(data, "fixed", join(details.fixed_versions));
	if (details.date)
		data.set("date", format_unix_time(details.date));

	km::data fields{km::data::type::list};
	for (auto &field: definitions.fields) {
		auto values = ticket->fields.find(field.name);
		if (values == ticket->fields.end())
			continue;

		km::data items{km::data::type::list};
		for (auto &value: values->second) {
			km::data item;
			item.set("text", display_value(field, value));
			if (field.type == FieldType::enumeration || field.has_multiple_values())
				item.set("url", filter_url(context, field, value));
			if (field.type == FieldType::number && field.minimum && field.maximum) {
				km::data progress;
				progress.set("value", std::to_string(std::stoll(value) - *field.minimum));
				progress.set("max", std::to_string(*field.maximum - *field.minimum));
				item.set("progress", progress);
			}
			items.push_back(item);
		}

		km::data item;
		item.set("label", capitalize(field.label));
		item.set("classes", field.type == FieldType::enumeration ? std::format("{}-{}", field.name, values->second.front()) : std::string());
		item.set("is_set", field.has_multiple_values());
		item.set("values", items);
		fields.push_back(item);
	}
	data.set("fields", fields);

	km::data links{km::data::type::list};
	auto add_links = [&](const std::vector<Link> &list, const auto &names) {
		for (auto &link: list) {
			km::data item;
			item.set("relation", capitalize(names.at(std::to_underlying(link.type))));
			item.set("id", std::to_string(link.ticket));
			item.set("title", link.title);
			links.push_back(item);
		}
	};
	add_links(details.links, verbose_link_names);
	add_links(details.reverse_links, reverse_link_names);
	data.set("links", links);

	auto message_ids = context.bts.get_message_ids(*ticket);
	km::data messages{km::data::type::list};
	for (std::size_t i = 0; i < message_ids.size(); i++)
		messages.push_back(message_data(context.bts, message_ids[i], i + 1, i == 0 || i + 1 == message_ids.size()));
	data.set("messages", messages);

	return page(context, std::format("#{} {}", ticket->id, ticket->title), "ticket.html", data);
}

Response show_message(const Context &context) {
	auto message_id = context.request.get_query_value("id");
	if (message_id.empty())
		throw BadRequest("Missing message ID");

	// Only serve messages that are in the index.
	if (!context.bts.has_message(message_id))
		throw NotFound("No such message");

	return {
		.content_type = "text/plain; charset=utf-8",
		.headers = {{"Content-Security-Policy", "default-src 'none'"}},
		.body = context.bts.get_message(message_id).to_string(),
	};
}

Response static_file(std::string_view content_type, std::string body) {
	return {
		.content_type = std::string(content_type),
		.headers = {{"Cache-Control", "public, max-age=3600"}},
		.body = std::move(body),
	};
}

Response route(Instance &bts, const Request &request) {
	auto root = bts.get_config("web", "root");
	root = with_trailing_slash(root.empty() ? request.script_name : root);
	auto static_root = bts.get_config("web", "static-root");
	static_root = static_root.empty() ? root + "static/" : with_trailing_slash(static_root);

	Context context{bts, request, root, static_root};
	auto &path = request.path;

	if (path == "/" || path.empty())
		return list_tickets(context);
	if (path.starts_with("/ticket/"))
		return show_ticket(context, std::string_view(path).substr(8));
	if (path == "/message")
		return show_message(context);
	if (path == "/static/lightbts.css")
		return static_file("text/css; charset=utf-8", bts.get_template("lightbts.css"));
	if (path == "/static/htmx.min.js" && has_htmx)
		return static_file("text/javascript; charset=utf-8", std::string(embedded_assets.front().data));

	throw NotFound("There is no such page");
}

}

std::string Request::get_header(const std::string &name) const {
	auto it = headers.find(name);
	return it == headers.end() ? std::string{} : it->second;
}

std::vector<std::string> Request::get_query_values(const std::string &name) const {
	auto [begin, end] = query.equal_range(name);
	return std::ranges::subrange(begin, end) | std::views::values | std::ranges::to<std::vector>();
}

std::string Request::get_query_value(const std::string &name) const {
	auto it = query.find(name);
	return it == query.end() ? std::string{} : it->second;
}

Handler::Handler(std::filesystem::path data_dir): data_dir(std::move(data_dir)) {}

Response Handler::handle(const Request &request) const {
	Response response;

	try {
		if (request.method != "GET" && request.method != "HEAD") {
			response = error_page(405, "Only GET and HEAD requests are supported");
			response.headers.emplace_back("Allow", "GET, HEAD");
		} else {
			Instance bts(data_dir, Instance::Mode::read_only);
			response = route(bts, request);
		}
	} catch (const NotFound &error) {
		response = error_page(404, error.what());
	} catch (const BadRequest &error) {
		response = error_page(400, error.what());
	} catch (const std::exception &error) {
		std::println(std::cerr, "lbts-web: error while handling {}: {}", request.path, error.what());
		response = error_page(500, "An internal error occurred");
	}

	response.headers.emplace_back("X-Content-Type-Options", "nosniff");
	response.headers.emplace_back("Referrer-Policy", "same-origin");
	return response;
}

std::string_view status_text(int status) {
	switch (status) {
	case 200: return "OK";
	case 400: return "Bad Request";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 500: return "Internal Server Error";
	default: return "Unknown";
	}
}

std::string url_encode(std::string_view text) {
	std::string result;
	for (unsigned char c: text) {
		if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
			result += static_cast<char>(c);
		else
			result += std::format("%{:02X}", c);
	}
	return result;
}

std::string url_decode(std::string_view text) {
	std::string result;
	for (std::size_t i = 0; i < text.size(); i++) {
		if (text[i] == '+') {
			result += ' ';
		} else if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1])) && std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
			unsigned value = 0;
			std::from_chars(text.data() + i + 1, text.data() + i + 3, value, 16);
			result += static_cast<char>(value);
			i += 2;
		} else {
			result += text[i];
		}
	}
	return result;
}

std::multimap<std::string, std::string> parse_query(std::string_view query) {
	std::multimap<std::string, std::string> result;
	for (auto &&part: query | std::views::split('&')) {
		std::string_view pair(part.begin(), part.end());
		if (pair.empty())
			continue;
		auto equals = pair.find('=');
		result.emplace(url_decode(pair.substr(0, equals)), equals == pair.npos ? std::string{} : url_decode(pair.substr(equals + 1)));
	}
	return result;
}

}

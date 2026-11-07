#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace LightBTS::Web {

/// An HTTP request, independent of whether it came in via HTTP, FastCGI or CGI.
struct Request {
	std::string method = "GET";
	std::string script_name;                         ///< The URL path under which the application is mounted, without trailing slash.
	std::string path = "/";                          ///< The decoded path below the script name.
	std::multimap<std::string, std::string> query;   ///< Decoded query parameters.
	std::map<std::string, std::string> headers;      ///< Header names are in lower case.

	std::string get_header(const std::string &name) const;
	std::vector<std::string> get_query_values(const std::string &name) const;
	std::string get_query_value(const std::string &name) const;
};

struct Response {
	int status = 200;
	std::string content_type = "text/html; charset=utf-8";
	std::vector<std::pair<std::string, std::string>> headers;
	std::string body;
};

/// Handles requests for one LightBTS instance. The instance is opened read-only for each request.
class Handler {
	std::filesystem::path data_dir;

	public:
	explicit Handler(std::filesystem::path data_dir);
	Response handle(const Request &request) const;
};

std::string_view status_text(int status);
std::string url_encode(std::string_view text);
std::string url_decode(std::string_view text);
std::multimap<std::string, std::string> parse_query(std::string_view query);

}

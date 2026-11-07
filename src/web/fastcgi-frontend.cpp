/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <cerrno>
#include <cstdio>
#include <fcgiapp.h>
#include <format>
#include <iostream>
#include <print>
#include <sys/socket.h>

#include "frontends.hpp"
#include "strings.hpp"

extern char **environ;

namespace LightBTS::Web {

namespace {

/// Converts CGI variables, as passed by both CGI and FastCGI, into a Request.
Request to_request(char **variables) {
	Request request;
	std::string query;

	for (char **variable = variables; *variable; ++variable) {
		std::string_view entry = *variable;
		auto equals = entry.find('=');
		if (equals == entry.npos)
			continue;

		auto name = entry.substr(0, equals);
		auto value = entry.substr(equals + 1);

		if (name == "REQUEST_METHOD") {
			request.method = value;
		} else if (name == "SCRIPT_NAME") {
			request.script_name = value;
			while (request.script_name.ends_with('/'))
				request.script_name.pop_back();
		} else if (name == "PATH_INFO") {
			request.path = value.empty() ? "/" : value;
		} else if (name == "QUERY_STRING") {
			query = value;
		} else if (name.starts_with("HTTP_")) {
			auto header = to_lower(name.substr(5));
			std::ranges::replace(header, '_', '-');
			request.headers.emplace(header, value);
		}
	}

	request.query = parse_query(query);
	return request;
}

/// Formats a response with a CGI Status header. The body is left out for HEAD requests.
std::string to_cgi_response(const Request &request, const Response &response) {
	auto text = std::format("Status: {} {}\r\nContent-Type: {}\r\nContent-Length: {}\r\n", response.status, status_text(response.status), response.content_type, response.body.size());
	for (auto &[name, value]: response.headers)
		text += std::format("{}: {}\r\n", name, value);
	text += "\r\n";
	if (request.method != "HEAD")
		text += response.body;
	return text;
}

}

bool is_fastcgi() {
	// Like FCGX_IsCGI(), but without initializing libfcgi, which installs its own SIGTERM handler.
	sockaddr_storage address;
	socklen_t length = sizeof address;
	return getpeername(0, reinterpret_cast<sockaddr *>(&address), &length) == -1 && errno == ENOTCONN;
}

int run_fastcgi(const Handler &handler, const std::string &socket) {
	if (FCGX_Init() != 0) {
		std::println(std::cerr, "Could not initialize FastCGI");
		return 1;
	}

	int fd = 0;
	if (!socket.empty()) {
		fd = FCGX_OpenSocket(socket.c_str(), 64);
		if (fd < 0) {
			std::println(std::cerr, "Could not open FastCGI socket {}", socket);
			return 1;
		}
		std::println(std::cerr, "Listening for FastCGI requests on {}", socket);
	}

	FCGX_Request fastcgi_request;
	FCGX_InitRequest(&fastcgi_request, fd, 0);

	while (FCGX_Accept_r(&fastcgi_request) == 0) {
		auto request = to_request(fastcgi_request.envp);
		auto text = to_cgi_response(request, handler.handle(request));
		FCGX_PutStr(text.data(), static_cast<int>(text.size()), fastcgi_request.out);
		FCGX_Finish_r(&fastcgi_request);
	}

	return 0;
}

int run_cgi(const Handler &handler) {
	auto request = to_request(environ);
	auto text = to_cgi_response(request, handler.handle(request));
	std::fwrite(text.data(), 1, text.size(), stdout);
	return std::fflush(stdout) == 0 ? 0 : 1;
}

}

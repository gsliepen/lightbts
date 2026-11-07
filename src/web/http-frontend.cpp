/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <charconv>
#include <format>
#include <httplib.h>
#include <iostream>
#include <print>

#include "frontends.hpp"
#include "strings.hpp"

namespace LightBTS::Web {

namespace {

struct Address {
	std::string host;
	int port;
};

Address parse_address(std::string_view address) {
	auto colon = address.rfind(':');
	if (colon == address.npos)
		throw std::runtime_error(std::format("Invalid address \"{}\", expected host:port", address));

	auto host = address.substr(0, colon);
	if (host.starts_with('[') && host.ends_with(']'))
		host = host.substr(1, host.size() - 2);

	auto port_text = address.substr(colon + 1);
	int port;
	auto [end, error] = std::from_chars(port_text.data(), port_text.data() + port_text.size(), port);
	if (error != std::errc() || end != port_text.data() + port_text.size() || port < 0 || port > 65535)
		throw std::runtime_error(std::format("Invalid port in address \"{}\"", address));

	return {std::string(host.empty() ? "127.0.0.1" : host), port};
}

Request to_request(const httplib::Request &http_request) {
	Request request{.method = http_request.method, .path = http_request.path};
	for (auto &[name, value]: http_request.params)
		request.query.emplace(name, value);
	for (auto &[name, value]: http_request.headers)
		request.headers.emplace(to_lower(name), value);
	return request;
}

}

int run_http(const Handler &handler, const std::string &address_text) {
	auto address = parse_address(address_text);

	httplib::Server server;

	auto serve = [&handler](const httplib::Request &http_request, httplib::Response &http_response) {
		auto response = handler.handle(to_request(http_request));
		http_response.status = response.status;
		for (auto &[name, value]: response.headers)
			http_response.set_header(name, value);
		http_response.set_content(std::move(response.body), response.content_type);
	};

	server.Get(".*", serve);
	server.Post(".*", serve);
	server.Put(".*", serve);
	server.Patch(".*", serve);
	server.Delete(".*", serve);
	server.Options(".*", serve);

	server.set_logger([](const httplib::Request &request, const httplib::Response &response) {
		std::println(std::cerr, "{} {} {} {}", request.remote_addr, request.method, request.target, response.status);
	});

	int port = address.port;
	if (port == 0)
		port = server.bind_to_any_port(address.host);
	else if (!server.bind_to_port(address.host, port))
		port = -1;

	if (port < 0) {
		std::println(std::cerr, "Could not listen on {}", address_text);
		return 1;
	}

	auto host = address.host.contains(':') ? std::format("[{}]", address.host) : address.host;
	std::println(std::cerr, "Listening on http://{}:{}/", host, port);

	return server.listen_after_bind() ? 0 : 1;
}

}

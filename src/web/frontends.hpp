#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <string>

#include "handler.hpp"

namespace LightBTS::Web {

/// Serves HTTP on the given address, in the form host:port. Port 0 picks a free port.
int run_http(const Handler &handler, const std::string &address);

/// Serves FastCGI requests on the given socket (a path, or :port for TCP), or on stdin if it is empty.
int run_fastcgi(const Handler &handler, const std::string &socket);

/// Handles a single CGI request from the environment.
int run_cgi(const Handler &handler);

/// Whether stdin is a FastCGI socket, which is the case when started by a FastCGI-capable web server.
bool is_fastcgi();

}

#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2017-2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "lightbts.hpp"

/// The command line options of lbts.
struct Options {
	std::string program_name = "lbts";
	bool help = false;
	bool verbose = false;
	bool no_hooks = false;
	bool no_email = false;
	bool batch = false;

	std::filesystem::path data_dir;
	std::optional<std::string> message;
	std::string severity;
	std::vector<std::string> tags;
	std::vector<std::string> fields; ///< From -F, of the form <field>=<value>.
	std::vector<std::string> versions;
	std::vector<std::string> attachments;

	LightBTS::Instance open_instance(LightBTS::Instance::Mode mode = LightBTS::Instance::Mode::open, LightBTS::InstanceOptions instance_options = {}) const {
		instance_options.no_hooks = no_hooks;
		return LightBTS::Instance(data_dir, mode, instance_options);
	}
};

using Arguments = std::vector<std::string>;

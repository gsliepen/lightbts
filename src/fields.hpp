#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config.hpp"

namespace LightBTS {

enum class FieldType {
	text,
	enumeration,
	set,
	user,
	date,
	number,
};

inline constexpr std::array<std::string_view, 6> field_type_names = {"text", "enum", "set", "user", "date", "number"};

enum class StatusCategory {
	to_do,
	in_progress,
	done,
};

inline constexpr std::array<std::string_view, 3> status_category_names = {"to-do", "in-progress", "done"};

/// A field of a ticket, as defined in a [field.<name>] section of the configuration.
struct FieldDefinition {
	std::string name;
	FieldType type;
	std::string label;                ///< The name shown in the interfaces.
	std::vector<std::string> aliases; ///< Other names for the pseudo-header.
	std::vector<std::string> values;  ///< The allowed values of an enum or set field. Any value is allowed in a set field without values.
	std::string default_value;        ///< Empty if the field has no default. For set fields, the values are separated by commas.
	bool ordered = false;             ///< Whether the values of an enum field are in a meaningful order.
	std::optional<std::int64_t> minimum;
	std::optional<std::int64_t> maximum;
	std::string unit;

	bool has_name(std::string_view name) const;
	bool allows(std::string_view value) const;
};

/// The variables of the status field that only it has.
struct StatusDefinition {
	std::vector<std::string> in_progress; ///< The values in the in-progress category.
	std::vector<std::string> done;        ///< The values in the done category.
	std::string close;                    ///< The value that lbts close sets.
	std::string reopen;                   ///< The value that lbts reopen sets.

	StatusCategory category(std::string_view value) const;
	bool is_open(std::string_view value) const { return category(value) != StatusCategory::done; }
};

struct FieldDefinitions {
	std::vector<FieldDefinition> fields; ///< In the order they are defined.
	StatusDefinition status;

	/// Finds a field by its name or one of its aliases, ignoring case.
	const FieldDefinition *find(std::string_view name) const;
};

/// Adds the definitions of the default fields to a configuration.
void add_default_fields(Config &config);

/// Reads the field definitions from a configuration. Without any field sections, the default fields are used.
/// \throws std::runtime_error if the definitions are invalid.
FieldDefinitions parse_field_definitions(const Config &config);

}

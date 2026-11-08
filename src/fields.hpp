#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
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
	std::string label;                       ///< The name shown in the interfaces.
	std::vector<std::string> aliases;        ///< Other names for the pseudo-header.
	std::vector<std::string> values;         ///< The allowed values of an enum or set field. Any value is allowed in a set field without values.
	std::vector<std::string> default_values; ///< The values a new ticket starts with. Only set fields can have more than one.
	bool ordered = false;                    ///< Whether the values of an enum field are in a meaningful order.
	std::optional<std::int64_t> minimum;
	std::optional<std::int64_t> maximum;
	std::string unit;

	bool has_name(std::string_view name) const;
	bool has_multiple_values() const { return type == FieldType::set; }
	/// Whether values can be compared with < and >.
	bool is_comparable() const { return ordered || type == FieldType::date || type == FieldType::number; }

	/// Converts a value as written in a pseudo-header to the form stored in the index.
	/// \return The value, or nothing if it is not valid for this field.
	std::optional<std::string> normalize(std::string_view value) const;
	/// A description of the valid values, like "a date of the form YYYY-MM-DD".
	std::string describe_valid_values() const;
	/// The name of the field as written in pseudo-headers, like "Severity" or "Due-Date".
	std::string header_name() const;
	/// Formats a stored value for display, adding the unit of number fields.
	std::string format(std::string_view value) const;
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
	std::string signature; ///< A text that changes whenever the definitions change.

	/// Finds a field by its name or one of its aliases, ignoring case.
	const FieldDefinition *find(std::string_view name) const;
	/// Like find(), but throws an error if there is no such field.
	const FieldDefinition &get(std::string_view name) const;

	/// Parses a list of field names, as in the cli.columns and web.columns variables.
	/// If the list is empty, the status and severity fields are used, if they exist.
	std::vector<const FieldDefinition *> parse_columns(std::string_view names) const;
};

/// Adds the definitions of the default fields to a configuration.
void add_default_fields(Config &config);

/// Reads the field definitions from a configuration. Without any field sections, the default fields are used.
/// \throws std::runtime_error if the definitions are invalid.
FieldDefinitions parse_field_definitions(const Config &config);

enum class Comparison {
	equal,
	not_equal,
	less,
	less_equal,
	greater,
	greater_equal,
};

inline constexpr std::array<std::string_view, 6> comparison_operators = {"=", "!=", "<", "<=", ">", ">="};

/// A condition on a field, like priority>=high.
struct Condition {
	std::string field; ///< The name of the field, never an alias.
	Comparison comparison;
	std::string value; ///< The value in the form stored in the index.
};

/// A condition on links, like blocks=12: tickets that block ticket 12.
struct LinkCondition {
	std::string type;   ///< The name of the link type.
	std::string ticket; ///< A reference to the other ticket, as accepted by Instance::find_ticket_number().
};

/// Which tickets Instance::list() returns.
struct Filter {
	/// The categories of the statuses of the tickets.
	std::set<StatusCategory> categories = {StatusCategory::to_do, StatusCategory::in_progress};
	/// Conditions with = on the same field match if any of them matches, all other conditions must all match.
	std::vector<Condition> conditions;
	/// Conditions on links, which must all match.
	std::vector<LinkCondition> links;
};

/// Parses selectors as given to lbts list:
/// - open, closed and all, and the status categories to-do, in-progress and done;
/// - a condition of the form <field><operator><value>;
/// - a condition of the form <link type>=<ticket>, like blocks=12;
/// - a value of an enum field, or else of a set field with a list of values, or else of any set field,
///   which selects tickets with that value.
/// \throws std::runtime_error if a selector is not valid.
Filter parse_filter(const FieldDefinitions &definitions, std::span<const std::string> selectors);

}

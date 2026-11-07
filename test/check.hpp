#pragma once

/* LightBTS -- a lightweight issue tracking system
   Copyright © 2026 Guus Sliepen <guus@lightbts.info>

   SPDX-License-Identifier: GPL-3.0+
*/

// A minimal test helper. Unlike assert(), CHECK() also works when NDEBUG is defined.

#include <iostream>
#include <print>

inline int check_failures = 0;

inline void check(bool condition, const char *expression, const char *file, int line) {
	if (!condition) {
		std::println(std::cerr, "{}:{}: check failed: {}", file, line, expression);
		check_failures++;
	}
}

#define CHECK(...) check(static_cast<bool>(__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)

template<typename Function>
bool throws(Function function) {
	try {
		function();
	} catch (...) {
		return true;
	}
	return false;
}

inline int check_result() {
	return check_failures ? 1 : 0;
}

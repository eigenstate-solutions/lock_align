#pragma once

// Functions defined in other.cpp, which each test builds with its own
// configuration. Only plain types cross between the two files.

#include <cstddef>

std::size_t other_mutex_size();
bool other_reports_out_of_order();

#pragma once

#include <mutex>

#include "lock_align/ordered_mutex.hpp"

// Defined in link_other.cpp with checks off, called from link_main.cpp with
// checks on. The parameter's type differs between the two, so this can't
// link.
void use_mutex(lock_align::ordered_mutex<std::mutex, 1>& m);

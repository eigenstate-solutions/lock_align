#include "link_shared.hpp"

void use_mutex(lock_align::ordered_mutex<std::mutex, 1>& m) { std::lock_guard g(m); }

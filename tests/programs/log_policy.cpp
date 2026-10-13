// log_on_violation prints the violation and lets the lock go ahead.

#include <cstdio>
#include <mutex>

#include "lock_align/held.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"

int main() {
  lock_align::ordered_mutex<std::mutex, 1, lock_align::log_on_violation> low("low");
  lock_align::ordered_mutex<std::mutex, 2, lock_align::log_on_violation> high("high");
  std::lock_guard h(high);
  std::lock_guard l(low);
  // On stderr too, so that it follows the report in a fixed order.
  std::fprintf(stderr, "went ahead holding %zu\n", lock_align::held_count());
}

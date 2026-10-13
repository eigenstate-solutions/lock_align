#include "other.hpp"

#include <mutex>

#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"

std::size_t other_mutex_size() { return sizeof(lock_align::ordered_mutex<std::mutex, 1>); }

bool other_reports_out_of_order() {
  lock_align::ordered_mutex<std::mutex, 1, lock_align::throw_on_violation> low;
  lock_align::ordered_mutex<std::mutex, 2, lock_align::throw_on_violation> high;
  std::lock_guard h(high);
  try {
    std::lock_guard l(low);
  } catch (const lock_align::lock_order_error&) {
    return true;
  }
  return false;
}

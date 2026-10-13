// Locks out of order and says whether lock_align noticed, which shows whether
// the installed package carried the configuration it was installed with.

#include <cstdio>
#include <mutex>

#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"

int main() {
  lock_align::ordered_mutex<std::mutex, 1> low("low");
  lock_align::ordered_mutex<std::mutex, 2> high("high");
  std::lock_guard g(high);
  try {
    std::lock_guard g2(low);
  } catch (const lock_align::lock_order_error&) {
    std::puts("checked");
    return 0;
  }
  std::puts("unchecked");
  return 0;
}

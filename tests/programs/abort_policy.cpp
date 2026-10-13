// abort_on_violation prints the violation and ends the program.

#include <cstdio>
#include <mutex>

#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"

int main() {
  lock_align::ordered_mutex<std::mutex, 1, lock_align::abort_on_violation> low("low");
  lock_align::ordered_mutex<std::mutex, 2, lock_align::abort_on_violation> high("high");
  std::lock_guard h(high);
  std::lock_guard l(low);
  std::puts("went ahead");  // never reached
}

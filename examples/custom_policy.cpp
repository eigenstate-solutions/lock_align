// A violation policy that records each violation and lets the program carry
// on, then reports them all at the end. Useful in a test suite that should
// list every ordering problem in one run instead of stopping at the first.

#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_thread.hpp"
#include "lock_align/violation.hpp"

using namespace lock_align;

struct record_violations {
  // A plain std::mutex: the policy must not take an ordered mutex, because it
  // runs in the middle of an ordering check.
  static inline std::mutex mutex;
  static inline std::vector<std::string> found;

  static void on_violation(const violation& v) {
    std::lock_guard g(mutex);
    found.push_back(v.message());
  }
};

template <auto Level>
using checked_mutex = ordered_mutex<std::mutex, Level, record_violations>;
using checked_thread = ordered_thread<record_violations>;

int main() {
  checked_mutex<1> accounts{"accounts"};
  checked_mutex<2> audit_log{"audit log"};

  // Right order on one thread, wrong order on another. Run one after the
  // other, they never deadlock, but run together they could.
  checked_thread first(0, [&] {
    std::lock_guard a(accounts);
    std::lock_guard l(audit_log);
  });
  first.join();

  checked_thread second(0, [&] {
    std::lock_guard l(audit_log);
    std::lock_guard a(accounts);  // recorded; the lock goes ahead anyway
  });
  second.join();

  std::printf("%zu violation(s)\n", record_violations::found.size());
  for (const std::string& message : record_violations::found)
    std::printf("  %s\n", message.c_str());
  return record_violations::found.empty() ? 0 : 1;
}

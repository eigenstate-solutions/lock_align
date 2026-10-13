// Built together with other.cpp, each with a configuration the test chooses.
// When the configurations disagree, the startup check aborts before main.

#include <cstdio>
#include <mutex>

#include "lock_align/ordered_mutex.hpp"
#include "other.hpp"

int main() {
  lock_align::ordered_mutex<std::mutex, 1> m;
  { std::lock_guard g(m); }

#ifdef EXPECT_OTHER_DISABLED
  // Each file keeps its own configuration: nothing was merged between them.
  const bool isolated = sizeof(m) > sizeof(std::mutex) &&
                        other_mutex_size() == sizeof(std::mutex) && !other_reports_out_of_order();
  std::puts(isolated ? "isolated" : "not isolated");
  return isolated ? 0 : 1;
#else
  const bool ok = other_reports_out_of_order();
  std::puts(ok ? "ran" : "other.cpp didn't report");
  return ok ? 0 : 1;
#endif
}

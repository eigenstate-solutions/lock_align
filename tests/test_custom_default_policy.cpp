// A policy of your own as LOCK_ALIGN_DEFAULT_POLICY, declared the way the
// config docs show: after violation_fwd.hpp, before the other headers.
#define LOCK_ALIGN_DEFAULT_POLICY ::my::policy

#include "lock_align/violation_fwd.hpp"

namespace my {
struct policy {
  static inline int violations = 0;
  static void on_violation(const lock_align::violation&) noexcept { ++violations; }
};
}  // namespace my

#include <catch2/catch_test_macros.hpp>
#include <mutex>
#include <type_traits>

#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"

using namespace lock_align;

static_assert(std::is_same_v<default_policy, my::policy>);
static_assert(std::is_same_v<ordered_mutex<std::mutex, 1>::policy_type, my::policy>);

TEST_CASE("a policy of your own can be the default", "[custom_default_policy][policy]") {
  ordered_mutex<std::mutex, 1> low;
  ordered_mutex<std::mutex, 2> high;
  std::lock_guard h(high);
  { std::lock_guard l(low); }  // reported; goes ahead
  CHECK(my::policy::violations == 1);
}

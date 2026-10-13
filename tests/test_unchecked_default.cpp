// With LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK set to unchecked, signalling
// without an obligation is allowed unless a resource asks for the check.
// tests/CMakeLists.txt builds this file with that definition; the
// static_assert below checks it.

#include <catch2/catch_test_macros.hpp>
#include <future>

#include "lock_align/obligation.hpp"
#include "lock_align/ordered_condition_variable.hpp"
#include "lock_align/ordered_latch.hpp"
#include "lock_align/ordered_promise.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"

using namespace lock_align;

static_assert(default_obligation_check == obligation_check::unchecked);

TEST_CASE("the default can be unchecked", "[unchecked_default][obligation_check]") {
  ordered_condition_variable<1> cv;
  ordered_latch done(1, 1);
  ordered_promise<int> p(1);
  CHECK_NOTHROW(cv.notify_one());
  CHECK_NOTHROW(done.count_down());
  CHECK_NOTHROW(p.set_value(1));
}

TEST_CASE("a resource can still ask for the check", "[unchecked_default][obligation_check]") {
  ordered_condition_variable<1, default_policy, obligation_check::required> cv;
  CHECK_THROWS_AS(cv.notify_one(), missing_obligation_error);
}

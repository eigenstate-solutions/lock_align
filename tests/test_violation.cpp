#include <catch2/catch_test_macros.hpp>
#include <mutex>
#include <string>
#include <type_traits>

#include "lock_align/obligation.hpp"
#include "lock_align/ordered_condition_variable.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/task_scope.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"

using namespace lock_align;

static_assert(std::is_abstract_v<violation>);
static_assert(!std::is_copy_constructible_v<violation>);
static_assert(std::is_base_of_v<task_violation, task_entry_violation>);
static_assert(std::is_base_of_v<violation_error, lock_order_error>);
static_assert(std::is_base_of_v<task_boundary_error, task_exit_error>);
static_assert(std::is_base_of_v<violation, missing_obligation_violation>);
static_assert(std::is_base_of_v<violation_error, missing_obligation_error>);
static_assert(std::is_base_of_v<std::logic_error, violation_error>);

namespace {

// Overloads on_violation: each call should reach the most specific overload.
struct sorting_policy {
  static inline std::string last;
  static void on_violation(const lock_order_violation& v) { last = "order " + v.requested().name; }
  static void on_violation(const task_violation& v) { last = "task " + v.task_name(); }
  static void on_violation(const missing_obligation_violation& v) {
    last = "missing " + v.resource().name;
  }
};

lock_order_violation sample() {
  return {{"low", 1, resource_kind::mutex}, {"high", 2, resource_kind::obligation}};
}

}  // namespace

TEST_CASE("each violation describes itself", "[violation]") {
  const resource_info held{"m", 3, resource_kind::mutex};
  CHECK(sample().message() ==
        "lock order violation: blocking on mutex 'low' (level 1) while holding obligation "
        "'high' (level 2)");
  CHECK(task_entry_violation("job", held).message() ==
        "task boundary violation: task 'job' started while holding mutex 'm' (level 3)");
  CHECK(task_entry_violation("job", held, "earlier").message() ==
        "task boundary violation: task 'job' started while holding mutex 'm' (level 3), left by "
        "task 'earlier'");
  CHECK(task_exit_violation("job", held).message() ==
        "task boundary violation: task 'job' ended still holding mutex 'm' (level 3)");
}

TEST_CASE("a missing obligation describes what needed it", "[violation]") {
  const resource_info r{"ready", 1, resource_kind::obligation};
  auto message = [&](obligation_needed_for what) {
    return missing_obligation_violation(r, what).message();
  };
  CHECK(message(obligation_needed_for::notify) ==
        "missing obligation: notifying 'ready' (level 1) without holding its obligation");
  CHECK(message(obligation_needed_for::count_down) ==
        "missing obligation: counting down 'ready' (level 1) without holding its obligation");
  CHECK(message(obligation_needed_for::set_value) ==
        "missing obligation: setting the value of 'ready' (level 1) without holding its "
        "obligation");
  CHECK(message(obligation_needed_for::set_exception) ==
        "missing obligation: setting an exception in 'ready' (level 1) without holding its "
        "obligation");
}

TEST_CASE("throw_error throws the matching error", "[violation]") {
  const resource_info held{"m", 3, resource_kind::mutex};
  const violation& order = sample();
  CHECK_THROWS_AS(order.throw_error(), lock_order_error);
  CHECK_THROWS_AS(task_entry_violation("job", held).throw_error(), task_entry_error);
  CHECK_THROWS_AS(task_exit_violation("job", held).throw_error(), task_exit_error);
  CHECK_THROWS_AS(missing_obligation_violation({"ready", 1, resource_kind::obligation},
                                               obligation_needed_for::notify)
                      .throw_error(),
                  missing_obligation_error);
}

TEST_CASE("an error carries its violation", "[violation]") {
  try {
    throw_on_violation::on_violation(sample());
  } catch (const violation_error& e) {
    CHECK(std::string(e.what()) == e.info().message());
    const auto* order = dynamic_cast<const lock_order_violation*>(&e.info());
    REQUIRE(order != nullptr);
    CHECK(order->requested().name == "low");
    CHECK(order->held().name == "high");
  }
}

TEST_CASE("task errors share a base", "[violation]") {
  bool caught = false;
  try {
    task_exit_violation("job", {"m", 3, resource_kind::mutex}).throw_error();
  } catch (const task_boundary_error& e) {
    caught = true;
    CHECK(e.info().task_name() == "job");
  }
  CHECK(caught);
}

TEST_CASE("a policy can overload on the kind of violation", "[violation][policy]") {
  ordered_mutex<std::mutex, 1, sorting_policy> low("low");
  ordered_mutex<std::mutex, 2, sorting_policy> high("high");
  {
    std::lock_guard h(high);
    std::lock_guard l(low);  // reported; goes ahead
    CHECK(sorting_policy::last == "order low");
    task_scope<sorting_policy> task("job");
    CHECK(sorting_policy::last == "task job");
  }
  ordered_condition_variable<1, sorting_policy> cv("ready");
  cv.notify_one();  // reported; goes ahead
  CHECK(sorting_policy::last == "missing ready");
}

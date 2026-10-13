#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <atomic>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#include "lock_align/held.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_thread.hpp"
#include "lock_align/task_scope.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("a task may lock in order when its worker holds nothing", "[task_scope]") {
  ordered_mutex<std::mutex, 1> low;
  ordered_mutex<std::mutex, 2> high;
  {
    task_scope task;
    std::lock_guard gl(low);
    std::lock_guard gh(high);
    CHECK(current_level() == 2);
  }
  CHECK(held_count() == 0);
}

TEST_CASE("starting a task while holding a lock is a violation", "[task_scope]") {
  ordered_mutex<std::mutex, 2> outer("outer");
  std::lock_guard g(outer);
  try {
    task_scope task("job");
    FAIL_CHECK("task_scope did not throw");
  } catch (const task_entry_error& e) {
    CHECK(e.info().task_name() == "job");
    CHECK(e.info().held().name == "outer");
    CHECK(e.info().left_by().empty());
    CHECK_THAT(e.what(), ContainsSubstring("'job' started while holding mutex 'outer' (level 2)"));
    CHECK_THAT(e.what(), !ContainsSubstring("left by"));
  }
  CHECK(held_count() == 1);
}

TEST_CASE("ending a task that still holds a lock is a violation", "[task_scope]") {
  ordered_mutex<std::mutex, 1> leaked("leaked");
  auto run_task = [&] {
    task_scope task("job");
    leaked.lock();
  };
  try {
    run_task();
    FAIL_CHECK("~task_scope did not throw");
  } catch (const task_exit_error& e) {
    CHECK(e.info().task_name() == "job");
    CHECK(e.info().held().name == "leaked");
    CHECK_THAT(e.what(), ContainsSubstring("'job' ended still holding mutex 'leaked'"));
  }
  CHECK(held_count() == 1);
  leaked.unlock();
}

namespace {

// Records the last violation and lets the operation go ahead. Tagged per
// test, like counting_policy.
template <class Tag>
struct recording_policy {
  static inline std::string last;
  static inline std::string left_by;
  static void on_violation(const violation& v) noexcept {
    last = v.message();
    const auto* entry = dynamic_cast<const task_entry_violation*>(&v);
    left_by = entry != nullptr ? entry->left_by() : "";
  }
};

}  // namespace

TEST_CASE("a throwing policy's exit check is skipped while an exception unwinds",
          "[task_scope]") {
  ordered_mutex<std::mutex, 1> leaked;
  auto run_task = [&] {
    task_scope task;
    leaked.lock();
    throw std::runtime_error("task failed");
  };
  CHECK_THROWS_AS(run_task(), std::runtime_error);
  leaked.unlock();
}

TEST_CASE("a noexcept policy's exit check runs while an exception unwinds",
          "[task_scope][policy]") {
  using recording = recording_policy<struct tag>;
  ordered_mutex<std::mutex, 1> leaked("leaked");
  auto run_task = [&] {
    task_scope<recording> task("job");
    leaked.lock();
    throw std::runtime_error("task failed");
  };
  CHECK_THROWS_AS(run_task(), std::runtime_error);
  CHECK_THAT(recording::last, ContainsSubstring("'job' ended still holding mutex 'leaked'"));
  leaked.unlock();
}

TEST_CASE("the next task names the task that left a mutex held", "[task_scope]") {
  ordered_mutex<std::mutex, 1> leaked("leaked");
  auto run_task = [&] {
    task_scope task("job");
    leaked.lock();
    throw std::runtime_error("task failed");
  };
  CHECK_THROWS_AS(run_task(), std::runtime_error);
  for (int i = 0; i < 2; ++i) {  // every later task, not just the next
    try {
      task_scope next("next");
      FAIL_CHECK("task_scope did not throw");
    } catch (const task_entry_error& e) {
      CHECK(e.info().left_by() == "job");
      CHECK(e.info().held().name == "leaked");
      CHECK_THAT(e.what(), ContainsSubstring(
                               "'next' started while holding mutex 'leaked' (level 1), left by "
                               "task 'job'"));
    }
  }
  leaked.unlock();
}

TEST_CASE("a leak reported at exit is also named by the next task", "[task_scope]") {
  using recording = recording_policy<struct tag>;
  ordered_mutex<std::mutex, 1> low("low");
  ordered_mutex<std::mutex, 2> high("high");
  auto run_task = [&] {
    task_scope task("job");
    low.lock();
    high.lock();
  };
  CHECK_THROWS_AS(run_task(), task_exit_error);
  { task_scope<recording> next("next"); }
  CHECK(recording::left_by == "job");
  high.unlock();
  low.unlock();
}

TEST_CASE("a mutex the worker took itself names no task", "[task_scope]") {
  using recording = recording_policy<struct tag>;
  ordered_mutex<std::mutex, 1> m("m");
  auto run_task = [&] {
    task_scope task("job");
    m.lock();
  };
  CHECK_THROWS_AS(run_task(), task_exit_error);
  m.unlock();  // forgets that the task left m, with no task in between
  m.lock();    // the worker's own lock this time
  recording::left_by = "unset";
  { task_scope<recording> next("next"); }
  CHECK(recording::left_by.empty());
  m.unlock();
}

TEST_CASE("a custom policy may let the task run anyway", "[task_scope][policy]") {
  using policy = counting_policy<struct tag>;
  ordered_mutex<std::mutex, 2> outer;
  std::lock_guard g(outer);
  { task_scope<policy> task; }
  CHECK(policy::violations.load() == 1);  // the worker's lock isn't a task leak
}

TEST_CASE("a task may start under an obligation its worker owes", "[task_scope]") {
  ordered_mutex<std::mutex, 1> low;
  ordered_mutex<std::mutex, 2> high;
  std::atomic<int> ok{0};
  ordered_thread worker(1, "worker", [&] {
    task_scope task;
    // The worker's obligation still constrains the task.
    if (throws_order_error([&] { std::lock_guard g(low); })) ++ok;
    if (!throws_order_error([&] { std::lock_guard g(high); })) ++ok;
  });
  worker.join();
  CHECK(ok.load() == 2);
}

TEST_CASE("ending a task that still owes an obligation is a violation", "[task_scope]") {
  auto run_task = [] {
    task_scope task("job");
    obligation leaked(1, "leaked");
  };
  CHECK_NOTHROW(run_task());  // destroyed before the scope ends: not a leak
  std::optional<obligation> kept;  // outlives the task
  bool reported = false;
  try {
    task_scope task("job");
    kept.emplace(1, "kept");
  } catch (const task_exit_error& e) {
    reported = true;
    CHECK(e.info().held().kind == resource_kind::obligation);
  }
  CHECK(reported);
  CHECK(held_count() == 1);
}

TEST_CASE("a task that swaps an entry held at entry for a new one is reported", "[task_scope]") {
  ordered_mutex<std::mutex, 2> taken("taken");
  std::optional<obligation> owed(std::in_place, 1, "owed");
  bool reported = false;
  try {
    task_scope task("job");
    owed.reset();  // releases what the worker held at entry
    taken.lock();  // and takes something new: the count is unchanged
  } catch (const task_exit_error& e) {
    reported = true;
    CHECK(e.info().held().name == "taken");
  }
  CHECK(reported);
  taken.unlock();
}

TEST_CASE("releasing and retaking an entry held at entry is not a leak", "[task_scope][policy]") {
  using policy = counting_policy<struct tag>;
  ordered_mutex<std::mutex, 1, policy> m;
  m.lock();
  std::optional<obligation> owed(std::in_place, 1);
  {
    task_scope<policy> task;  // reported: m is held
    m.unlock();
    REQUIRE(m.try_lock());  // back in the record, now after the obligation
  }
  CHECK(policy::violations == 1);
  owed.reset();
  m.unlock();
}

TEST_CASE("the exit check works with many entries held at entry", "[task_scope]") {
  ordered_mutex<std::mutex, 9> leaked("leaked");
  std::optional<obligation> owed[6];
  for (std::optional<obligation>& o : owed) o.emplace(1);
  CHECK_NOTHROW([] { task_scope task("clean"); }());
  bool reported = false;
  try {
    task_scope task("job");
    leaked.lock();
  } catch (const task_exit_error& e) {
    reported = true;
    CHECK(e.info().held().name == "leaked");
  }
  CHECK(reported);
  leaked.unlock();
  for (std::optional<obligation>& o : owed) o.reset();
  CHECK(held_count() == 0);
}

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <utility>

#include "lock_align/held.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_condition_variable.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;
using namespace std::chrono_literals;

TEST_CASE("a notifier holding the obligation may lock the mutex", "[condition_variable]") {
  ordered_mutex<std::mutex, 2> m;
  ordered_condition_variable<1> cv("ready");
  bool ready = false;
  std::thread notifier([&] {
    obligation duty = cv.take_obligation();
    std::lock_guard g(m);
    ready = true;
    cv.notify_one();
  });
  {
    std::unique_lock lk(m);
    cv.wait(lk, [&] { return ready; });
    CHECK(held_count() == 1);
  }
  notifier.join();
}

// A waiter on cv may hold config (level 1) while it waits. If the notifier
// blocked on config before notifying, each would wait for the other forever.
// The checks don't need a real waiter to find that: they run on one thread.
TEST_CASE("a notifier holding the obligation may not lock what a waiter may hold",
          "[condition_variable]") {
  ordered_mutex<std::mutex, 1> config("config");
  ordered_mutex<std::mutex, 2> m("queue");
  ordered_condition_variable<1> cv("ready");
  obligation duty = cv.take_obligation();
  bool reported = false;
  try {
    std::lock_guard g(config);
  } catch (const lock_order_error& e) {
    reported = true;
    CHECK(e.info().requested().name == "config");
    CHECK(e.info().held().name == "ready");
    CHECK(e.info().held().kind == resource_kind::obligation);
  }
  CHECK(reported);
  CHECK_NOTHROW(std::lock_guard(m));  // the condition variable's own mutex is above it
}

TEST_CASE("a notifier without the obligation is not checked", "[condition_variable]") {
  // The same lock as above, but nothing says this thread will notify, so
  // nothing is reported, and the deadlock would only show with bad timing.
  ordered_mutex<std::mutex, 1> config("config");
  ordered_condition_variable<1> cv("ready");
  CHECK_NOTHROW(std::lock_guard(config));
}

TEST_CASE("a waiter may hold mutexes up to the condition variable's level", "[condition_variable]") {
  ordered_mutex<std::mutex, 2> two;
  ordered_mutex<std::mutex, 3> m;
  ordered_condition_variable<2> cv;
  std::lock_guard g(two);
  std::unique_lock lk(m);
  CHECK_NOTHROW(cv.wait(lk, [] { return true; }));
}

TEST_CASE("waiting while holding a mutex the notifier may need throws", "[condition_variable]") {
  ordered_mutex<std::mutex, 2> other("other");
  ordered_mutex<std::mutex, 3> m("m");
  ordered_condition_variable<1> cv("ready");
  std::lock_guard g(other);
  std::unique_lock lk(m);
  try {
    cv.wait(lk, [] { return true; });
    FAIL_CHECK("wait did not throw");
  } catch (const lock_order_error& e) {
    CHECK(e.info().requested().name == "ready");
    CHECK(e.info().requested().kind == resource_kind::obligation);
    CHECK(e.info().held().name == "other");
  }
  CHECK(lk.owns_lock());
}

TEST_CASE("a notifier waiting on its own condition variable throws", "[condition_variable]") {
  ordered_mutex<std::mutex, 2> m;
  ordered_condition_variable<1> cv;
  obligation duty = cv.take_obligation();
  std::unique_lock lk(m);
  CHECK_THROWS_AS(cv.wait(lk, [] { return true; }), lock_order_error);
}

TEST_CASE("timed waits only check the relock", "[condition_variable]") {
  ordered_mutex<std::mutex, 2> other;
  ordered_mutex<std::mutex, 3> m;
  ordered_condition_variable<1> cv;
  std::lock_guard g(other);
  std::unique_lock lk(m);
  CHECK_NOTHROW(cv.wait_for(lk, 1ms));
  CHECK_NOTHROW(cv.wait_until(lk, std::chrono::steady_clock::now()));
}

TEST_CASE("condition variable checks the relock before waiting", "[condition_variable]") {
  ordered_mutex<std::mutex, 1> low;
  ordered_mutex<std::mutex, 2> high;
  ordered_condition_variable<0> cv;
  std::lock_guard g(high);
  REQUIRE(low.try_lock());
  std::unique_lock lk(low, std::adopt_lock);
  CHECK_THROWS_AS(cv.wait_for(lk, 1ms), lock_order_error);
  CHECK(lk.owns_lock());
}

TEST_CASE("the notification check uses the condition variable's policy",
          "[condition_variable][policy]") {
  using policy = counting_policy<struct tag>;
  ordered_mutex<std::mutex, 2> other;
  ordered_mutex<std::mutex, 3> m;
  ordered_condition_variable<1, policy> cv;
  std::lock_guard g(other);
  std::unique_lock lk(m);
  cv.wait(lk, [] { return true; });
  CHECK(policy::violations.load() == 1);
}

using unchecked_cv = ordered_condition_variable<1, default_policy, obligation_check::unchecked>;

static_assert(!noexcept(std::declval<ordered_condition_variable<1>&>().notify_one()));
static_assert(!noexcept(std::declval<ordered_condition_variable<1>&>().notify_all()));
static_assert(noexcept(std::declval<unchecked_cv&>().notify_one()));
static_assert(noexcept(std::declval<unchecked_cv&>().notify_all()));

// The waits accept only a lock on an ordered_mutex.
template <class Lock>
concept waitable = requires(ordered_condition_variable<1>& cv, Lock& lock) { cv.wait(lock); };
static_assert(waitable<std::unique_lock<ordered_mutex<std::mutex, 2>>>);
static_assert(waitable<std::shared_lock<ordered_mutex<std::shared_mutex, 2>>>);
static_assert(waitable<std::unique_lock<ordered_mutex<std::mutex, 0>>>);
static_assert(!waitable<std::unique_lock<std::mutex>>);
static_assert(!waitable<ordered_mutex<std::mutex, 2>>);

TEST_CASE("notifying without the obligation throws", "[condition_variable][obligation_check]") {
  ordered_condition_variable<1> cv("ready");
  try {
    cv.notify_one();
    FAIL_CHECK("notify_one did not throw");
  } catch (const missing_obligation_error& e) {
    CHECK(e.info().resource().name == "ready");
    CHECK(e.info().resource().level == 1);
    CHECK(e.info().needed_for() == obligation_needed_for::notify);
  }
  CHECK_THROWS_AS(cv.notify_all(), missing_obligation_error);
  obligation duty = cv.take_obligation();
  CHECK_NOTHROW(cv.notify_one());
  CHECK_NOTHROW(cv.notify_all());
}

TEST_CASE("another condition variable's obligation doesn't count",
          "[condition_variable][obligation_check]") {
  ordered_condition_variable<1> cv;
  ordered_condition_variable<1> other;
  obligation duty = other.take_obligation();
  CHECK_THROWS_AS(cv.notify_one(), missing_obligation_error);
}

TEST_CASE("an unchecked condition variable may notify without the obligation",
          "[condition_variable][obligation_check]") {
  unchecked_cv cv;
  CHECK_NOTHROW(cv.notify_one());
  CHECK_NOTHROW(cv.notify_all());
}

TEST_CASE("a missing obligation reported to a returning policy still notifies",
          "[condition_variable][obligation_check][policy]") {
  using policy = counting_policy<struct tag>;
  ordered_condition_variable<1, policy> cv;
  cv.notify_one();
  cv.notify_all();
  CHECK(policy::violations.load() == 2);
}

TEST_CASE("obligations from one condition variable can be released in any order",
          "[condition_variable][obligation_check]") {
  ordered_condition_variable<1> cv("ready");
  // Both are recorded under the condition variable, and each still removes
  // its own entry. On the heap, since an obligation can't be moved.
  std::unique_ptr<obligation> first(new obligation(cv.take_obligation()));
  std::unique_ptr<obligation> second(new obligation(cv.take_obligation()));
  first.reset();
  CHECK(held_count() == 1);
  CHECK_NOTHROW(cv.notify_one());  // second still counts
  second.reset();
  CHECK(held_count() == 0);
  CHECK_THROWS_AS(cv.notify_one(), missing_obligation_error);
}

TEST_CASE("a condition variable works with a shared_lock", "[condition_variable]") {
  ordered_mutex<std::shared_mutex, 2> m("shared");
  ordered_condition_variable<1> cv("ready");
  bool ready = false;
  std::thread notifier([&] {
    obligation duty = cv.take_obligation();
    {
      std::unique_lock lk(m);
      ready = true;
    }
    cv.notify_all();
  });
  {
    std::shared_lock lk(m);
    cv.wait(lk, [&] { return ready; });
    CHECK(held_count() == 1);
  }
  notifier.join();
  CHECK(held_count() == 0);
}

TEST_CASE("a returning policy reports the relock again when the wait wakes",
          "[condition_variable][policy]") {
  using cv_policy = counting_policy<struct cv_tag>;
  using mutex_policy = counting_policy<struct mutex_tag>;
  ordered_mutex<std::mutex, 3, mutex_policy> above;
  ordered_mutex<std::mutex, 2, mutex_policy> m;
  ordered_condition_variable<1, cv_policy> cv;
  bool waiting = false;
  bool ready = false;
  std::thread notifier([&] {
    obligation duty = cv.take_obligation();
    // waiting is set while m is held, and m is only free again once the
    // main thread is inside wait, so this notifies a thread that waits.
    for (;;) {
      std::lock_guard g(m);
      if (waiting) {
        ready = true;
        break;
      }
    }
    cv.notify_one();
  });
  {
    std::lock_guard a(above);
    std::unique_lock lk(m);  // reported once: m is below above
    const int before = mutex_policy::violations;
    waiting = true;
    while (!ready) cv.wait(lk);
    // Each wait reports the wait for a notification, then the relock: once
    // before waiting, and again when the standard library relocks.
    CHECK(cv_policy::violations >= 1);
    CHECK(mutex_policy::violations - before >= 2);
  }
  notifier.join();
}

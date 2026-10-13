// With LOCK_ALIGN_DISABLE, the wrappers cost nothing and check nothing.
// tests/CMakeLists.txt builds this file with LOCK_ALIGN_DISABLE defined.
#ifndef LOCK_ALIGN_DISABLE
#error "test_disabled.cpp is built with LOCK_ALIGN_DISABLE defined"
#endif

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <latch>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

#include "lock_align/held.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_condition_variable.hpp"
#include "lock_align/ordered_latch.hpp"
#include "lock_align/ordered_lock.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_promise.hpp"
#include "lock_align/ordered_thread.hpp"
#include "lock_align/task_scope.hpp"
#include "held_listener.hpp"
#include "replaced_new.hpp"

using namespace lock_align;
using namespace lock_align_test;

static_assert(sizeof(ordered_mutex<std::mutex, 1>) == sizeof(std::mutex));
static_assert(sizeof(ordered_mutex<std::shared_mutex, 1>) == sizeof(std::shared_mutex));
static_assert(sizeof(ordered_condition_variable<1>) == sizeof(std::condition_variable_any));
static_assert(sizeof(obligation) == sizeof(level_t));
static_assert(std::is_empty_v<task_scope<>>);
static_assert(sizeof(ordered_promise<int>) == sizeof(std::pair<std::promise<int>, level_t>));
static_assert(noexcept(std::declval<ordered_condition_variable<1>&>().notify_one()));
static_assert(noexcept(std::declval<ordered_condition_variable<1>&>().notify_all()));

TEST_CASE("locks out of order are not checked", "[disabled]") {
  ordered_mutex<std::mutex, 1> low;
  ordered_mutex<std::mutex, 2> high;
  std::lock_guard gh(high);
  std::lock_guard gl(low);
  CHECK(held_count() == 0);
}

TEST_CASE("ordered_lock locks without checking", "[disabled]") {
  ordered_mutex<std::mutex, 1> low;
  ordered_mutex<std::mutex, 2> mid;
  ordered_mutex<std::mutex, 3> high;
  std::lock_guard g(mid);
  {
    ordered_scoped_lock both(high, low);
    CHECK_FALSE(low.try_lock());
    CHECK_FALSE(high.try_lock());
  }
  CHECK(low.try_lock());
  low.unlock();
}

TEST_CASE("joins are not checked", "[disabled]") {
  ordered_mutex<std::mutex, 4> high;
  std::lock_guard g(high);
  ordered_thread t(1, [] {});
  CHECK_NOTHROW(t.join());
}

TEST_CASE("signalling without an obligation is not checked", "[disabled]") {
  ordered_condition_variable<1> cv;
  ordered_latch done(1, 1);
  ordered_promise<int> p(1);
  CHECK_NOTHROW(cv.notify_one());
  CHECK_NOTHROW(done.count_down());
  CHECK_NOTHROW(p.set_value(1));
}

TEST_CASE("a promise allocates no more than std::promise", "[disabled]") {
  if (!replaced_new) SKIP(no_replaced_new);
  const long before = allocations.load();
  { std::promise<int> plain; }
  const long plain = allocations.load() - before;
  { ordered_promise<int> ordered(1, "a promise name too long for any small string"); }
  CHECK(allocations.load() - before - plain == plain);
}

TEST_CASE("names are neither built nor stored", "[disabled]") {
  if (!replaced_new) SKIP(no_replaced_new);
  bool names_empty = false;
  const long before = allocations.load();
  {
    ordered_mutex<std::mutex, 1> unnamed_mutex;
    ordered_mutex<std::mutex, 1> named_mutex{"a mutex name too long for any small string"};
    obligation unnamed_duty(1);
    obligation named_duty(1, "an obligation name too long for any small string");
    task_scope<> unnamed_task;
    task_scope<> named_task("a task name too long for any small string");
    ordered_latch unnamed_latch(1, 1);
    ordered_latch named_latch(1, 1, "a latch name too long for any small string");
    names_empty = named_mutex.name().empty() && named_duty.name().empty() &&
                  named_task.name().empty();
  }
  const long allocated = allocations.load() - before;
  CHECK(allocated == 0);
  CHECK(names_empty);
}

TEST_CASE("waits are not checked", "[disabled]") {
  ordered_mutex<std::mutex, 4> high;
  std::lock_guard g(high);
  CHECK_NOTHROW(check_wait(1, "a wait"));

  ordered_promise<int> p(1);
  ordered_future<int> f = p.get_future();
  p.set_value(3);
  CHECK_NOTHROW(f.wait());
  CHECK(f.get() == 3);

  ordered_latch done(1, 1);
  done.count_down();
  CHECK_NOTHROW(done.wait());

  ordered_mutex<std::mutex, 5> m;
  ordered_condition_variable<1> cv;
  std::unique_lock lk(m);
  CHECK_NOTHROW(cv.wait_for(lk, std::chrono::milliseconds(1)));
  CHECK_NOTHROW(cv.wait(lk, [] { return true; }));
}

TEST_CASE("task boundaries are not checked", "[disabled]") {
  ordered_mutex<std::mutex, 1> m;
  m.lock();
  CHECK_NOTHROW([] { task_scope<> starts_holding("starts holding"); }());
  m.unlock();
  CHECK_NOTHROW([&] {
    task_scope<> leaks("leaks");
    m.lock();
  }());
  m.unlock();
}

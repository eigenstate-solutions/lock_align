#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <mutex>
#include <source_location>
#include <string>
#include <utility>

#include "lock_align/held.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_thread.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("a child thread may only lock above its level", "[thread]") {
  ordered_mutex<std::mutex, 2> two;
  ordered_mutex<std::mutex, 3> three;
  ordered_mutex<std::mutex, 4> four;
  std::atomic<int> ok{0};
  ordered_thread t(3, [&] {
    if (throws_order_error([&] { std::lock_guard g(two); })) ++ok;
    if (throws_order_error([&] { std::lock_guard g(three); })) ++ok;
    if (!throws_order_error([&] { std::lock_guard g(four); })) ++ok;
  });
  t.join();
  CHECK(ok.load() == 3);
}

TEST_CASE("join while holding at or below the thread's level", "[thread]") {
  ordered_mutex<std::mutex, 3> held;
  std::lock_guard g(held);
  ordered_thread t(3, [] {});
  CHECK_NOTHROW(t.join());
}

TEST_CASE("join while holding above the thread's level throws", "[thread]") {
  ordered_mutex<std::mutex, 4> held;
  ordered_thread t(3, "worker", [] {});
  {
    std::lock_guard g(held);
    CHECK_THROWS_WITH(t.join(), ContainsSubstring("worker"));
  }
  t.join();
}

TEST_CASE("joining a level 0 thread needs no ordered locks held", "[thread]") {
  ordered_mutex<std::mutex, 1> m;
  ordered_thread t(0, [] {});
  {
    std::lock_guard g(m);
    CHECK_THROWS_AS(t.join(), lock_order_error);
  }
  t.join();
}

TEST_CASE("a thread can inherit the spawning thread's level", "[thread]") {
  ordered_mutex<std::mutex, 3> held;
  ordered_mutex<std::mutex, 3> other_three;
  ordered_mutex<std::mutex, 4> four;
  std::atomic<int> ok{0};
  std::lock_guard g(held);
  ordered_thread t(inherit_level, [&] {
    if (current_level() == 3) ++ok;
    if (throws_order_error([&] { std::lock_guard g3(other_three); })) ++ok;
    if (!throws_order_error([&] { std::lock_guard g4(four); })) ++ok;
  });
  CHECK(t.level() == 3);
  t.join();
  CHECK(ok.load() == 3);
}

TEST_CASE("thread arguments are forwarded", "[thread]") {
  std::atomic<int> sum{0};
  ordered_thread t(
      1, [&](int a, std::string b) { sum = a + static_cast<int>(b.size()); }, 40,
      std::string("hi"));
  t.join();
  CHECK(sum.load() == 42);
}

TEST_CASE("an unnamed thread is named after the line that made it", "[thread]") {
  auto where = std::source_location::current(); ordered_thread t(1, [] {});
  CHECK(t.name() == location_name(where));
  t.join();
}

TEST_CASE("an inheriting thread is named after the line that made it", "[thread]") {
  auto where = std::source_location::current(); ordered_thread t(inherit_level, [] {});
  CHECK(t.name() == location_name(where));
  t.join();
}

TEST_CASE("a thread given an empty name is named after the line that made it", "[thread]") {
  auto where = std::source_location::current(); ordered_thread t(1, "", [] {});
  CHECK(t.name() == location_name(where));
  t.join();
}

TEST_CASE("a thread inherits one above an obligation its creator owes", "[thread]") {
  obligation duty(3);
  ordered_thread t(inherit_level, "helper", [] {});
  CHECK(t.level() == 4);
  CHECK(t.name() == "helper");
  t.join();
}

TEST_CASE("a name is not taken for the thread's function", "[thread]") {
  std::atomic<int> got{0};
  ordered_thread named(1, "named", [&](int n) { got = n; }, 7);
  named.join();
  CHECK(named.name() == "named");
  CHECK(got.load() == 7);
}

TEST_CASE("a default-constructed thread is unnamed", "[thread]") {
  ordered_thread t;
  CHECK(t.name() == "(unnamed)");
  CHECK_FALSE(t.joinable());
}

TEST_CASE("a moved thread keeps its level and name", "[thread]") {
  ordered_thread a(2, "worker", [] {});
  ordered_thread b(std::move(a));
  CHECK_FALSE(a.joinable());  // NOLINT(bugprone-use-after-move)
  CHECK(b.joinable());
  CHECK(b.level() == 2);
  CHECK(b.name() == "worker");
  ordered_thread<> c;
  c = std::move(b);
  CHECK(c.level() == 2);
  CHECK(c.name() == "worker");
  ordered_mutex<std::mutex, 3> above;
  {
    std::lock_guard g(above);
    CHECK(throws_order_error([&] { c.join(); }));  // still checked at its level
  }
  c.join();
}

TEST_CASE("a join that's a violation leaves the thread joinable", "[thread]") {
  ordered_mutex<std::mutex, 4> held;
  ordered_thread t(3, [] {});
  {
    std::lock_guard g(held);
    CHECK(throws_order_error([&] { t.join(); }));
  }
  CHECK(t.joinable());
  t.join();
  CHECK_FALSE(t.joinable());
}

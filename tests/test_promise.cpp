#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include "lock_align/held.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_promise.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;
using namespace std::chrono_literals;

TEST_CASE("future get while holding at or below its level", "[promise]") {
  ordered_promise<int> p(2, "answer");
  ordered_future<int> f = p.get_future();
  ordered_mutex<std::mutex, 2> mine;
  ordered_mutex<std::mutex, 2> theirs;
  std::atomic<bool> producer_refused{false};
  std::thread producer([&] {
    obligation duty = p.take_obligation();
    producer_refused = throws_order_error([&] { std::lock_guard g(theirs); });
    p.set_value(42);
  });
  {
    std::lock_guard g(mine);
    CHECK(f.get() == 42);
  }
  producer.join();
  CHECK(producer_refused.load());
}

TEST_CASE("future wait while holding above its level throws", "[promise]") {
  ordered_promise<void> p(2);
  ordered_future<void> f = p.get_future();
  ordered_mutex<std::mutex, 3> high;
  std::lock_guard g(high);
  CHECK_THROWS_AS(f.wait(), lock_order_error);
  CHECK(f.wait_for(0ms) == std::future_status::timeout);
}

TEST_CASE("waiting while owing work at the same level throws", "[promise]") {
  // Thread A owes p1 and waits on p2 while thread B owes p2 and waits on p1:
  // each side is refused before it can block.
  ordered_promise<int> p1(3);
  ordered_promise<int> p2(3);
  ordered_future<int> f2 = p2.get_future();
  obligation duty = p1.take_obligation();
  CHECK_THROWS_AS(f2.wait(), lock_order_error);
}

TEST_CASE("a promise, its future and its obligation share a name and level", "[promise]") {
  ordered_promise<int> p(2, "answer");
  ordered_future<int> f = p.get_future();
  obligation duty = p.take_obligation();
  CHECK(p.name() == "answer");
  CHECK(f.name() == "answer");
  CHECK(duty.name() == "answer");
  CHECK(p.level() == 2);
  CHECK(duty.level() == 2);
}

TEST_CASE("a promise can inherit its creator's level", "[promise]") {
  ordered_mutex<std::mutex, 3> three;
  ordered_mutex<std::mutex, 4> four;
  std::lock_guard g(three);
  ordered_promise<int> p(inherit_level, "inherited");
  ordered_future<int> f = p.get_future();
  CHECK(f.level() == 3);
  {
    obligation duty = p.take_obligation();
    CHECK(duty.name() == "inherited");
    p.set_value(1);
  }
  CHECK(f.get() == 1);  // holding level 3, waiting at level 3
  std::lock_guard g4(four);
  ordered_promise<int> above_four(inherit_level, "above four");
  CHECK(above_four.get_future().level() == 4);
}

TEST_CASE("setting a value without the obligation throws", "[promise][obligation_check]") {
  ordered_promise<int> p(2, "answer");
  ordered_future<int> f = p.get_future();
  try {
    p.set_value(1);
    FAIL_CHECK("set_value did not throw");
  } catch (const missing_obligation_error& e) {
    CHECK(e.info().resource().name == "answer");
    CHECK(e.info().resource().level == 2);
    CHECK(e.info().needed_for() == obligation_needed_for::set_value);
  }
  CHECK(f.wait_for(0ms) == std::future_status::timeout);  // not set
  try {
    p.set_exception(std::make_exception_ptr(std::runtime_error("failed")));
    FAIL_CHECK("set_exception did not throw");
  } catch (const missing_obligation_error& e) {
    CHECK(e.info().needed_for() == obligation_needed_for::set_exception);
  }
}

TEST_CASE("another promise's obligation doesn't count", "[promise][obligation_check]") {
  ordered_promise<int> p(2);
  ordered_promise<int> other(2);
  obligation duty = other.take_obligation();
  CHECK_THROWS_AS(p.set_value(1), missing_obligation_error);
}

TEST_CASE("an obligation still counts after its promise is moved", "[promise][obligation_check]") {
  ordered_promise<int> p(2, "answer");
  ordered_future<int> f = p.get_future();
  ordered_promise<int> moved(1);
  {
    obligation duty = p.take_obligation();
    moved = std::move(p);
    CHECK_NOTHROW(moved.set_value(7));
  }
  CHECK(f.get() == 7);
}

TEST_CASE("an obligation still counts after its promise is move-constructed",
          "[promise][obligation_check]") {
  ordered_promise<int> p(2, "answer");
  ordered_future<int> f = p.get_future();
  {
    obligation duty = p.take_obligation();
    ordered_promise<int> moved(std::move(p));
    CHECK_NOTHROW(moved.set_value(7));
  }
  CHECK(f.get() == 7);
}

TEST_CASE("an obligation outliving its promise doesn't count for a later one",
          "[promise][obligation_check]") {
  std::optional<ordered_promise<int>> p(std::in_place, 2);
  obligation duty = p->take_obligation();
  p.reset();  // a bug: duty is still held
  p.emplace(2);
  CHECK_THROWS_AS(p->set_value(1), missing_obligation_error);
}

TEST_CASE("a moved-from promise reports its missing state, not an obligation",
          "[promise][obligation_check]") {
  ordered_promise<int> p(2);
  ordered_promise<int> moved = std::move(p);
  CHECK_THROWS_AS(p.set_value(1), std::future_error);  // NOLINT: use after move is the point
}

TEST_CASE("an unchecked promise may set a value without the obligation",
          "[promise][obligation_check]") {
  ordered_promise<int, default_policy, obligation_check::unchecked> p(2);
  ordered_future<int> f = p.get_future();
  CHECK_NOTHROW(p.set_value(3));
  CHECK(f.get() == 3);
}

TEST_CASE("a returning policy lets the promise and future go ahead", "[promise][policy]") {
  using policy = counting_policy<struct tag>;
  ordered_promise<int, policy> p(1, "answer");
  ordered_future<int, policy> f = p.get_future();
  ordered_promise<int, policy> failing(1, "failure");
  ordered_future<int, policy> g = failing.get_future();
  p.set_value(5);                                                        // reported
  failing.set_exception(std::make_exception_ptr(std::runtime_error("x")));  // reported
  ordered_mutex<std::mutex, 2> high;
  std::lock_guard hold(high);
  f.wait();                                                    // reported
  CHECK(f.get() == 5);                                         // reported
  CHECK_THROWS_AS(g.get(), std::runtime_error);                // reported
  CHECK(policy::violations == 5);
}

TEST_CASE("a moved-from promise gives a plain obligation", "[promise][obligation_check]") {
  ordered_promise<int> p(2, "answer");
  ordered_promise<int> moved(std::move(p));
  ordered_mutex<std::mutex, 2> two;
  {
    obligation duty = p.take_obligation();  // NOLINT(bugprone-use-after-move)
    CHECK(held_count() == 1);
    CHECK(duty.level() == 2);
    // It still constrains the thread, but satisfies no promise.
    CHECK(throws_order_error([&] { std::lock_guard g(two); }));
    CHECK_THROWS_AS(moved.set_value(1), missing_obligation_error);
  }
  CHECK(held_count() == 0);
}

TEST_CASE("obligations from one promise can be released in any order",
          "[promise][obligation_check]") {
  ordered_promise<int> p(1);
  std::unique_ptr<obligation> first(new obligation(p.take_obligation()));
  std::unique_ptr<obligation> second(new obligation(p.take_obligation()));
  first.reset();
  CHECK(held_count() == 1);
  CHECK_NOTHROW(p.set_value(1));  // second still counts
  second.reset();
  CHECK(held_count() == 0);
}

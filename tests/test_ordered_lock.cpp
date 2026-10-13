#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "lock_align/held.hpp"
#include "lock_align/ordered_lock.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;
using namespace std::chrono_literals;

namespace {

template <class... M>
concept lockable_together = requires(M&... m) { ordered_lock(m...); };

template <class... M>
concept scoped_together = requires { typename ordered_scoped_lock<M...>; };

// A mutex whose lock() always fails.
struct failing_mutex {
  void lock() { throw std::runtime_error("lock failed"); }
  bool try_lock() { return false; }
  void unlock() {}
};

}  // namespace

static_assert(lockable_together<ordered_mutex<std::mutex, 1>, ordered_mutex<std::mutex, 2>>);
static_assert(!lockable_together<ordered_mutex<std::mutex, 1>, std::mutex>);
static_assert(!lockable_together<ordered_mutex<std::mutex, 1>, ordered_mutex<std::mutex, 0>>);
static_assert(!scoped_together<ordered_mutex<std::mutex, 1>, std::mutex>);
static_assert(std::is_same_v<ordered_scoped_lock<ordered_mutex<std::mutex, 1>>::mutex_type,
                             ordered_mutex<std::mutex, 1>>);

TEST_CASE("ordered_lock locks in level order whatever the argument order", "[ordered_lock]") {
  ordered_mutex<std::mutex, 1> one("one");
  ordered_mutex<std::mutex, 3> three("three");
  {
    ordered_scoped_lock both(three, one);
    CHECK(held_count() == 2);
    CHECK(current_level() == 3);
  }
  CHECK(held_count() == 0);
}

TEST_CASE("ordered_lock reports the lowest member whatever the argument order",
          "[ordered_lock]") {
  ordered_mutex<std::mutex, 1> one("one");
  ordered_mutex<std::mutex, 2> two("two");
  ordered_mutex<std::mutex, 3> three("three");
  std::lock_guard g(two);
  // std::scoped_lock(three, one) blocks on three and only tries one, so this
  // goes unreported unless one happens to be busy.
  try {
    ordered_lock(three, one);
    FAIL("expected a lock order error");
  } catch (const lock_order_error& e) {
    const auto& v = dynamic_cast<const lock_order_violation&>(e.info());
    CHECK(v.requested().name == "one");
    CHECK(v.held().name == "two");
  }
  CHECK(held_count() == 1);
}

TEST_CASE("ordered_lock reports at most once per call", "[ordered_lock][policy]") {
  using policy = counting_policy<struct tag>;
  ordered_mutex<std::mutex, 1, policy> one;
  ordered_mutex<std::mutex, 2, policy> two;
  ordered_mutex<std::mutex, 3, policy> three;
  std::lock_guard g(two);
  {
    ordered_scoped_lock both(three, one);  // reported; goes ahead
    CHECK(held_count() == 3);
  }
  CHECK(policy::violations == 1);
}

TEST_CASE("ordered_scoped_lock takes mutexes at the same level", "[ordered_lock]") {
  ordered_mutex<std::mutex, 2> a;
  ordered_mutex<std::recursive_mutex, 2> b;
  ordered_mutex<std::mutex, 2> c;
  {
    ordered_scoped_lock all(a, b, c);
    CHECK(held_count() == 3);
  }
  CHECK(held_count() == 0);
}

TEST_CASE("ordered_lock waits for a busy member at the same level", "[ordered_lock]") {
  ordered_mutex<std::mutex, 2> a;
  ordered_mutex<std::mutex, 2> b;
  std::latch holding(1);
  std::atomic<bool> release{false};
  std::thread other([&] {
    std::lock_guard g(b);
    holding.count_down();
    while (!release) std::this_thread::sleep_for(1ms);
  });
  holding.wait();
  std::thread releaser([&] {
    std::this_thread::sleep_for(20ms);
    release = true;
  });
  {
    ordered_scoped_lock both(a, b);
    CHECK(held_count() == 2);
  }
  releaser.join();
  other.join();
  CHECK(held_count() == 0);
}

TEST_CASE("ordered_lock waits for a busy member at a lower level", "[ordered_lock]") {
  ordered_mutex<std::mutex, 1> one;
  ordered_mutex<std::mutex, 3> three;
  std::latch holding(1);
  std::atomic<bool> release{false};
  std::thread other([&] {
    std::lock_guard g(one);
    holding.count_down();
    while (!release) std::this_thread::sleep_for(1ms);
  });
  holding.wait();
  std::thread releaser([&] {
    std::this_thread::sleep_for(20ms);
    release = true;
  });
  {
    ordered_scoped_lock both(three, one);
    CHECK(current_level() == 3);
  }
  releaser.join();
  other.join();
}

TEST_CASE("threads locking a level in opposite orders don't deadlock", "[ordered_lock]") {
  ordered_mutex<std::mutex, 2> a;
  ordered_mutex<std::mutex, 2> b;
  ordered_mutex<std::mutex, 1> c;
  std::atomic<int> count{0};
  auto work = [&](auto& first, auto& second, auto& third) {
    for (int i = 0; i < 2000; ++i) {
      ordered_scoped_lock all(first, second, third);
      ++count;
    }
  };
  std::thread t1([&] { work(a, b, c); });
  std::thread t2([&] { work(b, a, c); });
  std::thread t3([&] { work(c, b, a); });
  t1.join();
  t2.join();
  t3.join();
  CHECK(count == 6000);
}

TEST_CASE("a failure unlocks what the call locked", "[ordered_lock]") {
  ordered_mutex<std::mutex, 1> one;
  ordered_mutex<std::mutex, 2> two_a;
  ordered_mutex<std::mutex, 2> two_b;
  ordered_mutex<failing_mutex, 3> three;
  CHECK_THROWS_AS(ordered_lock(three, two_a, one, two_b), std::runtime_error);
  CHECK(held_count() == 0);
  // Everything is free again.
  REQUIRE(one.try_lock());
  REQUIRE(two_a.try_lock());
  REQUIRE(two_b.try_lock());
  two_b.unlock();
  two_a.unlock();
  one.unlock();
}

TEST_CASE("ordered_scoped_lock can adopt held mutexes", "[ordered_lock]") {
  ordered_mutex<std::mutex, 1> one;
  ordered_mutex<std::mutex, 3> three;
  ordered_lock(one, three);
  {
    ordered_scoped_lock both(std::adopt_lock, three, one);
    CHECK(held_count() == 2);
  }
  CHECK(held_count() == 0);
}

TEST_CASE("an empty set locks nothing", "[ordered_lock]") {
  ordered_lock();
  ordered_scoped_lock<> none;
  CHECK(held_count() == 0);
}

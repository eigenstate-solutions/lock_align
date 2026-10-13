#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <chrono>
#include <latch>
#include <mutex>
#include <shared_mutex>
#include <source_location>
#include <thread>
#include <type_traits>
#include <vector>

#include "lock_align/held.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;
using namespace std::chrono_literals;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("locks taken in order succeed", "[mutex]") {
  ordered_mutex<std::mutex, 1> a;
  ordered_mutex<std::mutex, 2> b;
  {
    std::lock_guard ga(a);
    std::lock_guard gb(b);
    CHECK(current_level() == 2);
    CHECK(held_count() == 2);
  }
  CHECK(current_level() == 0);
  CHECK(held_count() == 0);
}

TEST_CASE("a lock out of order throws and names both mutexes", "[mutex]") {
  ordered_mutex<std::mutex, 1> low("alpha");
  ordered_mutex<std::mutex, 2> high("beta");
  std::lock_guard g(high);
  try {
    low.lock();
    FAIL_CHECK("lock() did not throw");
  } catch (const lock_order_error& e) {
    CHECK_THAT(e.what(), ContainsSubstring("alpha") && ContainsSubstring("beta"));
    CHECK(e.info().requested().level == 1);
    CHECK(e.info().held().level == 2);
  }
  CHECK(held_count() == 1);
}

TEST_CASE("a blocking lock at the same level throws", "[mutex]") {
  ordered_mutex<std::mutex, 2> a;
  ordered_mutex<std::mutex, 2> b;
  std::lock_guard g(a);
  CHECK_THROWS_AS(b.lock(), lock_order_error);
}

TEST_CASE("releasing out of order keeps the correct level", "[mutex]") {
  ordered_mutex<std::mutex, 1> a;
  ordered_mutex<std::mutex, 2> b;
  ordered_mutex<std::mutex, 1> c;
  a.lock();
  b.lock();
  a.unlock();
  CHECK(current_level() == 2);
  CHECK_THROWS_AS(c.lock(), lock_order_error);
  b.unlock();
  CHECK(held_count() == 0);
}

TEST_CASE("try_lock at the same level is allowed", "[mutex]") {
  ordered_mutex<std::mutex, 2> a;
  ordered_mutex<std::mutex, 2> b;
  std::lock_guard g(a);
  REQUIRE(b.try_lock());
  CHECK(held_count() == 2);
  b.unlock();
}

TEST_CASE("try_lock_for on a timed mutex is not checked", "[mutex]") {
  ordered_mutex<std::timed_mutex, 1> low;
  ordered_mutex<std::timed_mutex, 2> high;
  std::lock_guard g(high);
  REQUIRE(low.try_lock_for(1ms));
  CHECK(current_level() == 2);
  low.unlock();
}

TEST_CASE("the default name is the source location", "[mutex]") {
  auto where = std::source_location::current(); ordered_mutex<std::mutex, 1> m;
  CHECK(m.name() == location_name(where));
}

TEST_CASE("a recursive mutex may be relocked by its owner", "[mutex]") {
  ordered_mutex<std::recursive_mutex, 3> r;
  ordered_mutex<std::mutex, 2> lower;
  r.lock();
  r.lock();
  CHECK(held_count() == 2);
  CHECK_THROWS_AS(lower.lock(), lock_order_error);
  r.unlock();
  r.unlock();
  CHECK(held_count() == 0);
}

namespace my {
struct recursive_mutex : std::recursive_mutex {};
struct other_recursive_mutex : std::recursive_mutex {};
}  // namespace my

// Both ways of specializing work, although is_recursive_mutex is declared in
// an inline namespace inside lock_align.
template <>
struct lock_align::is_recursive_mutex<my::recursive_mutex> : std::true_type {};
namespace lock_align {
template <>
struct is_recursive_mutex<my::other_recursive_mutex> : std::true_type {};
}  // namespace lock_align

TEST_CASE("is_recursive_mutex can be specialized for a mutex of your own", "[mutex]") {
  ordered_mutex<my::recursive_mutex, 3> r;
  ordered_mutex<my::other_recursive_mutex, 3> o;
  r.lock();
  CHECK_NOTHROW(r.lock());
  r.unlock();
  r.unlock();
  o.lock();
  CHECK_NOTHROW(o.lock());
  o.unlock();
  o.unlock();
  CHECK(held_count() == 0);
}

TEST_CASE("level 0 mutexes are untracked", "[mutex]") {
  ordered_mutex<std::mutex, 5> high;
  ordered_mutex<std::mutex, 0> unordered;
  std::lock_guard g(high);
  std::lock_guard u(unordered);
  CHECK(held_count() == 1);
}

TEST_CASE("levels can be enums", "[mutex]") {
  enum class level { database = 1, cache = 2 };
  ordered_mutex<std::mutex, level::database> db;
  ordered_mutex<std::mutex, level::cache> cache;
  static_assert(decltype(cache)::level() == 2);
  CHECK(db.level() == 1);
  std::lock_guard g(cache);
  CHECK_THROWS_AS(db.lock(), lock_order_error);
}

TEST_CASE("shared readers on many threads", "[mutex][shared]") {
  constexpr int readers = 4;
  ordered_mutex<std::shared_mutex, 2> shared;
  ordered_mutex<std::mutex, 1> lower;
  std::latch all_inside(readers);
  std::atomic<int> ok{0};
  std::vector<std::thread> threads;
  for (int i = 0; i < readers; ++i) {
    threads.emplace_back([&] {
      std::shared_lock s(shared);
      all_inside.arrive_and_wait();  // every reader holds it at once
      if (current_level() == 2 && throws_order_error([&] { std::lock_guard g(lower); })) ++ok;
    });
  }
  for (std::thread& t : threads) t.join();
  CHECK(ok.load() == readers);
}

TEST_CASE("a custom policy may let the lock go ahead", "[mutex][policy]") {
  using policy = counting_policy<struct tag>;
  ordered_mutex<std::mutex, 2, policy> high;
  ordered_mutex<std::mutex, 1, policy> low;
  std::lock_guard gh(high);
  std::lock_guard gl(low);
  CHECK(policy::violations.load() == 1);
  CHECK(held_count() == 2);
}

TEST_CASE("locking a mutex the thread already holds is reported", "[mutex]") {
  ordered_mutex<std::mutex, 1> m("m");
  std::lock_guard g(m);
  try {
    m.lock();  // would deadlock: reported before blocking
    FAIL_CHECK("expected a lock order error");
  } catch (const lock_order_error& e) {
    CHECK_THAT(e.what(), ContainsSubstring("blocking on mutex 'm' (level 1) while holding mutex 'm'"));
  }
  CHECK(held_count() == 1);
}

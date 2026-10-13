// Names are only built when asked for, so a resource with a default name
// doesn't allocate; nor does a promise's obligation token.

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <mutex>
#include <new>
#include <source_location>
#include <string>
#include <utility>

#include "lock_align/obligation.hpp"
#include "lock_align/ordered_condition_variable.hpp"
#include "lock_align/ordered_latch.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_promise.hpp"
#include "lock_align/ordered_thread.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;

// Counts every allocation in this test program.
static std::atomic<long> allocations{0};

void* operator new(std::size_t size) {
  ++allocations;
  if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

TEST_CASE("resources with default names don't allocate", "[names]") {
  // libc++'s std::condition_variable_any allocates; that isn't ours.
  long baseline = allocations;
  { std::condition_variable_any plain; }
  baseline = allocations - baseline;

  const long before = allocations;
  {
    ordered_mutex<std::mutex, 2> m;
    ordered_condition_variable<1> cv;
    ordered_latch done(1, 1);
    obligation duty(1);
    obligation owed = cv.take_obligation();
    obligation counting = done.take_obligation();
    std::unique_lock lock(m);
    cv.notify_one();
    done.count_down();
  }
  CHECK(allocations - before == baseline);
}

TEST_CASE("a promise allocates no more than std::promise", "[names][promise]") {
  long baseline = allocations;
  { std::promise<int> plain; }
  baseline = allocations - baseline;

  const long before = allocations;
  {
    ordered_promise<int> p(1);
    ordered_promise<int> moved(std::move(p));
    obligation duty = moved.take_obligation();
  }
  CHECK(allocations - before == baseline);
}

TEST_CASE("a default name is built when asked for", "[names]") {
  const auto where = std::source_location::current(); ordered_mutex<std::mutex, 1> m;
  CHECK(m.name() == location_name(where));
  CHECK(ordered_thread<>().name() == "(unnamed)");
}

TEST_CASE("a report still names a resource with a default name", "[names]") {
  const auto where = std::source_location::current(); ordered_mutex<std::mutex, 1> low;
  ordered_mutex<std::mutex, 2> high("high");
  std::lock_guard g(high);
  try {
    low.lock();
    FAIL_CHECK("expected a lock order error");
  } catch (const lock_order_error& e) {
    const auto& v = dynamic_cast<const lock_order_violation&>(e.info());
    CHECK(v.requested().name == location_name(where));
    CHECK(v.held().name == "high");
  }
}

TEST_CASE("a moved-from thread still has a name", "[names]") {
  ordered_thread a(1, "worker", [] {});
  ordered_thread b(std::move(a));
  CHECK(b.name() == "worker");
  CHECK_FALSE(a.name().empty());  // NOLINT(bugprone-use-after-move)
  b.join();
}

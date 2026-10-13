#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <mutex>
#include <thread>

#include "lock_align/held.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_latch.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;

TEST_CASE("latch wait", "[latch]") {
  ordered_latch done(1, 2, "done");
  ordered_mutex<std::mutex, 2> two;
  ordered_mutex<std::mutex, 3> three;
  std::thread worker([&] {
    obligation duty = done.take_obligation();
    done.count_down();
  });
  {
    std::lock_guard g(three);
    CHECK_THROWS_AS(done.wait(), lock_order_error);
  }
  {
    std::lock_guard g(two);
    CHECK_NOTHROW(done.wait());
  }
  worker.join();
}

TEST_CASE("a latch can inherit its creator's level", "[latch]") {
  ordered_mutex<std::mutex, 3> three;
  std::lock_guard g(three);
  ordered_latch done(1, inherit_level, "done");
  CHECK(done.level() == 3);
  CHECK(done.name() == "done");
  {
    obligation duty = done.take_obligation();
    CHECK(duty.name() == "done");
    done.count_down();
  }
  CHECK_NOTHROW(done.wait());  // holding level 3, waiting at level 3
}

TEST_CASE("counting down without the obligation throws", "[latch][obligation_check]") {
  ordered_latch done(1, 2, "done");
  try {
    done.count_down();
    FAIL_CHECK("count_down did not throw");
  } catch (const missing_obligation_error& e) {
    CHECK(e.info().resource().name == "done");
    CHECK(e.info().resource().level == 2);
    CHECK(e.info().needed_for() == obligation_needed_for::count_down);
  }
  CHECK_FALSE(done.try_wait());  // not counted down
}

TEST_CASE("another latch's obligation doesn't count", "[latch][obligation_check]") {
  ordered_latch done(1, 2);
  ordered_latch other(1, 2);
  obligation duty = other.take_obligation();
  CHECK_THROWS_AS(done.count_down(), missing_obligation_error);
}

TEST_CASE("an unchecked latch may count down without the obligation",
          "[latch][obligation_check]") {
  ordered_latch<default_policy, obligation_check::unchecked> done(1, 2);
  CHECK_NOTHROW(done.count_down());
  CHECK(done.try_wait());
}

TEST_CASE("arrive_and_wait needs the obligation and ignores it while waiting",
          "[latch][obligation_check]") {
  ordered_latch done(1, 2, "done");
  CHECK_THROWS_AS(done.arrive_and_wait(), missing_obligation_error);
  obligation duty = done.take_obligation();
  CHECK_NOTHROW(done.arrive_and_wait());
  CHECK(done.try_wait());
}

TEST_CASE("arrive_and_wait still checks other obligations", "[latch][obligation_check]") {
  ordered_latch done(1, 2, "done");
  obligation duty = done.take_obligation();
  obligation same_level(2, "same level");
  CHECK_THROWS_AS(done.arrive_and_wait(), lock_order_error);
}

TEST_CASE("waiting on a latch while holding its obligation throws", "[latch][obligation_check]") {
  ordered_latch done(1, 2, "done");
  obligation duty = done.take_obligation();
  CHECK_THROWS_AS(done.wait(), lock_order_error);
}

TEST_CASE("a returning policy lets the latch go ahead", "[latch][policy]") {
  using policy = counting_policy<struct tag>;
  ordered_latch<policy> done(2, 1, "done");
  done.count_down();  // reported: no obligation
  ordered_mutex<std::mutex, 2> high;
  std::lock_guard g(high);
  done.arrive_and_wait();  // reported twice: no obligation, and a wait while holding level 2
  done.wait();             // reported: a wait while holding level 2
  CHECK(done.try_wait());
  CHECK(policy::violations == 4);
}

TEST_CASE("obligations from one latch can be released in any order", "[latch][obligation_check]") {
  ordered_latch done(1, 1);
  std::unique_ptr<obligation> first(new obligation(done.take_obligation()));
  std::unique_ptr<obligation> second(new obligation(done.take_obligation()));
  first.reset();
  CHECK(held_count() == 1);
  CHECK_NOTHROW(done.count_down());  // second still counts
  second.reset();
  CHECK(held_count() == 0);
}

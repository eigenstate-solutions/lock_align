#include <catch2/catch_test_macros.hpp>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>

#include "lock_align/obligation.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;

TEST_CASE("check_wait for waits the library doesn't wrap", "[obligation]") {
  ordered_mutex<std::mutex, 3> m;
  std::lock_guard g(m);
  CHECK_NOTHROW(check_wait(3, "fits"));
  CHECK_THROWS_AS(check_wait(2, "too low"), lock_order_error);
}

TEST_CASE("check_wait takes any string as the name", "[obligation]") {
  ordered_mutex<std::mutex, 3> high;
  std::lock_guard g(high);
  const std::string owned = "owned name";
  const std::string_view part = std::string_view("partial name, not this").substr(0, 12);
  const auto name_reported = [](auto&& wait) {
    try {
      wait();
    } catch (const lock_order_error& e) {
      return e.info().requested().name;
    }
    return std::string("(not reported)");
  };
  CHECK(name_reported([&] { check_wait(2, owned); }) == "owned name");
  CHECK(name_reported([&] { check_wait(2, part); }) == "partial name");
}

TEST_CASE("check_wait takes an enum level", "[obligation]") {
  enum class level { low = 2, high = 3 };
  ordered_mutex<std::mutex, level::high> m;
  std::lock_guard g(m);
  CHECK_NOTHROW(check_wait(level::high, "fits"));
  CHECK_THROWS_AS(check_wait(level::low, "too low"), lock_order_error);
}

TEST_CASE("check_wait names an unnamed wait by its source location", "[obligation]") {
  ordered_mutex<std::mutex, 3> m;
  std::lock_guard g(m);
  std::source_location where;
  std::string reported;
  try {
    where = std::source_location::current(); check_wait(2);
    FAIL_CHECK("check_wait did not throw");
  } catch (const lock_order_error& e) {
    reported = e.info().requested().name;
  }
  CHECK(reported == location_name(where));
}

TEST_CASE("an unnamed obligation is named after the line that made it", "[obligation]") {
  auto where = std::source_location::current(); obligation duty(1);
  CHECK(duty.name() == location_name(where));
  CHECK(duty.level() == 1);
}

TEST_CASE("check_wait with a returning policy reports and returns", "[obligation][policy]") {
  using policy = counting_policy<struct tag>;
  ordered_mutex<std::mutex, 3> m;
  std::lock_guard g(m);
  check_wait<policy>(2, "too low");
  CHECK(policy::violations == 1);
}

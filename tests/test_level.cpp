#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>

#include "lock_align/held.hpp"
#include "lock_align/level.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_condition_variable.hpp"
#include "lock_align/ordered_latch.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_promise.hpp"
#include "lock_align/ordered_thread.hpp"
#include "held_listener.hpp"

using namespace lock_align;

namespace {

enum class layer { below = -1, data = 1 };
enum class flag : bool { off, on };
enum class letter : char { a = 'a' };

}  // namespace

static_assert(level_like<int>);
static_assert(level_like<unsigned char>);
static_assert(level_like<std::uint64_t>);
static_assert(level_like<layer>);
static_assert(!level_like<bool>);
static_assert(!level_like<char>);
static_assert(!level_like<char8_t>);
static_assert(!level_like<char32_t>);
static_assert(!level_like<flag>);
static_assert(!level_like<letter>);
static_assert(!level_like<double>);

template <auto Level>
concept mutex_level = requires { typename ordered_mutex<std::mutex, Level>; };
static_assert(mutex_level<1>);
static_assert(!mutex_level<true>);
static_assert(!mutex_level<'a'>);

static_assert(to_level(layer::data) == 1);
static_assert(to_level(std::numeric_limits<level_t>::max()) == std::numeric_limits<level_t>::max());
static_assert(!is_valid_level(-1));
static_assert(!is_valid_level(layer::below));
static_assert(!is_valid_level(std::uint64_t{1} << 32));

TEST_CASE("a level out of range throws", "[level]") {
  CHECK_THROWS_AS(to_level(-1), std::out_of_range);
  CHECK_THROWS_AS(to_level(layer::below), std::out_of_range);
  CHECK_THROWS_AS(to_level(std::uint64_t{1} << 32), std::out_of_range);
}

TEST_CASE("an obligation out of range is never held", "[level]") {
  CHECK_THROWS_AS(obligation(-1, "below"), std::out_of_range);
  CHECK(held_count() == 0);
}

TEST_CASE("a thread out of range is never started", "[level]") {
  bool ran = false;
  CHECK_THROWS_AS(ordered_thread(-1, "below", [&] { ran = true; }), std::out_of_range);
  CHECK_FALSE(ran);
}

TEST_CASE("promises and latches out of range throw", "[level]") {
  CHECK_THROWS_AS(ordered_promise<int>(layer::below, "below"), std::out_of_range);
  CHECK_THROWS_AS(ordered_latch(1, -1, "below"), std::out_of_range);
}

TEST_CASE("check_wait out of range throws", "[level]") {
  CHECK_THROWS_AS(check_wait(-1, "below"), std::out_of_range);
}

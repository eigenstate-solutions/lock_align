#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "lock_align/config.hpp"
#include "lock_align/held.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "held_listener.hpp"
#include "test_support.hpp"

using namespace lock_align;
using namespace lock_align_test;

// Lets a test make the next allocation fail.
static std::atomic<bool> fail_next_allocation{false};

void* operator new(std::size_t size) {
  if (fail_next_allocation.exchange(false)) throw std::bad_alloc();
  if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
// The record grows with new[]. Sanitizers supply their own array forms, which
// wouldn't go through the ones above, so these are replaced too.
void* operator new[](std::size_t size) { return operator new(size); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

// Well past the reserved slots, so the record has moved to the heap and grown
// more than once.
constexpr std::size_t many = 4 * held_slots_reserved + 3;

TEST_CASE("a thread may hold more entries than are reserved", "[held]") {
  ordered_mutex<std::recursive_mutex, 2> r;
  for (int round = 0; round < 2; ++round) {  // the second round regrows after release
    for (std::size_t i = 0; i < many; ++i) r.lock();
    CHECK(held_count() == many);
    for (std::size_t i = 0; i < many; ++i) r.unlock();
    CHECK(held_count() == 0);
  }
}

TEST_CASE("ordering is still checked past the reserved entries", "[held]") {
  ordered_mutex<std::recursive_mutex, 2> r;
  ordered_mutex<std::mutex, 1> low;
  ordered_mutex<std::mutex, 3> high;
  for (std::size_t i = 0; i < many; ++i) r.lock();
  CHECK(current_level() == 2);
  CHECK(throws_order_error([&] { std::lock_guard g(low); }));
  CHECK_FALSE(throws_order_error([&] { std::lock_guard g(high); }));
  CHECK(held_count() == many);
  for (std::size_t i = 0; i < many; ++i) r.unlock();
}

TEST_CASE("entries past the reserved ones can be released in any order", "[held]") {
  std::list<obligation> owed;
  for (std::size_t i = 0; i < many; ++i) owed.emplace_back(i + 1);
  CHECK(current_level() == many);
  // Release every other one, oldest first, then the newest.
  bool drop = true;
  for (auto it = owed.begin(); it != owed.end(); drop = !drop) it = drop ? owed.erase(it) : std::next(it);
  CHECK(held_count() == owed.size());
  owed.pop_back();
  CHECK(current_level() == owed.back().level());
  owed.clear();
  CHECK(held_count() == 0);
}

TEST_CASE("try_lock and try_lock_shared hold entries past the reserved ones", "[held]") {
  ordered_mutex<std::recursive_mutex, 2> r;
  for (std::size_t i = 0; i < many; ++i) REQUIRE(r.try_lock());
  CHECK(held_count() == many);
  for (std::size_t i = 0; i < many; ++i) r.unlock();

  std::vector<std::unique_ptr<ordered_mutex<std::shared_mutex, 3>>> shared;
  for (std::size_t i = 0; i < many; ++i) {
    shared.push_back(std::make_unique<ordered_mutex<std::shared_mutex, 3>>());
    REQUIRE(shared.back()->try_lock_shared());
  }
  CHECK(held_count() == many);
  for (auto& m : shared) m->unlock_shared();
  CHECK(held_count() == 0);
}

namespace {

// Fills the record exactly, so that the next entry needs it to grow.
std::list<obligation> fill_reserved_slots() {
  std::list<obligation> owed;
  for (std::size_t i = 0; i < held_slots_reserved; ++i) owed.emplace_back(0);
  return owed;
}

}  // namespace

TEST_CASE("a failed growth leaves the mutex unlocked", "[held]") {
  std::list<obligation> owed = fill_reserved_slots();
  ordered_mutex<std::mutex, 1> m;
  bool threw = false;
  fail_next_allocation = true;
  try {
    m.lock();
  } catch (const std::bad_alloc&) {
    threw = true;
  }
  fail_next_allocation = false;
  CHECK(threw);
  CHECK(held_count() == held_slots_reserved);

  bool lockable = false;  // from another thread: m isn't locked
  std::thread([&] {
    lockable = m.try_lock();
    if (lockable) m.unlock();
  }).join();
  CHECK(lockable);
  owed.clear();
}

TEST_CASE("a failed growth fails try_lock and taking an obligation", "[held]") {
  std::list<obligation> owed = fill_reserved_slots();
  ordered_mutex<std::mutex, 1> m;
  bool try_lock_threw = false;
  fail_next_allocation = true;
  try {
    (void)m.try_lock();
  } catch (const std::bad_alloc&) {
    try_lock_threw = true;
  }
  bool obligation_threw = false;
  fail_next_allocation = true;
  try {
    obligation extra(1);
  } catch (const std::bad_alloc&) {
    obligation_threw = true;
  }
  fail_next_allocation = false;
  CHECK(try_lock_threw);
  CHECK(obligation_threw);
  CHECK(held_count() == held_slots_reserved);
  owed.clear();
}

#pragma once

// ordered_mutex wraps any mutex type and checks lock ordering on every
// blocking acquire. It meets the standard Lockable requirements (and
// SharedLockable / TimedLockable when the wrapped mutex does), so it works
// with lock_guard, unique_lock, shared_lock, scoped_lock and std::lock. To
// lock several at once, prefer ordered_lock and ordered_scoped_lock
// (ordered_lock.hpp): with std::lock, which lock gets checked depends on
// argument order and timing.

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/detail/held_stack.hpp"
#include "lock_align/detail/name.hpp"
#include "lock_align/level.hpp"
#include "lock_align/violation.hpp"

LOCK_ALIGN_DETAIL_SUPPRESS_WARNINGS

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

// Specialize for your own recursive mutex types.
template <class Mutex>
struct is_recursive_mutex : std::false_type {};
template <>
struct is_recursive_mutex<std::recursive_mutex> : std::true_type {};
template <>
struct is_recursive_mutex<std::recursive_timed_mutex> : std::true_type {};

// Level 0 means unordered: the mutex is never checked or tracked. Mixing
// unordered and ordered mutexes can still deadlock.
//
// Blocking acquires (lock, lock_shared) are checked. Non-blocking and timed
// acquires are not, because they cannot wait forever; they are still
// recorded, so later blocking acquires are checked against them.
template <class Mutex, auto Level, class Policy = default_policy>
  requires level_like<decltype(Level)>
class ordered_mutex {
 public:
  using mutex_type = Mutex;
  using policy_type = Policy;
  static_assert(is_valid_level(Level),
                "an ordered_mutex's level must be between 0 and 4294967295");
  static constexpr level_t level() noexcept { return level_; }

  explicit ordered_mutex(std::string_view name = {},
                         std::source_location where = std::source_location::current())
      : name_(name, where) {}

  ordered_mutex(const ordered_mutex&) = delete;
  ordered_mutex& operator=(const ordered_mutex&) = delete;

  std::string name() const { return name_.str(); }

  void lock() {
    before_blocking();
    base_.lock();
    note_acquired();
  }

  bool try_lock() {
    reserve();
    if (!base_.try_lock()) return false;
    note_acquired();
    return true;
  }

  template <class Rep, class Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout)
    requires requires(Mutex& m) { m.try_lock_for(timeout); }
  {
    reserve();
    if (!base_.try_lock_for(timeout)) return false;
    note_acquired();
    return true;
  }

  template <class Clock, class Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& deadline)
    requires requires(Mutex& m) { m.try_lock_until(deadline); }
  {
    reserve();
    if (!base_.try_lock_until(deadline)) return false;
    note_acquired();
    return true;
  }

  void unlock() {
    note_released();
    base_.unlock();
  }

  void lock_shared()
    requires requires(Mutex& m) { m.lock_shared(); }
  {
    before_blocking();
    base_.lock_shared();
    note_acquired();
  }

  bool try_lock_shared()
    requires requires(Mutex& m) { m.try_lock_shared(); }
  {
    reserve();
    if (!base_.try_lock_shared()) return false;
    note_acquired();
    return true;
  }

  template <class Rep, class Period>
  bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& timeout)
    requires requires(Mutex& m) { m.try_lock_shared_for(timeout); }
  {
    reserve();
    if (!base_.try_lock_shared_for(timeout)) return false;
    note_acquired();
    return true;
  }

  template <class Clock, class Duration>
  bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration>& deadline)
    requires requires(Mutex& m) { m.try_lock_shared_until(deadline); }
  {
    reserve();
    if (!base_.try_lock_shared_until(deadline)) return false;
    note_acquired();
    return true;
  }

  void unlock_shared()
    requires requires(Mutex& m) { m.unlock_shared(); }
  {
    note_released();
    base_.unlock_shared();
  }

 private:
  friend struct detail::name_access;  // a condition variable checks its relock

  static constexpr level_t level_ = to_level(Level);
  static constexpr bool tracked = enabled && level_ != 0;

  void before_blocking() {
    if constexpr (tracked) {
      detail::reserve_slot();
      // The owner of a recursive mutex may re-enter it without a check.
      if (!(is_recursive_mutex<Mutex>::value && detail::held.contains(detail::id_of(this))))
        detail::check_acquire<Policy>(detail::mutex_rank(level_), name_);
    }
  }

  void reserve() {
    if constexpr (tracked) detail::reserve_slot();
  }

  void note_acquired() noexcept {
    if constexpr (tracked) detail::held.push(detail::id_of(this), detail::mutex_rank(level_), &name_);
  }

  void note_released() noexcept {
    if constexpr (tracked) detail::held.remove(detail::id_of(this));
  }

  Mutex base_;
  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::resource_name name_;
};

namespace detail {

template <class T>
struct is_ordered_mutex : std::false_type {};
template <class Mutex, auto Level, class Policy>
  requires level_like<decltype(Level)>
struct is_ordered_mutex<ordered_mutex<Mutex, Level, Policy>> : std::true_type {};

}  // namespace detail

}  // namespace lock_align

LOCK_ALIGN_DETAIL_RESTORE_WARNINGS

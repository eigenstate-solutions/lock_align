#pragma once

// A condition variable with a level of its own. Waiting on it is waiting for
// another thread to notify, so it is checked like waiting on an obligation at
// that level (see obligation.hpp). A thread that will notify holds
// take_obligation() for as long as someone may be waiting for it.

#include <condition_variable>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/detail/held_stack.hpp"
#include "lock_align/detail/name.hpp"
#include "lock_align/level.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"

LOCK_ALIGN_DETAIL_SUPPRESS_WARNINGS

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

namespace detail {

// A lock whose mutex() points to an ordered_mutex, such as
// std::unique_lock<ordered_mutex<...>>. The waits need the mutex's level.
template <class Lock>
concept ordered_mutex_lock = requires(Lock& lock) {
  requires std::is_pointer_v<decltype(lock.mutex())>;
  requires is_ordered_mutex<std::remove_cv_t<std::remove_pointer_t<decltype(lock.mutex())>>>::value;
};

}  // namespace detail

// The level must be below the level of every mutex waited with, because a
// notifier holding the obligation still has to lock that mutex.
//
// A wait releases its mutex, waits for a notification, then relocks the
// mutex. Both halves are checked before the wait starts, since the relock
// happens inside the standard library, where an exception would terminate the
// program:
//   - the wait for a notification, as a wait on an obligation at this level,
//     ignoring the mutex the wait releases. Timed waits skip this, because
//     they can't wait forever.
//   - the relock, as a lock of the mutex, ignoring the mutex itself, through
//     the mutex's own policy.
//
// With obligation_check::required, notify_one and notify_all also report a
// thread that doesn't hold an obligation taken from this condition variable.
template <auto Level, class Policy = default_policy,
          obligation_check Check = default_obligation_check>
  requires level_like<decltype(Level)>
class ordered_condition_variable {
 public:
  using policy_type = Policy;
  static_assert(is_valid_level(Level),
                "an ordered_condition_variable's level must be between 0 and 4294967295");
  static constexpr level_t level() noexcept { return level_; }

  explicit ordered_condition_variable(std::string_view name = {},
                                      std::source_location where = std::source_location::current())
      : name_(name, where) {}

  ordered_condition_variable(const ordered_condition_variable&) = delete;
  ordered_condition_variable& operator=(const ordered_condition_variable&) = delete;

  std::string name() const { return name_.str(); }

  [[nodiscard]] obligation take_obligation() const {
    return obligation(detail::from_source, level_, detail::id_of(this), name_);
  }

  void notify_one() noexcept(!checks_obligation) {
    check_notifier();
    cv_.notify_one();
  }

  void notify_all() noexcept(!checks_obligation) {
    check_notifier();
    cv_.notify_all();
  }

  template <detail::ordered_mutex_lock Lock, class... Args>
  void wait(Lock& lock, Args&&... args) {
    check_notification(lock);
    check_relock(lock);
    cv_.wait(lock, std::forward<Args>(args)...);
  }

  template <detail::ordered_mutex_lock Lock, class... Args>
  decltype(auto) wait_for(Lock& lock, Args&&... args) {
    check_relock(lock);
    return cv_.wait_for(lock, std::forward<Args>(args)...);
  }

  template <detail::ordered_mutex_lock Lock, class... Args>
  decltype(auto) wait_until(Lock& lock, Args&&... args) {
    check_relock(lock);
    return cv_.wait_until(lock, std::forward<Args>(args)...);
  }

 private:
  static constexpr level_t level_ = to_level(Level);
  static constexpr bool checks_obligation = enabled && Check == obligation_check::required;

  void check_notifier() const {
    if constexpr (checks_obligation)
      detail::check_obligation_held<Policy>(detail::id_of(this), level_, name_,
                                            obligation_needed_for::notify);
  }

  template <class Lock>
  using mutex_of = std::remove_pointer_t<decltype(std::declval<Lock&>().mutex())>;

  template <class Lock>
  void check_notification(Lock& lock) const {
    using M = mutex_of<Lock>;
    static_assert(M::level() == 0 || level_ < M::level(),
                  "an ordered_condition_variable's level must be below its mutex's level, so "
                  "that a notifier holding its obligation can still lock the mutex");
    detail::check_acquire<Policy>(detail::obligation_rank(level_), name_,
                                  detail::id_of(lock.mutex()));
  }

  template <class Lock>
  static void check_relock(Lock& lock) {
    using M = mutex_of<Lock>;
    if constexpr (enabled && M::level() != 0)
      detail::check_acquire<typename M::policy_type>(detail::mutex_rank(M::level()),
                                                      detail::name_access::of(*lock.mutex()),
                                                      detail::id_of(lock.mutex()));
  }

  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::resource_name name_;
  std::condition_variable_any cv_;
};

}  // namespace lock_align

LOCK_ALIGN_DETAIL_RESTORE_WARNINGS

#pragma once

// A promise and future pair whose producer holds an obligation until it sets
// the value, so that get() and wait() are checked like a lock (see
// obligation.hpp). Timed waits (wait_for, wait_until) are not checked,
// matching try_lock_for.

#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/detail/held_stack.hpp"
#include "lock_align/detail/name.hpp"
#include "lock_align/level.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/violation.hpp"

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

template <class T, class Policy, obligation_check Check>
class ordered_promise;

namespace detail {

// What a promise's obligations are recorded under. A promise can be moved,
// so its address won't do. Instead each promise takes a number of its own,
// which moves with it; a moved-from promise has none. The numbers are odd,
// so they never equal an id made from an address (see id_of), and never
// reused, so an obligation outliving its promise can't satisfy a later one.
// Empty when checking is off.
#if !LOCK_ALIGN_DETAIL_ENABLED
struct promise_token {
  held_id get() const noexcept { return no_id; }
};
#else
inline std::atomic<held_id> promise_tokens_taken{0};

class promise_token {
 public:
  promise_token() noexcept
      : id_((promise_tokens_taken.fetch_add(1, std::memory_order_relaxed) << 1) | 1) {}
  promise_token(promise_token&& other) noexcept : id_(std::exchange(other.id_, no_id)) {}
  promise_token& operator=(promise_token&& other) noexcept {
    id_ = std::exchange(other.id_, no_id);
    return *this;
  }

  held_id get() const noexcept { return id_; }

 private:
  held_id id_;
};
#endif

}  // namespace detail

template <class T, class Policy = default_policy>
class ordered_future {
 public:
  ordered_future() noexcept = default;

  decltype(auto) get() {
    before_wait();
    return future_.get();
  }

  void wait() const {
    before_wait();
    future_.wait();
  }

  template <class Rep, class Period>
  std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout) const {
    return future_.wait_for(timeout);
  }

  template <class Clock, class Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& deadline) const {
    return future_.wait_until(deadline);
  }

  bool valid() const noexcept { return future_.valid(); }
  level_t level() const noexcept { return level_; }
  std::string name() const { return name_.str(); }

 private:
  template <class, class, obligation_check>
  friend class ordered_promise;

  ordered_future(std::future<T> future, level_t level, detail::resource_name name)
      : future_(std::move(future)), level_(level), name_(std::move(name)) {}

  void before_wait() const {
    detail::check_acquire<Policy>(detail::obligation_rank(level_), name_);
  }

  std::future<T> future_;
  level_t level_ = 0;
  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::resource_name name_;
};

// The producing thread should hold take_obligation() until it has set the
// value, so it cannot block on anything a waiter might hold. With
// obligation_check::required, set_value and set_exception report a thread
// that doesn't. The obligation stays valid if the promise is moved.
template <class T, class Policy = default_policy, obligation_check Check = default_obligation_check>
class ordered_promise {
 public:
  template <level_like L>
  explicit ordered_promise(L level, std::string_view name = {},
                           std::source_location where = std::source_location::current())
      : level_(to_level(level)), name_(name, where) {}

  explicit ordered_promise(inherit_level_t, std::string_view name = {},
                           std::source_location where = std::source_location::current())
      : ordered_promise(detail::inherited_level(), name, where) {}

  ordered_future<T, Policy> get_future() { return {promise_.get_future(), level_, name_}; }

  [[nodiscard]] obligation take_obligation() const {
    return obligation(detail::from_source, level_, token_.get(), name_);
  }

  template <class... Args>
  void set_value(Args&&... args) {
    check_producer(obligation_needed_for::set_value);
    promise_.set_value(std::forward<Args>(args)...);
  }

  void set_exception(std::exception_ptr e) {
    check_producer(obligation_needed_for::set_exception);
    promise_.set_exception(std::move(e));
  }

  level_t level() const noexcept { return level_; }
  std::string name() const { return name_.str(); }

 private:
  static constexpr bool checks_obligation = enabled && Check == obligation_check::required;

  // A moved-from promise has no token and isn't checked: std::promise reports
  // that it has no state.
  void check_producer(obligation_needed_for needed_for) const {
    if constexpr (checks_obligation)
      if (const detail::held_id id = token_.get(); id != detail::no_id)
        detail::check_obligation_held<Policy>(id, level_, name_, needed_for);
  }

  std::promise<T> promise_;
  level_t level_;
  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::resource_name name_;
  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::promise_token token_;
};

}  // namespace lock_align

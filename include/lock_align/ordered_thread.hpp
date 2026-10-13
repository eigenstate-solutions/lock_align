#pragma once

// A thread whose body holds an obligation at the thread's level, so that
// join() is checked like a lock (see obligation.hpp).

#include <functional>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/detail/held_stack.hpp"
#include "lock_align/detail/name.hpp"
#include "lock_align/level.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/violation.hpp"

LOCK_ALIGN_DETAIL_SUPPRESS_WARNINGS

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

namespace detail {

// The level argument of an ordered_thread constructor. Converting a level, or
// inherit_level, to it records where the conversion happened: the
// constructor's call site. That is what names an unnamed thread, because a
// defaulted source_location can't follow the constructor's parameter pack.
struct thread_level {
  level_t level;
  std::source_location where;

  template <level_like L>
  thread_level(L lvl, std::source_location w = std::source_location::current())
      : level(to_level(lvl)), where(w) {}

  thread_level(inherit_level_t, std::source_location w = std::source_location::current()) noexcept
      : level(inherited_level()), where(w) {}
};

// What std::thread can run: F called with Args, after both are copied into
// the new thread. This also keeps a name from being taken for the function.
template <class F, class... Args>
concept thread_body = std::is_invocable_v<std::decay_t<F>, std::decay_t<Args>...>;

}  // namespace detail

// A std::thread whose body holds an obligation at its level.
// As with std::thread, destroying it while joinable calls std::terminate.
template <class Policy = default_policy>
class ordered_thread {
 public:
  ordered_thread() noexcept = default;

  // Named after the file:line of the call.
  template <class F, class... Args>
    requires detail::thread_body<F, Args...>
  explicit ordered_thread(detail::thread_level level, F&& f, Args&&... args)
      : ordered_thread(level, detail::resource_name({}, level.where), std::forward<F>(f),
                             std::forward<Args>(args)...) {}

  template <class F, class... Args>
    requires detail::thread_body<F, Args...>
  explicit ordered_thread(detail::thread_level level, std::string_view name, F&& f,
                                Args&&... args)
      : ordered_thread(level, detail::resource_name(name, level.where), std::forward<F>(f),
                             std::forward<Args>(args)...) {}

  ordered_thread(ordered_thread&&) noexcept = default;
  ordered_thread& operator=(ordered_thread&&) noexcept = default;

  void join() {
    detail::check_acquire<Policy>(detail::obligation_rank(level_), name_);
    thread_.join();
  }

  void detach() { thread_.detach(); }
  bool joinable() const noexcept { return thread_.joinable(); }
  std::thread::id get_id() const noexcept { return thread_.get_id(); }
  level_t level() const noexcept { return level_; }
  std::string name() const { return name_.str(); }

 private:
  template <class F, class... Args>
  ordered_thread(detail::thread_level level, detail::resource_name name, F&& f,
                       Args&&... args)
      : level_(level.level),
        name_(std::move(name)),
        thread_([level = level_, name = name_, f = std::forward<F>(f),
                 ... args = std::forward<Args>(args)]() mutable {
          obligation running(detail::from_source, level, detail::no_id, name);
          std::invoke(std::move(f), std::move(args)...);
        }) {}

  level_t level_ = 0;
  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::resource_name name_;
  std::thread thread_;
};

}  // namespace lock_align

LOCK_ALIGN_DETAIL_RESTORE_WARNINGS

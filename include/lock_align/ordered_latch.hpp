#pragma once

// A latch whose counting threads hold an obligation until they count down,
// so that wait() and arrive_and_wait() are checked like a lock (see
// obligation.hpp).

#include <cstddef>
#include <latch>
#include <source_location>
#include <string>
#include <string_view>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/detail/held_stack.hpp"
#include "lock_align/detail/name.hpp"
#include "lock_align/level.hpp"
#include "lock_align/obligation.hpp"
#include "lock_align/violation.hpp"

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

// Threads that count down should hold take_obligation() until they have.
// With obligation_check::required, count_down and arrive_and_wait report a
// thread that doesn't. arrive_and_wait's wait ignores obligations taken from
// this latch, since arriving fulfils them; a plain wait() doesn't.
template <class Policy = default_policy, obligation_check Check = default_obligation_check>
class ordered_latch {
 public:
  template <level_like L>
  explicit ordered_latch(std::ptrdiff_t expected, L level, std::string_view name = {},
                               std::source_location where = std::source_location::current())
      : latch_(expected), level_(to_level(level)), name_(name, where) {}

  explicit ordered_latch(std::ptrdiff_t expected, inherit_level_t,
                               std::string_view name = {},
                               std::source_location where = std::source_location::current())
      : ordered_latch(expected, detail::inherited_level(), name, where) {}

  void count_down(std::ptrdiff_t n = 1) {
    check_counter();
    latch_.count_down(n);
  }

  bool try_wait() const noexcept { return latch_.try_wait(); }

  void wait() const {
    before_wait(detail::no_id);
    latch_.wait();
  }

  void arrive_and_wait(std::ptrdiff_t n = 1) {
    check_counter();
    before_wait(detail::id_of(this));
    latch_.arrive_and_wait(n);
  }

  [[nodiscard]] obligation take_obligation() const {
    return obligation(detail::from_source, level_, detail::id_of(this), name_);
  }

  level_t level() const noexcept { return level_; }
  std::string name() const { return name_.str(); }

 private:
  static constexpr bool checks_obligation = enabled && Check == obligation_check::required;

  // exclude: this, to ignore obligations taken from this latch.
  void before_wait(detail::held_id exclude) const {
    detail::check_acquire<Policy>(detail::obligation_rank(level_), name_, exclude);
  }

  void check_counter() const {
    if constexpr (checks_obligation)
      detail::check_obligation_held<Policy>(detail::id_of(this), level_, name_,
                                            obligation_needed_for::count_down);
  }

  std::latch latch_;
  level_t level_;
  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::resource_name name_;
};

}  // namespace lock_align

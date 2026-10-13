#pragma once

// Waiting on another thread follows the same rule as locking a mutex.
// Whoever will do the work holds an obligation at level N for as long as
// someone may be waiting, so it can only block on mutexes above N. Waiting
// for that work is a blocking acquire at level N, so the waiter may hold
// mutexes up to and including N, never above.
//
// obligation is the producer's side; check_wait is the waiter's side for waits
// this library doesn't wrap.

#include <source_location>
#include <string>
#include <string_view>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/detail/held_stack.hpp"
#include "lock_align/detail/name.hpp"
#include "lock_align/level.hpp"
#include "lock_align/violation.hpp"

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

// Whether a condition variable, latch or promise reports a thread that
// notifies, counts down or sets a value without holding an obligation taken
// from it.
enum class obligation_check { unchecked, required };

inline constexpr obligation_check default_obligation_check =
    obligation_check::LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK;

namespace detail {

// Selects the obligation constructor that take_obligation() uses.
struct from_source_t {
  explicit from_source_t() = default;
};
inline constexpr from_source_t from_source{};

// What an obligation's entry is recorded under: the obligation itself, or
// the resource it was taken from. Empty when checking is off.
#if !LOCK_ALIGN_DETAIL_ENABLED
struct obligation_id {
  void set(held_id) noexcept {}
  held_id get() const noexcept { return no_id; }
};
#else
struct obligation_id {
  held_id value = no_id;
  void set(held_id id) noexcept { value = id; }
  held_id get() const noexcept { return value; }
};
#endif

}  // namespace detail

// Declares that the current thread owes work at some level that another
// thread may be waiting for. While it lives, this thread may only block on
// mutexes above that level. Level 0 is meaningful here: the owing thread may
// lock any ordered mutex, but a waiter must hold no ordered mutex.
class obligation {
 public:
  template <level_like L>
  explicit obligation(L level, std::string_view name = {},
                      std::source_location where = std::source_location::current())
      : level_(to_level(level)), name_(name, where) {
    record(detail::id_of(this));
  }

  // Used by take_obligation(), and by ordered_thread with no source. The
  // entry is recorded under source, so the resource can tell whether the
  // thread holds an obligation taken from it. Copies the resource's name,
  // which doesn't allocate unless it was given one too long to store inline.
  obligation(detail::from_source_t, level_t level, detail::held_id source,
             const detail::resource_name& name)
      : level_(level), name_(name) {
    record(source != detail::no_id ? source : detail::id_of(this));
  }

  ~obligation() {
    if constexpr (enabled) detail::held.remove(id_.get(), &name_);
  }

  obligation(const obligation&) = delete;
  obligation& operator=(const obligation&) = delete;

  level_t level() const noexcept { return level_; }
  std::string name() const { return name_.str(); }

 private:
  void record(detail::held_id id) {
    if constexpr (enabled) {
      id_.set(id);
      detail::reserve_slot();
      detail::held.push(id, detail::obligation_rank(level_), &name_);
    }
  }

  level_t level_;
  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::resource_name name_;
  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::obligation_id id_;
};

namespace detail {

// Reports the thread if it holds no obligation taken from source.
template <class Policy>
void check_obligation_held(held_id source, level_t level, const resource_name& name,
                           obligation_needed_for needed_for) {
  if constexpr (enabled) {
    if (!held.contains(source))
      Policy::on_violation(missing_obligation_violation(
          {name.str(), level, resource_kind::obligation}, needed_for));
  }
}

}  // namespace detail

// Call before blocking on something that another thread completes while
// holding an obligation at this level, for waits this library doesn't wrap.
// An empty name is reported as the file:line of the call.
template <class Policy = default_policy, level_like L>
void check_wait(L level, std::string_view name = {},
                std::source_location where = std::source_location::current()) {
  if constexpr (enabled) {
    const detail::rank_t rank = detail::obligation_rank(to_level(level));
    if (const detail::held_entry* top = detail::conflict(rank)) {
      // Only build the name when there is something to report.
      detail::report_conflict<Policy>(
          rank, name.empty() ? detail::location_name(where) : std::string(name), *top);
    }
  }
}

}  // namespace lock_align

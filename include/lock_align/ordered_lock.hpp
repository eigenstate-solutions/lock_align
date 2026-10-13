#pragma once

// ordered_lock and ordered_scoped_lock lock several ordered mutexes at once,
// like std::lock and std::scoped_lock, but in a known order: by rising level,
// with deadlock-avoiding back-off only among mutexes at the same level. So
// the lowest-level mutex is always the first one blocked on, and the check
// gives the same answer whatever the argument order or timing.

#include <array>
#include <cstddef>
#include <mutex>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/level.hpp"
#include "lock_align/ordered_mutex.hpp"

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

namespace detail {

// An ordered_mutex with a level other than 0. Plain mutexes and level-0
// mutexes are not tracked, so they can't be put in order; use std::lock or
// std::scoped_lock for those.
template <class T>
concept orderable_mutex = is_ordered_mutex<T>::value && T::level() != 0;

// One mutex of the set, with its type erased so that one loop can lock
// mutexes of different types.
struct lock_ops {
  void* mutex;
  void (*lock)(void*);
  bool (*try_lock)(void*);
  void (*unlock)(void*);
  level_t level;
};

template <class M>
lock_ops ops_of(M& m) noexcept {
  return {&m, [](void* p) { static_cast<M*>(p)->lock(); },
          [](void* p) { return static_cast<M*>(p)->try_lock(); },
          [](void* p) { static_cast<M*>(p)->unlock(); }, M::level()};
}

// The argument indices sorted by level; equal levels keep argument order.
template <class... M>
consteval std::array<std::size_t, sizeof...(M)> level_order() {
  constexpr std::size_t n = sizeof...(M);
  const std::array<level_t, n> levels{M::level()...};
  std::array<std::size_t, n> order{};
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t j = i;
    for (; j > 0 && levels[order[j - 1]] > levels[i]; --j) order[j] = order[j - 1];
    order[j] = i;
  }
  return order;
}

inline void unlock_reversed(std::span<const lock_ops> ms) noexcept {
  for (std::size_t i = ms.size(); i-- > 0;) ms[i].unlock(ms[i].mutex);
}

// Locks mutexes that share one level. Blocks on one of them while holding
// none of the others, then tries the rest; if one is busy, releases the group
// and blocks on that one instead. Holds none of the group if it throws.
inline void lock_group(std::span<const lock_ops> group) {
  const std::size_t n = group.size();
  std::size_t first = 0;
  for (;;) {
    // The members in the order this round takes them.
    auto at = [&](std::size_t k) -> const lock_ops& { return group[(first + k) % n]; };
    auto release = [&](std::size_t count) noexcept {
      for (std::size_t k = count; k-- > 0;) at(k).unlock(at(k).mutex);
    };
    at(0).lock(at(0).mutex);
    std::size_t k = 1;
    try {
      for (; k < n; ++k)
        if (!at(k).try_lock(at(k).mutex)) break;
    } catch (...) {
      release(k);
      throw;
    }
    if (k == n) return;
    release(k);
    first = (first + k) % n;
  }
}

// Locks mutexes sorted by level, one level at a time. Holds none of them if
// it throws.
inline void lock_sorted(std::span<const lock_ops> ms) {
  std::size_t locked = 0;
  try {
    while (locked < ms.size()) {
      std::size_t end = locked + 1;
      while (end < ms.size() && ms[end].level == ms[locked].level) ++end;
      lock_group(ms.subspan(locked, end - locked));
      locked = end;
    }
  } catch (...) {
    unlock_reversed(ms.first(locked));
    throw;
  }
}

template <class... M>
struct scoped_lock_types {};
template <class M>
struct scoped_lock_types<M> {
  using mutex_type = M;
};

}  // namespace detail

// Locks every mutex, in rising order of level. Mutexes at the same level are
// locked together without deadlock, as std::lock does. Each blocking lock is
// checked, and reported through that mutex's policy. If anything throws, the
// mutexes this call locked are unlocked again.
//
// Passing the same mutex twice is undefined, as with std::lock.
template <detail::orderable_mutex... M>
void ordered_lock(M&... m) {
  if constexpr (sizeof...(M) > 0) {
    constexpr auto order = detail::level_order<M...>();
    const std::array<detail::lock_ops, sizeof...(M)> given{detail::ops_of(m)...};
    std::array<detail::lock_ops, sizeof...(M)> sorted{};
    for (std::size_t i = 0; i < sorted.size(); ++i) sorted[i] = given[order[i]];
    detail::lock_sorted(sorted);
  }
}

// Holds every mutex for a scope, locking them with ordered_lock. Unlocks
// them in the reverse of the order they were locked.
template <detail::orderable_mutex... M>
class ordered_scoped_lock : public detail::scoped_lock_types<M...> {
 public:
  explicit ordered_scoped_lock(M&... m) : mutexes_(m...) { ordered_lock(m...); }

  // Takes over mutexes the calling thread has already locked.
  explicit ordered_scoped_lock(std::adopt_lock_t, M&... m) noexcept : mutexes_(m...) {}

  ~ordered_scoped_lock() {
    unlock_reversed(std::make_index_sequence<sizeof...(M)>());
  }

  ordered_scoped_lock(const ordered_scoped_lock&) = delete;
  ordered_scoped_lock& operator=(const ordered_scoped_lock&) = delete;

 private:
  template <std::size_t... I>
  void unlock_reversed(std::index_sequence<I...>) noexcept {
    if constexpr (sizeof...(M) > 0) {
      constexpr auto order = detail::level_order<M...>();
      (..., std::get<order[sizeof...(M) - 1 - I]>(mutexes_).unlock());
    }
  }

  std::tuple<M&...> mutexes_;
};

}  // namespace lock_align

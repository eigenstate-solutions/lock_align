#pragma once

// Levels, and the ranks that put mutexes and obligations into one order.
//
// The rule: every resource a thread can block on has a rank. A thread may
// block on a resource only if everything it currently holds ranks strictly
// below that resource. Because all threads obey one total order, no cycle of
// waiters (and so no deadlock) can form among tracked resources.

#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

using level_t = std::uint32_t;

namespace detail {

// Integer types that can hold a level: not bool, and not a character type.
// These are exactly the types std::in_range accepts.
template <class T>
concept level_integer =
    std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool> &&
    !std::is_same_v<std::remove_cv_t<T>, char> && !std::is_same_v<std::remove_cv_t<T>, wchar_t> &&
    !std::is_same_v<std::remove_cv_t<T>, char8_t> &&
    !std::is_same_v<std::remove_cv_t<T>, char16_t> &&
    !std::is_same_v<std::remove_cv_t<T>, char32_t>;

template <class L>
constexpr auto level_value(L level) noexcept {
  if constexpr (std::is_enum_v<L>)
    return static_cast<std::underlying_type_t<L>>(level);
  else
    return level;
}

}  // namespace detail

// An integer type, or an enum whose underlying type is one. bool and the
// character types are not levels.
template <class L>
concept level_like = detail::level_integer<L> ||
                     (std::is_enum_v<L> && detail::level_integer<std::underlying_type_t<L>>);

// Whether level is between 0 and the largest level_t.
template <level_like L>
constexpr bool is_valid_level(L level) noexcept {
  return std::in_range<level_t>(detail::level_value(level));
}

// Throws std::out_of_range if level is negative or too large for level_t.
// In a constant expression, such as a template argument, that is a compile
// error instead.
template <level_like L>
constexpr level_t to_level(L level) {
  if (!is_valid_level(level))
    throw std::out_of_range("lock_align: a level must be between 0 and 4294967295");
  return static_cast<level_t>(detail::level_value(level));
}

// Passed in place of a level to ordered_thread, ordered_promise or
// ordered_latch: use the lowest level the creating thread could wait at while
// still holding everything it holds now.
struct inherit_level_t {
  explicit inherit_level_t() = default;
};
inline constexpr inherit_level_t inherit_level{};

namespace detail {

// Mutexes and obligations share one total order. A mutex at level N has rank
// 2N; an obligation at level N has rank 2N+1, just above mutexes of the same
// level. So whoever owes an obligation at level N may only block on mutexes
// above N, and a waiter on it may hold mutexes up to and including N.
using rank_t = std::uint64_t;

constexpr rank_t mutex_rank(level_t level) noexcept { return rank_t{level} * 2; }
constexpr rank_t obligation_rank(level_t level) noexcept { return rank_t{level} * 2 + 1; }
constexpr level_t level_of(rank_t rank) noexcept { return static_cast<level_t>(rank / 2); }
constexpr bool is_obligation(rank_t rank) noexcept { return (rank & 1) != 0; }

}  // namespace detail

}  // namespace lock_align

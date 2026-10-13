#pragma once

// What the calling thread holds, for tests and diagnostics.

#include <cstddef>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/detail/held_stack.hpp"
#include "lock_align/level.hpp"

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

// Highest level this thread holds (mutex or obligation), or 0 if none.
inline level_t current_level() noexcept {
  const detail::held_entry* top = detail::held.highest();
  return top == nullptr ? 0 : detail::level_of(top->rank);
}

// Number of tracked entries this thread holds.
inline std::size_t held_count() noexcept { return detail::held.size(); }

}  // namespace lock_align

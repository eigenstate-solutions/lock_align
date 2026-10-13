#pragma once

// The per-thread record of held mutexes and obligations, and the ordering
// check every blocking operation runs against it.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/detail/name.hpp"
#include "lock_align/level.hpp"
#include "lock_align/violation.hpp"

LOCK_ALIGN_DETAIL_SUPPRESS_WARNINGS

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI::detail {

// What an entry in the held record is recorded under. An id is only ever
// compared, never turned back into a pointer. 0 is no id.
using held_id = std::uint64_t;
inline constexpr held_id no_id = 0;

// The id of a resource, made from its address. Every type recorded this way
// is at least 2-byte aligned, so these ids are even; the ids promises use
// (see ordered_promise.hpp) are odd, so the two never collide.
template <class T>
held_id id_of(const T* resource) noexcept {
  static_assert(alignof(T) >= 2, "a resource's id needs its address's low bit to be 0");
  static_assert(sizeof(std::uintptr_t) <= sizeof(held_id), "an address must fit in a held_id");
  return static_cast<held_id>(reinterpret_cast<std::uintptr_t>(resource));
}

struct held_entry {
  held_id id;
  rank_t rank;
  const resource_name* name;  // the resource's, which outlives its entry
};

// What one thread currently holds. Entries may be released in any order.
//
// The first held_slots_reserved entries live inside the object. Beyond that
// the entries move to the heap, which grows as needed and is freed when the
// thread holds nothing again. There is deliberately no destructor: a
// thread_local with one would be destroyed at thread exit, before anything
// else that might still lock or unlock, and would need an initialization
// check on every use. A thread that exits holding more than the reserved
// slots leaks their storage, but that is already a bug.
class held_stack {
 public:
  static constexpr std::size_t reserved = held_slots_reserved;

  held_stack() = default;
  held_stack(const held_stack&) = delete;
  held_stack& operator=(const held_stack&) = delete;

  std::size_t size() const noexcept { return size_; }

  // Makes room for one more entry, so that push can't fail. Called before
  // acquiring, so a failed allocation leaves nothing locked.
  void reserve_one() {
    if (size_ == capacity_) grow();
  }

  // Callers call reserve_one() before acquiring, so this cannot fail.
  void push(held_id id, rank_t rank, const resource_name* name) noexcept {
    data()[size_++] = held_entry{id, rank, name};
  }

  // Removes the most recent entry for id (a recursive mutex has several).
  // Obligations taken from the same resource share an id, so each passes its
  // own name too, to remove exactly its own entry.
  void remove(held_id id, const resource_name* name = nullptr) noexcept {
    held_entry* entries = data();
    for (std::size_t i = size_; i-- > 0;) {
      if (entries[i].id == id && (name == nullptr || entries[i].name == name)) {
        if (id == left_behind_) left_behind_ = no_id;
        for (std::size_t j = i + 1; j < size_; ++j) entries[j - 1] = entries[j];
        --size_;
        if (size_ == 0) release_heap();
        return;
      }
    }
  }

  // Every entry, oldest first.
  std::span<const held_entry> entries() const noexcept { return {data(), size_}; }

  // The mutex a task ended still holding, as task_scope noted it, until its
  // entry is removed. Forgetting it here, rather than when a later task
  // starts, means a different mutex at the same address, or the same one
  // locked again, is never taken for what the task left.
  void mark_left_behind(held_id id) noexcept { left_behind_ = id; }
  held_id left_behind() const noexcept { return left_behind_; }

  bool contains(held_id id) const noexcept {
    for (const held_entry& e : entries())
      if (e.id == id) return true;
    return false;
  }

  // The highest-ranked entry for which pred is true, or nullptr if there is
  // none. Of entries with the same rank, the oldest. pred must not throw.
  template <class Pred>
  const held_entry* highest_where(Pred pred) const noexcept {
    const held_entry* best = nullptr;
    for (const held_entry& e : entries())
      if (pred(e) && (best == nullptr || e.rank > best->rank)) best = &e;
    return best;
  }

  // The highest-ranked mutex entry, or nullptr if no mutex is held.
  const held_entry* highest_mutex() const noexcept {
    return highest_where([](const held_entry& e) { return !is_obligation(e.rank); });
  }

  // The highest-ranked entry, ignoring any belonging to exclude.
  const held_entry* highest(held_id exclude = no_id) const noexcept {
    return highest_where([exclude](const held_entry& e) { return e.id != exclude; });
  }

 private:
  held_entry* data() noexcept { return heap_ != nullptr ? heap_ : inline_.data(); }
  const held_entry* data() const noexcept { return heap_ != nullptr ? heap_ : inline_.data(); }

  // Doubles the capacity, moving the entries to the heap.
  void grow() {
    const std::size_t capacity = capacity_ == 0 ? 1 : capacity_ * 2;
    held_entry* heap = new held_entry[capacity];
    std::copy_n(data(), size_, heap);
    delete[] heap_;
    heap_ = heap;
    capacity_ = capacity;
  }

  void release_heap() noexcept {
    delete[] heap_;
    heap_ = nullptr;
    capacity_ = reserved;
  }

  std::array<held_entry, reserved> inline_{};
  held_entry* heap_ = nullptr;
  std::size_t capacity_ = reserved;
  std::size_t size_ = 0;
  held_id left_behind_ = no_id;
};

static_assert(std::is_trivially_destructible_v<held_stack>);

inline constinit thread_local held_stack held;

inline void reserve_slot() { held.reserve_one(); }

// The held entry that makes blocking on a resource of the given rank a
// violation, or nullptr if blocking is allowed.
inline const held_entry* conflict(rank_t rank, held_id exclude = no_id) noexcept {
  const held_entry* top = held.highest(exclude);
  return top != nullptr && top->rank >= rank ? top : nullptr;
}

// A held entry as a violation reports it.
inline resource_info resource_of(const held_entry& e) {
  return {e.name->str(), level_of(e.rank), kind_of(e.rank)};
}

template <class Policy>
void report_conflict(rank_t rank, std::string name, const held_entry& top) {
  Policy::on_violation(
      lock_order_violation({std::move(name), level_of(rank), kind_of(rank)}, resource_of(top)));
}

// Call before blocking on a resource of the given rank. The name is only
// built if there is something to report.
template <class Policy>
void check_acquire(rank_t rank, const resource_name& name, held_id exclude = no_id) {
  if constexpr (enabled) {
    if (const held_entry* top = conflict(rank, exclude)) report_conflict<Policy>(rank, name.str(), *top);
  }
}

// The lowest level of an obligation this thread may wait on while holding
// what it holds now. What inherit_level resolves to.
inline level_t inherited_level() noexcept {
  const held_entry* top = held.highest();
  if (top == nullptr) return 0;
  return level_of(top->rank) + (is_obligation(top->rank) ? 1 : 0);
}

}  // namespace lock_align::detail

LOCK_ALIGN_DETAIL_RESTORE_WARNINGS

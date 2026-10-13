#pragma once

// Task boundaries: checks that a pool worker holds no mutex when it starts a
// task and that the task releases everything it took.

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <span>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/detail/held_stack.hpp"
#include "lock_align/detail/name.hpp"
#include "lock_align/level.hpp"
#include "lock_align/violation.hpp"

LOCK_ALIGN_DETAIL_SUPPRESS_WARNINGS

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

namespace detail {

// What a task_scope remembers at entry to check at exit. Nothing with
// LOCK_ALIGN_DISABLE.
#if !LOCK_ALIGN_DETAIL_ENABLED
struct task_entry_state {};
#else
class task_entry_state {
 public:
  task_entry_state() {
    const std::span<const held_entry> now = held.entries();
    count_ = now.size();
    if (count_ > inline_.size()) heap_ = std::make_unique<entry_key[]>(count_);
    entry_key* keys = snapshot();
    for (std::size_t i = 0; i < count_; ++i) keys[i] = key_of(now[i]);
  }

  // Whether an exception is unwinding through the scope.
  bool unwinding() const noexcept { return std::uncaught_exceptions() != exceptions_; }

  // The newest entry the task took and still holds, or nullptr if none.
  const held_entry* newest_taken() const noexcept {
    const std::span<const held_entry> now = held.entries();
    for (std::size_t i = now.size(); i-- > 0;)
      if (taken(now, i)) return &now[i];
    return nullptr;
  }

  // The highest-ranked mutex the task took and still holds, or nullptr.
  const held_entry* highest_taken_mutex() const noexcept {
    const std::span<const held_entry> now = held.entries();
    const held_entry* best = nullptr;
    for (std::size_t i = 0; i < now.size(); ++i)
      if (!is_obligation(now[i].rank) && (best == nullptr || now[i].rank > best->rank) &&
          taken(now, i))
        best = &now[i];
    return best;
  }

 private:
  // An entry's identity: obligations taken from one resource share an id but
  // not a name. A recursive mutex has several entries with the same key.
  struct entry_key {
    held_id id;
    const resource_name* name;
    bool operator==(const entry_key&) const = default;
  };
  static entry_key key_of(const held_entry& e) noexcept { return {e.id, e.name}; }

  entry_key* snapshot() noexcept { return heap_ ? heap_.get() : inline_.data(); }
  const entry_key* snapshot() const noexcept { return heap_ ? heap_.get() : inline_.data(); }

  // Whether now[i] is one the task took: counting it, the thread holds more
  // entries like it than it did at entry. So releasing an entry held at entry
  // and taking it again doesn't count.
  bool taken(std::span<const held_entry> now, std::size_t i) const noexcept {
    const entry_key key = key_of(now[i]);
    std::size_t held_now = 0;
    for (std::size_t j = 0; j <= i; ++j)
      if (key_of(now[j]) == key) ++held_now;
    const entry_key* keys = snapshot();
    for (std::size_t j = 0; j < count_; ++j)
      if (keys[j] == key && --held_now == 0) return false;
    return true;
  }

  int exceptions_ = std::uncaught_exceptions();
  std::size_t count_ = 0;
  // Workers rarely hold more than an obligation or two at entry, so a few
  // keys are kept inline and only a longer snapshot allocates.
  std::array<entry_key, 4> inline_{};
  std::unique_ptr<entry_key[]> heap_;
};

// The last task on this thread that ended still holding a mutex, so that a
// later task's entry check can name it. Trivially destructible, like held:
// a thread that exits with a note still set leaks the name, but it also
// exits holding a mutex.
//
// Which mutex the task left is marked in held itself, which forgets it as
// soon as that mutex's entry is removed.
struct leftover_note {
  std::string* task = nullptr;  // the task's name

  // Records that task left the mutex at m. Allocates, so a failure just
  // leaves no note.
  void record(const held_entry& m, const std::string& name) noexcept {
    try {
      std::string* copy = new std::string(name);
      clear();
      held.mark_left_behind(m.id);
      task = copy;
    } catch (...) {
    }
  }

  void clear() noexcept {
    delete task;
    task = nullptr;
    held.mark_left_behind(no_id);
  }

  // Frees the name once its mutex has been released.
  void forget_if_released() noexcept {
    if (task != nullptr && held.left_behind() == no_id) clear();
  }

  // The task that left the mutex at m, or "" if no task did.
  std::string left(const held_entry& m) const {
    return task != nullptr && held.left_behind() == m.id ? *task : std::string();
  }
};

inline constinit thread_local leftover_note leftover;
#endif

}  // namespace detail

// Marks a task boundary for a thread pool or executor. A worker must hold no
// tracked mutex when it starts a task, and the task must have released
// everything it took by the time it ends; either lapse is reported to Policy.
//
// Obligations the worker already owes, such as an ordered_thread's own, are
// allowed at entry: the task really does run under them, and they keep
// constraining it. The held stack is never hidden or swapped, so whatever the
// thread holds, the checks inside the task see. The exit check reports only
// entries the task added.
//
// While an exception is unwinding through the scope, the exit check runs
// only if Policy::on_violation is noexcept, so a throwing policy can't
// terminate the program. Either way, a task that ends holding a mutex is
// remembered, and a later task's entry check names it as the one that left
// the mutex behind.
template <class Policy = default_policy>
class task_scope {
 public:
  explicit task_scope(std::string_view name = {},
                      std::source_location where = std::source_location::current())
      : name_(name, where) {
#if LOCK_ALIGN_DETAIL_ENABLED
    detail::leftover.forget_if_released();
    if (const detail::held_entry* m = detail::held.highest_mutex())
      Policy::on_violation(
          task_entry_violation(name_.str(), detail::resource_of(*m), detail::leftover.left(*m)));
#endif
  }

  ~task_scope() noexcept(false) {
#if LOCK_ALIGN_DETAIL_ENABLED
    const detail::held_entry* newest = entry_.newest_taken();
    if (newest == nullptr) return;
    if (const detail::held_entry* m = entry_.highest_taken_mutex())
      detail::leftover.record(*m, name_.str());
    if (!entry_.unwinding()) {
      report_exit(*newest);
    } else if constexpr (reports_while_unwinding) {
      // Building the report may still throw std::bad_alloc.
      try {
        report_exit(*newest);
      } catch (...) {
      }
    }
#endif
  }

  task_scope(const task_scope&) = delete;
  task_scope& operator=(const task_scope&) = delete;

  std::string name() const { return name_.str(); }

 private:
  static constexpr bool reports_while_unwinding =
      noexcept(Policy::on_violation(std::declval<const task_exit_violation&>()));

  void report_exit(const detail::held_entry& newest) const {
    Policy::on_violation(task_exit_violation(name_.str(), detail::resource_of(newest)));
  }

  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::resource_name name_;
  LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS detail::task_entry_state entry_;
};

}  // namespace lock_align

LOCK_ALIGN_DETAIL_RESTORE_WARNINGS

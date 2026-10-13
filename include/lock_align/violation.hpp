#pragma once

// What a violation reports, and the policies that decide what happens next.

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"
#include "lock_align/level.hpp"
#include "lock_align/violation_fwd.hpp"

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

enum class resource_kind { mutex, obligation };

namespace detail {

constexpr resource_kind kind_of(rank_t rank) noexcept {
  return is_obligation(rank) ? resource_kind::obligation : resource_kind::mutex;
}

}  // namespace detail

// One side of a violation: the resource requested, or the one held.
struct resource_info {
  std::string name;
  level_t level;
  resource_kind kind;

  // For example "mutex 'accounts' (level 1)".
  std::string describe() const {
    return std::string(kind == resource_kind::mutex ? "mutex '" : "obligation '") + name +
           "' (level " + std::to_string(level) + ")";
  }
};

// Describes one violation. Each kind of violation is its own class, and a
// policy may overload on_violation for any of them; the one it is called
// with is always the most derived.
class violation {
 public:
  virtual ~violation() = default;

  virtual std::string message() const = 0;

  // Throws the violation_error that matches this violation.
  [[noreturn]] virtual void throw_error() const = 0;

 protected:
  violation() = default;
  // Protected, so a violation can't be copied by its base class and sliced.
  violation(const violation&) = default;
  violation& operator=(const violation&) = default;
};

// Blocking on a resource while holding one at or above it.
class lock_order_violation final : public violation {
 public:
  lock_order_violation(resource_info requested, resource_info held)
      : requested_(std::move(requested)), held_(std::move(held)) {}

  // What the thread was about to block on.
  const resource_info& requested() const noexcept { return requested_; }

  // What the thread held that made blocking a violation.
  const resource_info& held() const noexcept { return held_; }

  std::string message() const override {
    return "lock order violation: blocking on " + requested_.describe() + " while holding " +
           held().describe();
  }

  [[noreturn]] void throw_error() const override;

 private:
  resource_info requested_;
  resource_info held_;
};

// A task_scope's rules broken: see task_entry_violation and
// task_exit_violation.
class task_violation : public violation {
 public:
  const std::string& task_name() const noexcept { return task_name_; }

  // What the thread held that broke the task's rules.
  const resource_info& held() const noexcept { return held_; }

 protected:
  task_violation(std::string task_name, resource_info held)
      : task_name_(std::move(task_name)), held_(std::move(held)) {}

 private:
  std::string task_name_;
  resource_info held_;
};

// A task_scope began while its thread held a mutex.
class task_entry_violation final : public task_violation {
 public:
  task_entry_violation(std::string task_name, resource_info held, std::string left_by = {})
      : task_violation(std::move(task_name), std::move(held)), left_by_(std::move(left_by)) {}

  // The earlier task on this thread that ended still holding the mutex, or ""
  // if it wasn't left by a task.
  const std::string& left_by() const noexcept { return left_by_; }

  std::string message() const override {
    std::string m = "task boundary violation: task '" + task_name() + "' started while holding " +
                    held().describe();
    if (!left_by_.empty()) m += ", left by task '" + left_by_ + "'";
    return m;
  }

  [[noreturn]] void throw_error() const override;

 private:
  std::string left_by_;
};

// A task_scope ended while the task still held something it took.
class task_exit_violation final : public task_violation {
 public:
  task_exit_violation(std::string task_name, resource_info held)
      : task_violation(std::move(task_name), std::move(held)) {}

  std::string message() const override {
    return "task boundary violation: task '" + task_name() + "' ended still holding " +
           held().describe();
  }

  [[noreturn]] void throw_error() const override;
};

// The operations that need the thread to hold an obligation taken from the
// resource, unless the resource is obligation_check::unchecked.
enum class obligation_needed_for { notify, count_down, set_value, set_exception };

// Notifying a condition variable, counting down a latch, or setting a
// promise's value or exception without holding an obligation taken from it.
class missing_obligation_violation final : public violation {
 public:
  missing_obligation_violation(resource_info resource, obligation_needed_for needed_for)
      : resource_(std::move(resource)), needed_for_(needed_for) {}

  // The condition variable, latch or promise.
  const resource_info& resource() const noexcept { return resource_; }

  // What the thread was doing.
  obligation_needed_for needed_for() const noexcept { return needed_for_; }

  std::string message() const override {
    return "missing obligation: " + action() + " '" + resource_.name + "' (level " +
           std::to_string(resource_.level) + ") without holding its obligation";
  }

  [[noreturn]] void throw_error() const override;

 private:
  std::string action() const {
    switch (needed_for_) {
      case obligation_needed_for::notify:
        return "notifying";
      case obligation_needed_for::count_down:
        return "counting down";
      case obligation_needed_for::set_value:
        return "setting the value of";
      case obligation_needed_for::set_exception:
        return "setting an exception in";
    }
    return "signalling";
  }

  resource_info resource_;
  obligation_needed_for needed_for_;
};

// The base of every exception throw_on_violation throws. what() is the
// violation's message().
class violation_error : public std::logic_error {
 public:
  virtual const violation& info() const noexcept = 0;

 protected:
  explicit violation_error(const violation& v) : std::logic_error(v.message()) {}
};

class lock_order_error final : public violation_error {
 public:
  explicit lock_order_error(lock_order_violation info)
      : violation_error(info), info_(std::move(info)) {}

  const lock_order_violation& info() const noexcept override { return info_; }

 private:
  lock_order_violation info_;
};

// The base of task_entry_error and task_exit_error.
class task_boundary_error : public violation_error {
 public:
  const task_violation& info() const noexcept override = 0;

 protected:
  using violation_error::violation_error;
};

class task_entry_error final : public task_boundary_error {
 public:
  explicit task_entry_error(task_entry_violation info)
      : task_boundary_error(info), info_(std::move(info)) {}

  const task_entry_violation& info() const noexcept override { return info_; }

 private:
  task_entry_violation info_;
};

class task_exit_error final : public task_boundary_error {
 public:
  explicit task_exit_error(task_exit_violation info)
      : task_boundary_error(info), info_(std::move(info)) {}

  const task_exit_violation& info() const noexcept override { return info_; }

 private:
  task_exit_violation info_;
};

class missing_obligation_error final : public violation_error {
 public:
  explicit missing_obligation_error(missing_obligation_violation info)
      : violation_error(info), info_(std::move(info)) {}

  const missing_obligation_violation& info() const noexcept override { return info_; }

 private:
  missing_obligation_violation info_;
};

inline void lock_order_violation::throw_error() const { throw lock_order_error(*this); }
inline void task_entry_violation::throw_error() const { throw task_entry_error(*this); }
inline void task_exit_violation::throw_error() const { throw task_exit_error(*this); }
inline void missing_obligation_violation::throw_error() const {
  throw missing_obligation_error(*this);
}

// Violation policies. A policy is any type with a static on_violation that
// accepts each violation class: one on_violation(const violation&) is
// enough. If it returns, the blocking operation (or task) goes ahead anyway.
struct throw_on_violation {
  [[noreturn]] static void on_violation(const violation& v) { v.throw_error(); }
};

struct abort_on_violation {
  [[noreturn]] static void on_violation(const violation& v) noexcept {
    std::fprintf(stderr, "lock_align: %s\n", v.message().c_str());
    std::abort();
  }
};

struct log_on_violation {
  static void on_violation(const violation& v) noexcept {
    std::fprintf(stderr, "lock_align: %s\n", v.message().c_str());
  }
};

using default_policy = LOCK_ALIGN_DEFAULT_POLICY;

}  // namespace lock_align

// Registers this file's configuration at startup; see detail/config_check.hpp.
#ifndef LOCK_ALIGN_NO_CONFIG_CHECK
#include "lock_align/detail/config_check.hpp"

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI::detail {
namespace {  // one per file, which is the point

#if defined(__clang__) || defined(__GNUC__)
inline constexpr const char* this_file = __BASE_FILE__;
#else
inline constexpr const char* this_file = "a translation unit";
#endif

#ifdef LOCK_ALIGN_ALLOW_MIXED_DEFAULTS
inline constexpr bool compare_defaults = false;
#else
inline constexpr bool compare_defaults = true;
#endif

[[maybe_unused]] const bool configuration_registered =
    (config_detail::register_configuration(
         {LOCK_ALIGN_DETAIL_STR(LOCK_ALIGN_DETAIL_ABI), enabled, held_slots_reserved,
          config_detail::type_name<default_policy>(),
          LOCK_ALIGN_DETAIL_STR(LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK), compare_defaults, this_file}),
     true);

}  // namespace
}  // namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI::detail
#endif

# Reference

Everything is in namespace `lock_align`, through an inline namespace whose
name encodes the configuration, such as `lock_align::v1_on_s32`. Write
`lock_align::ordered_mutex` as usual, but don't name the inline namespace
or declare anything in `lock_align` yourself; see
[One configuration per program](config/README.md#one-configuration-per-program).

Each header can be included on its own. Headers under `lock_align/detail/`
are internal: the other headers include them, and nothing in them is part
of the API.

Each header has a page listing what it provides. Classes and class
templates have their own pages. Free functions, constants, type aliases and
macros are documented on their header's page.

## Mutexes and condition variables

| Header | Contents |
|---|---|
| [`<lock_align/ordered_mutex.hpp>`](ordered_mutex/README.md) | [`ordered_mutex`](ordered_mutex/ordered_mutex.md), [`is_recursive_mutex`](ordered_mutex/is_recursive_mutex.md) |
| [`<lock_align/ordered_lock.hpp>`](ordered_lock/README.md) | [`ordered_lock`](ordered_lock/README.md#ordered_lock), [`ordered_scoped_lock`](ordered_lock/ordered_scoped_lock.md) |
| [`<lock_align/ordered_condition_variable.hpp>`](ordered_condition_variable/README.md) | [`ordered_condition_variable`](ordered_condition_variable/ordered_condition_variable.md) |

## Waiting on other threads

| Header | Contents |
|---|---|
| [`<lock_align/obligation.hpp>`](obligation/README.md) | [`obligation`](obligation/obligation.md), [`obligation_check`](obligation/README.md#obligation_check), `default_obligation_check`, [`check_wait`](obligation/README.md#check_wait) |
| [`<lock_align/ordered_thread.hpp>`](ordered_thread/README.md) | [`ordered_thread`](ordered_thread/ordered_thread.md) |
| [`<lock_align/ordered_promise.hpp>`](ordered_promise/README.md) | [`ordered_promise`](ordered_promise/ordered_promise.md), [`ordered_future`](ordered_promise/ordered_future.md) |
| [`<lock_align/ordered_latch.hpp>`](ordered_latch/README.md) | [`ordered_latch`](ordered_latch/ordered_latch.md) |
| [`<lock_align/task_scope.hpp>`](task_scope/README.md) | [`task_scope`](task_scope/task_scope.md) |

## Foundations

| Header | Contents |
|---|---|
| [`<lock_align/level.hpp>`](level/README.md) | `level_t`, `level_like`, `is_valid_level`, `to_level`, [`inherit_level`](level/README.md#inherit_level) |
| [`<lock_align/violation.hpp>`](violation/README.md) | `resource_kind`, `obligation_needed_for`, [`resource_info`](violation/resource_info.md), [`violation`](violation/violation.md) and its derived classes, [`violation_error`](violation/violation_error.md) and its derived classes, [`throw_on_violation`](violation/throw_on_violation.md), [`abort_on_violation`](violation/abort_on_violation.md), [`log_on_violation`](violation/log_on_violation.md), `default_policy` |
| [`<lock_align/violation_fwd.hpp>`](violation/README.md#violation_fwdhpp) | Declarations of `violation` and its derived classes, for a [default policy](config/README.md#lock_align_default_policy) of your own |
| [`<lock_align/held.hpp>`](held/README.md) | `current_level`, `held_count` |
| [`<lock_align/config.hpp>`](config/README.md) | `LOCK_ALIGN_DISABLE`, `LOCK_ALIGN_HELD_SLOTS_RESERVED`, `LOCK_ALIGN_DEFAULT_POLICY`, `LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK`, `LOCK_ALIGN_NO_CONFIG_CHECK`, `LOCK_ALIGN_ALLOW_MIXED_DEFAULTS`, `enabled`, `held_slots_reserved` |

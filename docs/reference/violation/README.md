# `<lock_align/violation.hpp>`

What a violation reports, and the policies that decide what happens next.

## Violations

| Class | Description |
|---|---|
| [`violation`](violation.md) | The base of every violation |
| [`lock_order_violation`](lock_order_violation.md) | Blocking on a resource while holding one at or above it |
| [`task_violation`](task_violation.md) | The base of the two task violations |
| [`task_entry_violation`](task_entry_violation.md) | A task began while its thread held a mutex |
| [`task_exit_violation`](task_exit_violation.md) | A task ended still holding something it took |
| [`missing_obligation_violation`](missing_obligation_violation.md) | Notifying, counting down or setting a value without the obligation |
| [`resource_info`](resource_info.md) | One side of a violation: a resource's name, level and kind |

## Exceptions

| Class | Description |
|---|---|
| [`violation_error`](violation_error.md) | The base of every exception `throw_on_violation` throws |
| [`lock_order_error`](lock_order_error.md) | Thrown for a `lock_order_violation` |
| [`task_boundary_error`](task_boundary_error.md) | The base of the two task errors |
| [`task_entry_error`](task_entry_error.md) | Thrown for a `task_entry_violation` |
| [`task_exit_error`](task_exit_error.md) | Thrown for a `task_exit_violation` |
| [`missing_obligation_error`](missing_obligation_error.md) | Thrown for a `missing_obligation_violation` |

Each violation class has a matching error, and each error's `info()`
returns its violation:

```
violation                           violation_error  (a std::logic_error)
├── lock_order_violation            ├── lock_order_error
├── task_violation                  ├── task_boundary_error
│   ├── task_entry_violation        │   ├── task_entry_error
│   └── task_exit_violation         │   └── task_exit_error
└── missing_obligation_violation    └── missing_obligation_error
```

## Policies

| Class | Description |
|---|---|
| [`throw_on_violation`](throw_on_violation.md) | Throw the violation's error |
| [`abort_on_violation`](abort_on_violation.md) | Print and abort |
| [`log_on_violation`](log_on_violation.md) | Print and carry on |

## Types

| Name | Description |
|---|---|
| [`resource_kind`](#resource_kind) | Whether a resource is a mutex or an obligation |
| [`obligation_needed_for`](#obligation_needed_for) | The operation a `missing_obligation_violation` was for |
| [`default_policy`](#default_policy) | The policy used when none is given |

## Policy requirements

A policy is any type `P` with a static member function `on_violation` that
can be called with each kind of violation:

```cpp
P::on_violation(v);  // v is a const lock_order_violation&,
                     // const task_entry_violation&, const task_exit_violation&
                     // or const missing_obligation_violation&
```

One `on_violation(const violation&)` is enough. A policy can also overload
`on_violation` for particular kinds: the call is always made with the most
derived type, so overload resolution picks the closest match, with no
`dynamic_cast` needed.

```cpp
struct my_policy {
  static void on_violation(const lock_align::lock_order_violation& v);  // lock order
  static void on_violation(const lock_align::task_violation& v);        // either task check
  static void on_violation(const lock_align::missing_obligation_violation& v);
};
```

It's called in the thread that broke the rule:

- for a lock or wait, before the thread blocks;
- for a missing obligation, before the notify, count-down or set;
- for a [`task_scope`](../task_scope/task_scope.md), when the scope begins
  or ends.

If `on_violation` throws or doesn't return, the operation doesn't happen.
If it returns, the operation goes ahead as though the check had passed.

A policy must not lock an ordered mutex or wait on anything **lock_align**
checks: it runs in the middle of a check. A plain `std::mutex` is fine.

The policy is a template parameter of each **lock_align** class, so different
objects can use different policies. See the
[custom policy example](../../examples/custom_policy.md).

---

### resource_kind

```cpp
enum class resource_kind { mutex, obligation };
```

What kind of resource a [`resource_info`](resource_info.md) describes.

### obligation_needed_for

```cpp
enum class obligation_needed_for { notify, count_down, set_value, set_exception };
```

What a thread was doing when a
[`missing_obligation_violation`](missing_obligation_violation.md) was
reported:

| Value | Operation |
|---|---|
| `notify` | `ordered_condition_variable::notify_one` or `notify_all` |
| `count_down` | `ordered_latch::count_down`, or the counting down in `arrive_and_wait` |
| `set_value` | `ordered_promise::set_value` |
| `set_exception` | `ordered_promise::set_exception` |

### default_policy

```cpp
using default_policy = LOCK_ALIGN_DEFAULT_POLICY;
```

The policy every **lock_align** template uses when its policy parameter is left
out. `throw_on_violation` unless
[`LOCK_ALIGN_DEFAULT_POLICY`](../config/README.md#lock_align_default_policy)
is defined.

## violation_fwd.hpp

```cpp
#include "lock_align/violation_fwd.hpp"
```

Declares `violation`, `lock_order_violation`, `task_violation`,
`task_entry_violation`, `task_exit_violation` and
`missing_obligation_violation` without defining them. Include it to
declare a policy of your own before
[`LOCK_ALIGN_DEFAULT_POLICY`](../config/README.md#lock_align_default_policy)
is used; `violation.hpp` includes it too.

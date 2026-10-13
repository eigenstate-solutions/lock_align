# throw_on_violation

```cpp
struct throw_on_violation;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

A [violation policy](README.md#policy-requirements) that throws the
[`violation_error`](violation_error.md) matching the violation: a
[`lock_order_error`](lock_order_error.md), a
[`task_entry_error`](task_entry_error.md) or a
[`task_exit_error`](task_exit_error.md). The default policy unless
[`LOCK_ALIGN_DEFAULT_POLICY`](../config/README.md#lock_align_default_policy)
says otherwise.

Because it throws, a violation found where exceptions can't escape ends the
program through `std::terminate`. That includes a destructor, such as one
that joins threads, and a function marked `noexcept`.

## Member functions

### on_violation

```cpp
[[noreturn]] static void on_violation(const violation& v);
```

Calls [`v.throw_error()`](violation.md#throw_error).

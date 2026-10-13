# violation

```cpp
class violation;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

The base of every violation. It's what a
[policy](README.md#policy-requirements) taking `const violation&` receives,
and what [`violation_error::info()`](violation_error.md#info) returns.

`violation` is abstract. Each kind of violation derives from it:

- [`lock_order_violation`](lock_order_violation.md);
- [`task_entry_violation`](task_entry_violation.md) and
  [`task_exit_violation`](task_exit_violation.md), through
  [`task_violation`](task_violation.md);
- [`missing_obligation_violation`](missing_obligation_violation.md).

What a violation carries depends on its kind, so the base has only what
they all share. `lock_order_violation` and `task_violation` have `held()`;
a missing obligation has nothing held to report.

Its copy constructor and copy assignment are protected, so a violation
can't be copied through the base class and sliced. Copy the derived class
instead.

## Member functions

| Function | Description |
|---|---|
| [`message`](#message) | Describes the violation in one line |
| [`throw_error`](#throw_error) | Throws the matching error |
| (destructor) | Virtual |

---

### message

```cpp
virtual std::string message() const = 0;
```

Returns a one-line description. Each derived class gives its own; see
their pages.

### throw_error

```cpp
[[noreturn]] virtual void throw_error() const = 0;
```

Throws the [`violation_error`](violation_error.md) that matches the
violation's class, holding a copy of it. This is how
[`throw_on_violation`](throw_on_violation.md) throws the right error from
a `const violation&`. A custom policy can call it too.

## Example

```cpp
struct log_then_throw {
  static void on_violation(const lock_align::violation& v) {
    std::cerr << v.message() << '\n';
    v.throw_error();
  }
};
```

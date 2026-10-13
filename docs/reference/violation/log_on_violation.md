# log_on_violation

```cpp
struct log_on_violation;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

A [violation policy](README.md#policy-requirements) that prints the
violation and lets the operation go ahead. Useful for adding **lock_align** to
existing code: you see every violation without changing what the program
does. The program can still deadlock.

## Member functions

### on_violation

```cpp
static void on_violation(const violation& v) noexcept;
```

Writes `lock_align: ` followed by
[`v.message()`](violation.md#message) and a newline to `stderr`, then
returns.

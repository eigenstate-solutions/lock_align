# abort_on_violation

```cpp
struct abort_on_violation;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

A [violation policy](README.md#policy-requirements) that prints the
violation and aborts. Use it where an exception would be caught and
ignored, or where you want a core dump at the point of the violation.

## Member functions

### on_violation

```cpp
[[noreturn]] static void on_violation(const violation& v) noexcept;
```

Writes `lock_align: ` followed by
[`v.message()`](violation.md#message) and a newline to `stderr`, then
calls `std::abort()`.

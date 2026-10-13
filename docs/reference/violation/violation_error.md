# violation_error

```cpp
class violation_error : public std::logic_error;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

The base of every exception [`throw_on_violation`](throw_on_violation.md)
throws. Catch it to handle any violation:

- [`lock_order_error`](lock_order_error.md);
- [`task_entry_error`](task_entry_error.md) and
  [`task_exit_error`](task_exit_error.md), through
  [`task_boundary_error`](task_boundary_error.md).

Each is thrown before anything blocks, so whatever the thread was about to
lock or wait for is untouched, and everything it already held is still
held.

`what()` returns [`info().message()`](violation.md#message).

## Member functions

| Function | Description |
|---|---|
| [`info`](#info) | The violation |
| `what` | Inherited from `std::logic_error` |

---

### info

```cpp
virtual const violation& info() const noexcept = 0;
```

Returns the [`violation`](violation.md) the error was thrown for. Each
derived error overrides it to return its own violation class.

## Example

```cpp
try {
  run_job();
} catch (const lock_align::missing_obligation_error& e) {
  std::cerr << "forgot an obligation: " << e.what() << '\n';
} catch (const lock_align::violation_error& e) {
  std::cerr << e.what() << '\n';
}
```

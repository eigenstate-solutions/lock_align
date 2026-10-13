# task_boundary_error

```cpp
class task_boundary_error : public violation_error;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

The base of [`task_entry_error`](task_entry_error.md) and
[`task_exit_error`](task_exit_error.md), the exceptions
[`throw_on_violation`](throw_on_violation.md) throws for a
[`task_scope`](../task_scope/task_scope.md)'s violations. Catch it to
handle both alike.

## Member functions

| Function | Description |
|---|---|
| [`info`](#info) | The violation |
| `what` | Inherited from `std::logic_error` |

---

### info

```cpp
const task_violation& info() const noexcept override = 0;
```

Returns the [`task_violation`](task_violation.md) the error was thrown for.

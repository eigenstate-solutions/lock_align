# task_exit_error

```cpp
class task_exit_error final : public task_boundary_error;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

The exception [`throw_on_violation`](throw_on_violation.md) throws for a
[`task_exit_violation`](task_exit_violation.md). It's thrown from the
`task_scope`'s destructor, which is `noexcept(false)`; it isn't thrown while
an exception is already unwinding.

`what()` returns [`info().message()`](task_exit_violation.md#message).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs from a `task_exit_violation` |
| [`info`](#info) | The violation |
| `what` | Inherited from `std::logic_error` |

---

### (constructor)

```cpp
explicit task_exit_error(task_exit_violation info);
```

### info

```cpp
const task_exit_violation& info() const noexcept override;
```

Returns the [`task_exit_violation`](task_exit_violation.md) the
error was built from.

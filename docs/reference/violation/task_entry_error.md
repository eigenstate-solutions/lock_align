# task_entry_error

```cpp
class task_entry_error final : public task_boundary_error;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

The exception [`throw_on_violation`](throw_on_violation.md) throws for a
[`task_entry_violation`](task_entry_violation.md). It's thrown from the
`task_scope`'s constructor, so the scope never begins.

`what()` returns [`info().message()`](task_entry_violation.md#message).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs from a `task_entry_violation` |
| [`info`](#info) | The violation |
| `what` | Inherited from `std::logic_error` |

---

### (constructor)

```cpp
explicit task_entry_error(task_entry_violation info);
```

### info

```cpp
const task_entry_violation& info() const noexcept override;
```

Returns the [`task_entry_violation`](task_entry_violation.md) the
error was built from.

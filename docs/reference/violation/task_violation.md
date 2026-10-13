# task_violation

```cpp
class task_violation : public violation;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

The base of the two ways a [`task_scope`](../task_scope/task_scope.md)'s
rules can be broken:

- [`task_entry_violation`](task_entry_violation.md): the task began while
  its thread held a mutex;
- [`task_exit_violation`](task_exit_violation.md): the task ended still
  holding something it took.

The *held* resource is the one that shouldn't have been held. A policy can
overload `on_violation(const task_violation&)` to handle both alike.

## Member functions

| Function | Description |
|---|---|
| [`task_name`](#task_name) | The task's name |
| [`held`](#held) | The resource that shouldn't have been held |
| [`message`](violation.md#message), [`throw_error`](violation.md#throw_error) | Inherited from [`violation`](violation.md) |

---

### task_name

```cpp
const std::string& task_name() const noexcept;
```

The [`task_scope`](../task_scope/task_scope.md)'s name.

### held

```cpp
const resource_info& held() const noexcept;
```

The [resource](resource_info.md) that shouldn't have been held: for a
`task_entry_violation`, a mutex held when the task began; for a
`task_exit_violation`, something the task took and didn't release.

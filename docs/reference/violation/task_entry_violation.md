# task_entry_violation

```cpp
class task_entry_violation final : public task_violation;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

A [`task_scope`](../task_scope/task_scope.md) began while its thread held a
mutex. The *held* resource is the highest mutex the thread held. If an
earlier task on the thread ended still holding that mutex, `left_by()`
names it.

Thrown as a [`task_entry_error`](task_entry_error.md) by
[`throw_on_violation`](throw_on_violation.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs from the task's name, the held resource, and the task that left it |
| [`task_name`](task_violation.md#task_name) | Inherited from [`task_violation`](task_violation.md) |
| [`held`](task_violation.md#held) | Inherited from [`task_violation`](task_violation.md) |
| [`left_by`](#left_by) | The earlier task that left the mutex held |
| [`message`](#message) | Describes the violation in one line |
| [`throw_error`](#throw_error) | Throws a `task_entry_error` |

---

### (constructor)

```cpp
task_entry_violation(std::string task_name, resource_info held, std::string left_by = {});
```

### left_by

```cpp
const std::string& left_by() const noexcept;
```

The name of the earlier task on this thread that ended still holding the
*held* mutex, or `""` if no task left it: the worker took it itself, or
the thread no longer remembers. See
[`task_scope`](../task_scope/task_scope.md) for how long a thread
remembers.

### message

```cpp
std::string message() const override;
```

For example:

```
task boundary violation: task 'job' started while holding mutex 'outer' (level 2)
task boundary violation: task 'job' started while holding mutex 'outer' (level 2), left by task 'load'
```

### throw_error

```cpp
[[noreturn]] void throw_error() const override;
```

Throws [`task_entry_error(*this)`](task_entry_error.md).

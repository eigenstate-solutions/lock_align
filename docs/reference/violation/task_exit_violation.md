# task_exit_violation

```cpp
class task_exit_violation final : public task_violation;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

A [`task_scope`](../task_scope/task_scope.md) ended, not by an exception,
while the task still held something it took. The *held* resource is the
newest entry the task added.

Thrown as a [`task_exit_error`](task_exit_error.md) by
[`throw_on_violation`](throw_on_violation.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs from the task's name and the held resource |
| [`task_name`](task_violation.md#task_name) | Inherited from [`task_violation`](task_violation.md) |
| [`held`](task_violation.md#held) | Inherited from [`task_violation`](task_violation.md) |
| [`message`](#message) | Describes the violation in one line |
| [`throw_error`](#throw_error) | Throws a `task_exit_error` |

---

### (constructor)

```cpp
task_exit_violation(std::string task_name, resource_info held);
```

### message

```cpp
std::string message() const override;
```

For example:

```
task boundary violation: task 'job' ended still holding mutex 'leaked' (level 1)
```

### throw_error

```cpp
[[noreturn]] void throw_error() const override;
```

Throws [`task_exit_error(*this)`](task_exit_error.md).

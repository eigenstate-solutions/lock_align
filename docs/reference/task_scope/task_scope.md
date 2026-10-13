# task_scope

```cpp
template <class Policy = default_policy>
class task_scope;
```

**Header:** [`<lock_align/task_scope.hpp>`](README.md)

Marks one task run by a thread pool or executor. Construct it in the worker
just before running the task, and let it go out of scope just after.

- **At entry,** the worker must hold no tracked mutex. If it holds one,
  `Policy` is given a
  [`task_entry_violation`](../violation/task_entry_violation.md).
- **At exit,** the task must have released everything it took. If it still
  holds a mutex or obligation it didn't hold at entry, `Policy` is given a
  [`task_exit_violation`](../violation/task_exit_violation.md).

Obligations the worker already owes at entry are allowed. A worker built on
[`ordered_thread`](../ordered_thread/ordered_thread.md) owes one for
as long as it runs. The task really does run under it.

Nothing is hidden. Whatever the thread holds, including those obligations,
the checks inside the task see. So a task can't take a lock that whoever
joins the worker might be holding.

If an exception is unwinding through the scope, the exit check runs only
if `Policy::on_violation` is `noexcept` for a `task_exit_violation`, as it
is for [`log_on_violation`](../violation/log_on_violation.md) and
[`abort_on_violation`](../violation/abort_on_violation.md). A throw from a
destructor during unwinding would end the program, so for a policy that
may throw, such as the default, the check is skipped.

Entries a task leaves behind stay recorded, so later checks still see them.
The thread also remembers the last task that ended still holding a mutex,
whether that was reported or not. When a later task's entry check reports
that mutex, the [`task_entry_violation`](../violation/task_entry_violation.md)
names the task that left it, through
[`left_by()`](../violation/task_entry_violation.md#left_by). The thread
forgets as soon as the mutex is released, so a mutex locked later, even
the same one or another at the same address, is never blamed on the
earlier task.

The exit check compares what's held with a snapshot taken at entry, not
just how many entries there are. So a task that releases something it held
at entry and takes something else is still reported. Releasing something
held at entry and taking the same thing back isn't a leak. The snapshot
keeps a few entries inside the `task_scope`, and allocates only when the
worker holds more than that at entry.

## Template parameters

| Parameter | Description |
|---|---|
| `Policy` | What to do on a violation; see [policy requirements](../violation/README.md#policy-requirements) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Begins the task and runs the entry check |
| [(destructor)](#destructor) | Ends the task and runs the exit check |
| [`name`](#name) | The task's name |

Not copyable or movable.

---

### (constructor)

```cpp
explicit task_scope(std::string_view name = {},
                    std::source_location where = std::source_location::current());
```

Runs the entry check. `name` appears in reports; if it's empty, the name is
`"file:line"` taken from `where`.

**Exceptions:** whatever `Policy::on_violation` throws; `std::bad_alloc`
if the thread holds more than four entries and the snapshot can't be
allocated.

### (destructor)

```cpp
~task_scope() noexcept(false);
```

Runs the exit check. While an exception is unwinding through the scope, it
runs only if `Policy::on_violation` is `noexcept`; a `std::bad_alloc`
while building that report is ignored. If the task still holds a mutex it
took, remembers the task for later entry checks.

**Exceptions:** whatever `Policy::on_violation` throws, except while
unwinding. The destructor is `noexcept(false)` so that the default policy
can report a leak by throwing.

### name

```cpp
std::string name() const;
```

The task's name. `""` when
[`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is defined.

Returned by value. A default name is stored as the source location and
built into `"file:line"` on each call, so that creating the object doesn't
allocate.

## Example

```cpp
void worker_loop() {
  for (;;) {
    job next = take_next_job();
    if (!next) return;
    try {
      lock_align::task_scope task(next.name);
      next.run();
    } catch (const lock_align::violation_error& e) {  // the task's locks, or the scope
      report(next.name, e.what());
    }
  }
}
```

## See also

- [Thread pool example](../../examples/thread_pool.md)

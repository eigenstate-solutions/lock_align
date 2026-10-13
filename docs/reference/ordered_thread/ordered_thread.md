# ordered_thread

```cpp
template <class Policy = default_policy>
class ordered_thread;
```

**Header:** [`<lock_align/ordered_thread.hpp>`](README.md)

A `std::thread` with a level. The thread's body runs while holding an
[`obligation`](../obligation/obligation.md) at that level. `join()` is
checked as a wait on that obligation:

- Inside the thread, it may only block on mutexes above its level.
- A thread joining it may hold mutexes up to and including its level,
  never above.

Together these mean a thread can never be joined by someone holding a lock
it needs.

As with `std::thread`, destroying or assigning over a joinable
`ordered_thread` calls `std::terminate`.

## Template parameters

| Parameter | Description |
|---|---|
| `Policy` | What to do when `join()` is a violation; see [policy requirements](../violation/README.md#policy-requirements). Locks taken inside the thread use each mutex's own policy |

A declaration with arguments deduces the default policy, as in
`ordered_thread t(1, work);`. Where the type is named on its own, such as
a parameter or a container's element type, write `ordered_thread<>`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Starts a thread at a level, with an optional name |
| [`join`](#join) | Checks, then waits for the thread to finish |
| [`detach`](#detach) | Detaches the thread; not checked |
| [`joinable`](#joinable-get_id) | Whether the thread can be joined |
| [`get_id`](#joinable-get_id) | The thread's id |
| [`level`](#level-name) | The thread's level |
| [`name`](#level-name) | The thread's name |

Movable, not copyable.

---

### (constructor)

```cpp
ordered_thread() noexcept;

template <class F, class... Args>
explicit ordered_thread(L level, F&& f, Args&&... args);

template <class F, class... Args>
explicit ordered_thread(L level, std::string_view name, F&& f, Args&&... args);

ordered_thread(ordered_thread&& other) noexcept;
```

1. Default: no thread, level 0, named `"(unnamed)"`.
2. Starts a thread that holds an obligation at `level` while it runs
   `std::invoke(f, args...)`. `f` and `args` are moved or copied into the
   new thread, as `std::thread` does. The thread is named `"file:line"`
   after the call.
3. The same, named `name`, or `"file:line"` if `name` is empty.
4. Move: takes over `other`'s thread, level and name.

In 2 and 3, `level` is any integer or enum value (see
[`level_like`](../level/README.md#level_like)), or
[`inherit_level`](../level/README.md#inherit_level) to use the lowest level
the creating thread may join at while holding what it holds now. The
parameter's real type is an internal one that a level converts to; the
conversion records where the call was made, which is how an unnamed thread
gets its name.

**Exceptions:** in 2 and 3, [`std::out_of_range`](../level/README.md#to_level) if `level` is negative or
too large for [`level_t`](../level/README.md#level_t), before any thread is started; then
whatever `std::thread`'s constructor throws.

Both take part in overload resolution only when `f` can be called with
`args` after they are copied into the thread. So in
`ordered_thread(3, "indexer", work)`, `"indexer"` is always the name.

```cpp
ordered_thread a(3, work);                         // level 3, named "file.cpp:12"
ordered_thread b(3, "indexer", work, 42);          // level 3, named "indexer", calls work(42)
ordered_thread c(inherit_level, "helper", work);   // the creator's level
```

### join

```cpp
void join();
```

Checks that the calling thread may wait on an obligation at this thread's
level, then joins.

**Exceptions:** whatever `Policy::on_violation` throws, before joining; the
thread is then still joinable. Then whatever `std::thread::join` throws.

### detach

```cpp
void detach();
```

Detaches the thread. Not checked: nothing waits.

The thread still holds its obligation until its body returns, so it can
still only block on mutexes above its level, although nobody will ever
join it. For a thread you mean to detach, a plain `std::thread` is
usually better: the locks it takes are still checked, and it owes no
obligation. If it must be an `ordered_thread`, give it level 0. It may
then lock any ordered mutex, though it still can't wait on another
level-0 obligation.

### joinable, get_id

```cpp
bool joinable() const noexcept;
std::thread::id get_id() const noexcept;
```

The same as on `std::thread`.

### level, name

```cpp
level_t level() const noexcept;
std::string name() const;
```

The thread's level and name. The name is `""` when
[`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is defined.

Returned by value. A default name is stored as the source location and
built into `"file:line"` on each call, so that creating the object doesn't
allocate.

## Example

```cpp
#include <mutex>

#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_thread.hpp"

int main() {
  lock_align::ordered_mutex<std::mutex, 2> config{"config"};
  lock_align::ordered_mutex<std::mutex, 3> stats{"stats"};

  std::lock_guard hold(config);  // level 2
  lock_align::ordered_thread worker(2, "worker", [&] {
    std::lock_guard s(stats);  // fine: above the thread's level
    // Locking config here would be refused: the main thread holds it and
    // is about to wait for this thread.
  });
  worker.join();  // fine: holding level 2, joining a level-2 thread
}
```

## See also

- [`inherit_level`](../level/README.md#inherit_level)
- [Thread pool example](../../examples/thread_pool.md)

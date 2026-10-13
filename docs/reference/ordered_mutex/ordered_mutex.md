# ordered_mutex

```cpp
template <class Mutex, auto Level, class Policy = default_policy>
  requires level_like<decltype(Level)>
class ordered_mutex;
```

**Header:** [`<lock_align/ordered_mutex.hpp>`](README.md)

Wraps a mutex of type `Mutex` and gives it a level. Before each blocking
lock, it checks that everything the calling thread holds is at a lower
level; if not, `Policy` is told before the thread blocks. See
[How **lock_align** works](../../concepts.md) for the rule.

It has the same members as the mutex it wraps, as far as that mutex
provides them. So it meets the *Lockable* requirements, and
*TimedLockable* and *SharedLockable* when `Mutex` does. It works with
`std::lock_guard`, `std::unique_lock`, `std::shared_lock`,
`std::scoped_lock` and `std::lock`.

Which operations are checked:

| Operation | Checked | Recorded as held |
|---|---|---|
| `lock`, `lock_shared` | yes | yes |
| `try_lock`, `try_lock_shared` | no | yes, if it succeeds |
| `try_lock_for`, `try_lock_until` and the shared forms | no | yes, if it succeeds |
| `unlock`, `unlock_shared` | no | removed |

Operations that can't wait forever aren't checked. A mutex they acquire is
still recorded, so later blocking locks are checked against it.

A mutex at level 0 is never checked or recorded.

Two mutexes at the same level can't be locked one after the other. Lock
them together with
[`ordered_scoped_lock`](../ordered_lock/ordered_scoped_lock.md) or
[`ordered_lock`](../ordered_lock/README.md#ordered_lock). `std::scoped_lock`
and `std::lock` also work, but which lock they check depends on argument
order and timing; see [`<lock_align/ordered_lock.hpp>`](../ordered_lock/README.md).

With [`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable)
defined, an `ordered_mutex` is the same size as a `Mutex` and does nothing
but forward to it.

## Template parameters

| Parameter | Description |
|---|---|
| `Mutex` | The mutex to wrap: `std::mutex`, `std::shared_mutex`, `std::recursive_mutex`, `std::timed_mutex`, or any type with the same interface |
| `Level` | The level, as a [`level_like`](../level/README.md#level_like) value from 0 to 4294967295; anything else doesn't compile. 0 means untracked |
| `Policy` | What to do on a violation; see [policy requirements](../violation/README.md#policy-requirements) |

## Member types

| Type | Definition |
|---|---|
| `mutex_type` | `Mutex` |
| `policy_type` | `Policy` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs the mutex, with an optional name |
| [`name`](#name) | The name shown in reports |
| [`level`](#level) | The mutex's level |
| [`lock`](#lock) | Checks, then locks |
| [`try_lock`](#try_lock) | Tries to lock without blocking |
| [`try_lock_for`, `try_lock_until`](#try_lock_for-try_lock_until) | Tries to lock, with a timeout |
| [`unlock`](#unlock) | Unlocks |
| [`lock_shared`](#lock_shared) | Checks, then locks for shared ownership |
| [`try_lock_shared`, `try_lock_shared_for`, `try_lock_shared_until`](#try_lock_shared-try_lock_shared_for-try_lock_shared_until) | Tries to lock for shared ownership |
| [`unlock_shared`](#unlock_shared) | Unlocks shared ownership |

Not copyable or movable.

---

### (constructor)

```cpp
explicit ordered_mutex(std::string_view name = {},
                       std::source_location where = std::source_location::current());
```

Constructs the wrapped mutex. `name` appears in violation reports. If it's
empty, the name is `"file:line"` taken from `where`. Leave `where`
defaulted; it records where the mutex is declared.

### name

```cpp
std::string name() const;
```

The mutex's name. `""` when
[`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is defined.

Returned by value. A default name is stored as the source location and
built into `"file:line"` on each call, so that creating the object doesn't
allocate.

### level

```cpp
static constexpr level_t level() noexcept;
```

`Level` as a `level_t`. Being `static constexpr`, it can be called on an
object, `m.level()`, or on the type in a constant expression,
`decltype(m)::level()`.

### lock

```cpp
void lock();
```

Checks that every mutex and obligation the calling thread holds ranks below
this mutex, then locks it and records it as held.

If the check fails, `Policy::on_violation` is called before locking. With
the default policy, that throws [`lock_order_error`](../violation/lock_order_error.md)
and the mutex isn't locked.

If `Mutex` is recursive (see [`is_recursive_mutex`](is_recursive_mutex.md))
and the calling thread already holds it, the check is skipped. Relocking
your own recursive mutex can't deadlock.

**Exceptions:** whatever `Policy::on_violation` throws;
`std::bad_alloc` if the thread's record must
[grow](../config/README.md#lock_align_held_slots_reserved) and can't;
whatever `Mutex::lock` throws. In each case the mutex isn't locked.

### try_lock

```cpp
bool try_lock();
```

Tries to lock without blocking. Not checked. If it succeeds, the mutex is
recorded as held.

**Return value:** `true` if the mutex was locked.

**Exceptions:** `std::bad_alloc` if the thread's record must grow and
can't, before trying.

### try_lock_for, try_lock_until

```cpp
template <class Rep, class Period>
bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout);

template <class Clock, class Duration>
bool try_lock_until(const std::chrono::time_point<Clock, Duration>& deadline);
```

Available when `Mutex` has them. Like [`try_lock`](#try_lock), but wait up to
the timeout. Not checked, because they can't wait forever.

### unlock

```cpp
void unlock();
```

Removes the mutex from the calling thread's record, then unlocks it. Must
be called on the thread that locked it.

### lock_shared

```cpp
void lock_shared();
```

Available when `Mutex` has it. Checked and recorded exactly like
[`lock`](#lock). Shared ownership can deadlock the same way exclusive
ownership can, so the same rule applies.

### try_lock_shared, try_lock_shared_for, try_lock_shared_until

```cpp
bool try_lock_shared();

template <class Rep, class Period>
bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& timeout);

template <class Clock, class Duration>
bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration>& deadline);
```

Available when `Mutex` has them. Not checked; recorded as held if they
succeed.

### unlock_shared

```cpp
void unlock_shared();
```

Available when `Mutex` has it. Removes the entry, then unlocks.

## Example

```cpp
#include <cstdio>
#include <mutex>

#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"

enum class level { accounts = 1, audit_log = 2 };

lock_align::ordered_mutex<std::mutex, level::accounts> accounts{"accounts"};
lock_align::ordered_mutex<std::mutex, level::audit_log> audit_log{"audit log"};

int main() {
  {
    std::lock_guard a(accounts);
    std::lock_guard l(audit_log);  // fine: level 1, then level 2
  }
  try {
    std::lock_guard l(audit_log);
    std::lock_guard a(accounts);  // level 2, then level 1
  } catch (const lock_align::lock_order_error& e) {
    std::puts(e.what());
  }
}
```

Output:

```
lock order violation: blocking on mutex 'accounts' (level 1) while holding mutex 'audit log' (level 2)
```

## See also

- [`ordered_condition_variable`](../ordered_condition_variable/ordered_condition_variable.md)
- [`is_recursive_mutex`](is_recursive_mutex.md)
- [Layered locks example](../../examples/layered_locks.md)

# ordered_condition_variable

```cpp
template <auto Level, class Policy = default_policy,
          obligation_check Check = default_obligation_check>
  requires level_like<decltype(Level)>
class ordered_condition_variable;
```

**Header:** [`<lock_align/ordered_condition_variable.hpp>`](README.md)

A condition variable with a level of its own, for use with
[`ordered_mutex`](../ordered_mutex/ordered_mutex.md). It wraps
`std::condition_variable_any`.

Waiting on a condition variable is waiting for another thread to notify it.
So it's checked the way **lock_align** checks any wait on another thread (see
[How **lock_align** works](../../concepts.md#waiting-on-other-threads)):

- A thread that will notify holds [`take_obligation()`](#take_obligation)
  for as long as someone may be waiting for it. While it does, it may only
  block on mutexes above the condition variable's level.
- A thread waiting may hold mutexes up to and including that level, apart
  from the mutex the wait releases, never above.

So a waiter can't hold something a notifier needs before it can notify.
The waiter's side is checked automatically by `wait`. The notifier's side
is checked while the notifier holds the obligation, and by default
`notify_one` and `notify_all` check that it does; see
[Notifiers](#notifiers).

A wait also relocks its mutex at the end. That relock happens inside the
standard library, where an exception would end the program, so it's
checked before the wait starts, through the mutex's own policy. With a
policy that returns, the relock is reported again each time it happens.

**The level must be below the mutex's level.** A notifier holding the
obligation still has to lock the mutex to change the condition safely, and
the obligation only allows locking above its level. This is checked at
compile time, in each `wait`, `wait_for` and `wait_until`, unless the mutex
is at level 0 (untracked).

## Notifiers

The level is a promise each side makes about the other. For an
`ordered_condition_variable<5>` used with a mutex at level 6:

| | May hold while waiting or owing | May block on |
|---|---|---|
| **Waiter** | the mutex at 6, released for the wait; other mutexes at 5 or below; obligations below 5 | the notification, checked as level 5 |
| **Notifier holding the obligation** | anything, including the mutex at 6 | mutexes above 5, such as the one at 6 to set the flag; obligations above 5 |

The obligation is how the notifier's half gets checked. Without it, this
deadlock is found only when the timing goes wrong; with it, the notifier's
lock is reported every time:

```cpp
ordered_mutex<std::mutex, 1> config{"config"};
ordered_mutex<std::mutex, 2> m{"queue"};
ordered_condition_variable<1> cv{"ready"};

// waiter                              // notifier
std::lock_guard c(config);             obligation duty = cv.take_obligation();
std::unique_lock lk(m);                std::lock_guard c(config);  // reported
cv.wait(lk, [&] { return ready; });    // ... set ready under m, notify
```

The waiter is allowed to hold `config` at level 1 while it waits. If the
notifier blocks on `config` before notifying, each waits for the other
forever.

For the check to mean anything:

- **Take the obligation before anyone could start waiting for you,** and
  keep it until you've notified. Usually that's the notifying thread's
  whole run, or the whole of the work that ends in the notification.
- **Taking it while already holding locks is fine,** even locks above its
  level, such as the condition variable's own mutex. An obligation
  doesn't block, so it limits only what you block on afterwards.
- **Forgetting it is reported,** at `notify_one` or `notify_all`, as a
  [`missing_obligation_violation`](../violation/missing_obligation_violation.md).
  Only an obligation taken from this condition variable counts. With
  `obligation_check::unchecked`, forgetting it is silent, and a notifier
  that never takes it is simply not checked.
- **Holding it at `notify` isn't enough on its own.** The check can't tell
  when you took it. Taking it just before notifying satisfies the check and
  checks nothing.
- **Notifying from a destructor needs it too.** Destructors are `noexcept`,
  so with the throwing policy a missing obligation there calls
  `std::terminate`. Take the obligation in the destructor, before the work
  that leads to the notify, and release it before anything that waits at
  or below its level, such as joining threads.
- **Don't wait on the same condition variable while holding its
  obligation.** That would be waiting for yourself, and is reported.

The level is also why the condition variable must be below its mutex: a
notifier holding the obligation has to be able to lock the mutex to change
the condition.

## Template parameters

| Parameter | Description |
|---|---|
| `Level` | The level, as a [`level_like`](../level/README.md#level_like) value from 0 to 4294967295; anything else doesn't compile. Must be below the level of every mutex it's used with |
| `Policy` | What to do when a wait for a notification, or a missing obligation, is a violation; see [policy requirements](../violation/README.md#policy-requirements). The relock uses the mutex's policy |
| `Check` | Whether notifying needs the condition variable's obligation; see [`obligation_check`](../obligation/README.md#obligation_check) |

## Member types

| Type | Definition |
|---|---|
| `policy_type` | `Policy` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs the condition variable, with an optional name |
| [`name`](#name) | The name shown in reports |
| [`level`](#level) | The condition variable's level |
| [`take_obligation`](#take_obligation) | Returns an obligation for a notifying thread to hold |
| [`notify_one`](#notify_one-notify_all) | Checks for the obligation, then wakes one waiting thread |
| [`notify_all`](#notify_one-notify_all) | Checks for the obligation, then wakes every waiting thread |
| [`wait`](#wait) | Checks the wait and the relock, then waits |
| [`wait_for`](#wait_for-wait_until) | Checks the relock, then waits with a timeout |
| [`wait_until`](#wait_for-wait_until) | Checks the relock, then waits until a deadline |

Not copyable or movable.

---

### (constructor)

```cpp
explicit ordered_condition_variable(std::string_view name = {},
                                    std::source_location where = std::source_location::current());
```

`name` appears in violation reports. If it's empty, the name is
`"file:line"` taken from `where`, the path as the compiler was given it.
Leave `where` defaulted; it records where the condition variable is
declared.

### name

```cpp
std::string name() const;
```

The condition variable's name. `""` when
[`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is defined.

Returned by value. A default name is stored as the source location and
built into `"file:line"` on each call, so that creating the object doesn't
allocate.

### level

```cpp
static constexpr level_t level() noexcept;
```

`Level` as a `level_t`. Being `static constexpr`, it can be called on an
object, `cv.level()`, or on the type in a constant expression,
`decltype(cv)::level()`.

### take_obligation

```cpp
[[nodiscard]] obligation take_obligation() const;
```

Returns an [`obligation`](../obligation/obligation.md) at the condition
variable's level and with its name. A thread that will notify holds it for
as long as someone may be waiting for that notification. It can still lock
the condition variable's mutex, which is above the obligation's level.

A thread holding the obligation can't wait on the same condition variable:
that would be waiting on itself.

The obligation is recorded as taken from this condition variable, which is
what [`notify_one` and `notify_all`](#notify_one-notify_all) check for.

See [Notifiers](#notifiers) for when to take it and what it catches.

Destroy the obligation before the condition variable. One that outlives it may count
for a new condition variable created at the same address; see
[An obligation must not outlive its source](../obligation/obligation.md#an-obligation-must-not-outlive-its-source).

### notify_one, notify_all

```cpp
void notify_one() noexcept(see below);
void notify_all() noexcept(see below);
```

With `Check` `required`, checks that the calling thread holds an obligation
taken from this condition variable, reporting a
[`missing_obligation_violation`](../violation/missing_obligation_violation.md)
if not. Then the same as on `std::condition_variable_any`. They never
block.

`noexcept` when nothing is checked: with `obligation_check::unchecked`, or
when [`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is
defined.

**Exceptions:** whatever `Policy::on_violation` throws, before notifying.

### wait

```cpp
template <class Lock, class... Args>  // Lock: see below
void wait(Lock& lock, Args&&... args);
```

Before waiting, checks:

1. that the calling thread may wait on an obligation at this condition
   variable's level, ignoring `*lock.mutex()`, which the wait releases;
2. that it may relock `*lock.mutex()`, ignoring that mutex itself.

Then forwards to `std::condition_variable_any::wait`. Every overload works,
including the ones that take a predicate or a `std::stop_token`.

`Lock` must have a `mutex()` member that returns a pointer to an
`ordered_mutex`, such as `std::unique_lock<ordered_mutex<...>>` or
`std::shared_lock<ordered_mutex<...>>`. Any other lock type doesn't
compile: the waits are constrained on it.

**Exceptions:** whatever `Policy::on_violation` throws for the first check,
or the mutex's policy for the second, before waiting, with the lock still
held. Then whatever `std::condition_variable_any::wait` throws.

### wait_for, wait_until

```cpp
template <class Lock, class... Args>
decltype(auto) wait_for(Lock& lock, Args&&... args);

template <class Lock, class... Args>
decltype(auto) wait_until(Lock& lock, Args&&... args);
```

`Lock` has the same requirement as for [`wait`](#wait).

Check only the relock, then forward to `std::condition_variable_any`. A
timed wait can't wait forever for a notification, so that part isn't
checked; the relock at the end still has no timeout, so it is.

## Example

```cpp
lock_align::ordered_mutex<std::mutex, 2> m{"queue"};
lock_align::ordered_condition_variable<1> cv{"queue ready"};
bool ready = false;

std::thread producer([&] {
  lock_align::obligation duty = cv.take_obligation();
  std::lock_guard g(m);  // fine: level 2 is above the obligation's level 1
  ready = true;
  cv.notify_one();
});

std::unique_lock lk(m);
cv.wait(lk, [&] { return ready; });  // may hold locks up to level 1, besides m
producer.join();
```

## See also

- [`take_obligation` on promises](../ordered_promise/ordered_promise.md#take_obligation)
- [Thread pool example](../../examples/thread_pool.md)

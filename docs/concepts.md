# How **lock_align** works

## Levels and the ordering rule

A deadlock among mutexes needs a cycle: thread A holds X and waits for Y
while thread B holds Y and waits for X. If every thread locks mutexes in
one agreed order, that cycle can't form.

**lock_align** makes the order explicit. Every
[`ordered_mutex`](reference/ordered_mutex/ordered_mutex.md) has a level, a
number fixed when you write the type:

```cpp
ordered_mutex<std::mutex, 1> accounts;
ordered_mutex<std::mutex, 2> audit_log;
```

The rule is:

> A thread may block on a mutex only if every mutex it holds has a strictly
> lower level.

So locks are taken in rising order of level. Locking `accounts` and then
`audit_log` is fine; the other way round is a violation. Two mutexes at the
same level can't be locked one after the other. They can be locked
together with
[`ordered_scoped_lock`](reference/ordered_lock/ordered_scoped_lock.md) or
[`ordered_lock`](reference/ordered_lock/README.md#ordered_lock), which lock
a set in rising order of level, and avoid deadlock among mutexes at the
same level as `std::lock` does.

Every check runs against what the current thread holds, before it blocks.
You don't need two threads to race in the bad order: the first time any
thread takes the bad path, it's reported.

Levels can be any integer or enum type. An enum keeps the order in one
place:

```cpp
enum class level { config = 1, accounts = 2, audit_log = 3 };
ordered_mutex<std::mutex, level::accounts> accounts;
```

## Waiting on other threads

Mutexes aren't the only way to deadlock. A thread that holds a lock and
waits for another thread to finish is stuck if that thread needs the lock.

**lock_align** treats such a wait as one more kind of lock. Whoever will do the
work holds an [`obligation`](reference/obligation/obligation.md) at some
level N for as long as someone may be waiting for it. Waiting for the work
is then a blocking acquire at level N:

- The thread that owes the work may only block on mutexes **above** N.
- A thread waiting for the work may hold mutexes **up to and including** N,
  never above.

Between them, those two rules keep a wait from closing a cycle. Mutexes and
obligations share one order; an obligation at level N sits just above
mutexes at level N.

The library's wrappers create and check obligations for you:

| Wrapper | Who holds the obligation | Which waits are checked |
|---|---|---|
| [`ordered_thread`](reference/ordered_thread/ordered_thread.md) | the thread's body, automatically | `join()` |
| [`ordered_promise`](reference/ordered_promise/ordered_promise.md) | the producer, from `take_obligation()` | `get()`, `wait()` on the future |
| [`ordered_latch`](reference/ordered_latch/ordered_latch.md) | each counting thread, from `take_obligation()` | `wait()`, `arrive_and_wait()` |
| [`ordered_condition_variable`](reference/ordered_condition_variable/ordered_condition_variable.md) | each notifying thread, from `take_obligation()` | `wait()`, apart from the mutex it releases |

By default, the promise, latch and condition variable also check the other
side: `set_value`, `count_down` and `notify_one` report a thread that
doesn't hold an obligation taken from that object. Without that check,
forgetting the obligation would be silent. It can be turned off per object,
or for the whole program; see
[`obligation_check`](reference/obligation/README.md#obligation_check).

For a wait the library doesn't wrap, construct an `obligation` in the
thread that does the work, and call
[`check_wait`](reference/obligation/README.md#check_wait) before blocking
on it.

An obligation at level 0 is still meaningful. Its owner may lock any
ordered mutex, and anyone waiting for it must hold none.

## What happens on a violation

A violation is handed to a policy. The default,
[`throw_on_violation`](reference/violation/throw_on_violation.md), throws
a [`violation_error`](reference/violation/violation_error.md) before the
thread blocks, so nothing is locked or waited on:
[`lock_order_error`](reference/violation/lock_order_error.md) for a lock or
wait out of order, or a
[`task_boundary_error`](reference/violation/task_boundary_error.md) for a
broken [task boundary](reference/task_scope/task_scope.md), or a
[`missing_obligation_error`](reference/violation/missing_obligation_error.md)
for signalling without an obligation. The library also provides
[`abort_on_violation`](reference/violation/abort_on_violation.md) and
[`log_on_violation`](reference/violation/log_on_violation.md), and you can
write your own. A policy that returns lets the operation go ahead.

Choose a policy per object with a template parameter, or for the whole
program with
[`LOCK_ALIGN_DEFAULT_POLICY`](reference/config/README.md).

## What is and isn't checked

Checked, before blocking:

- `lock()` and `lock_shared()` on an ordered mutex
- `wait()` on an
  [`ordered_condition_variable`](reference/ordered_condition_variable/ordered_condition_variable.md),
  as a wait for a notification, and the relock of its mutex at the end of
  every wait, timed or not
- `join()` on an ordered thread
- `get()` and `wait()` on an ordered future
- `wait()` and `arrive_and_wait()` on an ordered latch
- `check_wait()`

Checked for an obligation taken from the object, unless it's
`obligation_check::unchecked`:

- `notify_one()` and `notify_all()` on a condition variable
- `count_down()` and `arrive_and_wait()` on a latch
- `set_value()` and `set_exception()` on a promise

Not checked, because they can't wait forever:

- `try_lock()` and the timed locks (`try_lock_for`, `try_lock_until`, and
  their shared forms). A mutex taken this way is still recorded, so later
  blocking locks are checked against it.
- timed waits on futures (`wait_for`, `wait_until`), and the wait for a
  notification in a condition variable's timed waits
- `detach()` on a thread. A detached `ordered_thread` still holds its
  obligation until its body returns, so its locks are still limited to
  levels above its own, although nobody can join it. Start a thread you
  mean to detach as a plain `std::thread`, whose locks are still checked,
  or at level 0.

Not tracked at all:

- mutexes at level 0. Use level 0 for a mutex you don't want checked;
  mixing it with ordered mutexes can still deadlock.
- plain `std::mutex`, `std::thread` and the rest. **lock_align** only knows
  about its own types.

Each thread keeps its own record of what it holds, so a mutex must be
unlocked on the thread that locked it (which the standard requires
anyway). There's no limit on how many entries a thread can hold. The
first [`LOCK_ALIGN_HELD_SLOTS_RESERVED`](reference/config/README.md#lock_align_held_slots_reserved)
are kept without allocating; beyond that the record moves to the heap.

## Thread pools

A pool worker runs many unrelated tasks. Wrap each one in a
[`task_scope`](reference/task_scope/task_scope.md). It reports a worker
that starts a task while holding a mutex, and a task that finishes without
releasing something it took. Nothing is hidden from the checks inside the
task. A worker built on `ordered_thread` holds its own obligation, and the
task runs under it, so a task can't take a lock that whoever joins the pool
may be holding. The [thread pool example](examples/thread_pool.md) shows
this.

## Turning it off

Define `LOCK_ALIGN_DISABLE` before including any **lock_align** header, and use
the same setting in every file of the program. Every check then compiles
away and no names are built. An `ordered_mutex` is the same size as the
mutex it wraps; the other wrappers keep at most their level.

## Choosing levels

- Number layers of your program from the outside in. Whatever is locked
  first, such as a registry or a top-level container, gets the lowest
  level; whatever is locked last, such as logging, gets the highest.
- Leave gaps (10, 20, 30) so new layers can go in between.
- Give each mutex a name. The default name is the file and line where it
  was declared, which is usually enough, but a name like
  `"account 1234"` makes reports easier to read.

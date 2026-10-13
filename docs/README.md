# **lock_align**

**lock_align** finds deadlocks before they happen. You give each mutex a
level, and every blocking lock is checked against the locks the thread
already holds. Code that locks in the wrong order is reported the first
time it runs, on one thread, with no unlucky timing needed.

Waiting on other threads is checked the same way. Joining a thread, and
waiting on a future, a latch or a condition variable, are all treated like
locking at a level. So a thread that waits for work while holding a lock
that work needs is reported too.

```cpp
#include <mutex>
#include "lock_align/ordered_mutex.hpp"

lock_align::ordered_mutex<std::mutex, 1> accounts{"accounts"};
lock_align::ordered_mutex<std::mutex, 2> audit_log{"audit log"};

void transfer() {
  std::lock_guard a(accounts);   // level 1
  std::lock_guard l(audit_log);  // level 2: fine, levels go up
}

void report() {
  std::lock_guard l(audit_log);  // level 2
  std::lock_guard a(accounts);   // level 1: throws lock_order_error
}
```

The library is header-only and needs C++20. Each header stands on its own,
so include only the ones you use. Define `LOCK_ALIGN_DISABLE` to compile every
check away; a mutex is then the same size as what it wraps.

## How it works

[How **lock_align** works](concepts.md) explains levels, the ordering rule, how
waits between threads fit into it, and what is and isn't checked. Read it
before choosing your levels.

## Headers

| Header | Provides |
|---|---|
| [`ordered_mutex.hpp`](reference/ordered_mutex/README.md) | `ordered_mutex`, a wrapper that checks every blocking lock |
| [`ordered_lock.hpp`](reference/ordered_lock/README.md) | `ordered_lock` and `ordered_scoped_lock`, which lock several ordered mutexes in level order |
| [`ordered_condition_variable.hpp`](reference/ordered_condition_variable/README.md) | A condition variable whose waits are checked like waiting on another thread |
| [`ordered_thread.hpp`](reference/ordered_thread/README.md) | A thread whose `join()` is checked |
| [`ordered_promise.hpp`](reference/ordered_promise/README.md) | A promise and future whose waits are checked |
| [`ordered_latch.hpp`](reference/ordered_latch/README.md) | A latch whose waits are checked |
| [`obligation.hpp`](reference/obligation/README.md) | Obligations, for waits on work other threads owe; `check_wait` |
| [`task_scope.hpp`](reference/task_scope/README.md) | Task boundaries for thread pools |
| [`violation_fwd.hpp`](reference/violation/README.md#violation_fwdhpp) | Declarations of the violation classes, for a default policy of your own |
| [`violation.hpp`](reference/violation/README.md) | What a violation reports, and the policies that handle it |
| [`level.hpp`](reference/level/README.md) | Level types, `is_valid_level`, `to_level`, and `inherit_level` |
| [`held.hpp`](reference/held/README.md) | `current_level` and `held_count`, to look at what a thread holds |
| [`config.hpp`](reference/config/README.md) | Compile-time settings |

The [reference index](reference/README.md) lists every class and function.

## Examples

Each example is a complete program in [`examples/`](../examples). It builds
with the rest of the project, or on its own with one compiler command.

| Example | Shows |
|---|---|
| [Layered locks](examples/layered_locks.md) | Two layers of mutexes, and a lock in the wrong order caught on one thread |
| [Thread pool](examples/thread_pool.md) | A pool built from ordered threads, mutexes and a condition variable, with `task_scope` |
| [Waiting on threads](examples/waiting_on_threads.md) | Latches and promises, including two threads that each wait for the other |
| [Custom policy](examples/custom_policy.md) | A policy that records every violation and reports them at the end |

For an existing program converted to lock_align, see
[lock_align-example-Concurrent-RaceSimulator](https://github.com/eigenstate-solutions/lock_align-example-Concurrent-RaceSimulator),
a fork of [parakhkavin/Concurrent-RaceSimulator](https://github.com/parakhkavin/Concurrent-RaceSimulator).
It is a race simulation, written by someone else, in which racer threads are
coordinated by a game master using mutexes and condition variables.

## Using it in a CMake project

With CMake 3.21 or newer:

```cmake
include(FetchContent)
FetchContent_Declare(lock_align
  GIT_REPOSITORY https://github.com/eigenstate-solutions/lock_align.git
  GIT_TAG v0.8.20261013)
FetchContent_MakeAvailable(lock_align)

target_link_libraries(your_target PRIVATE lock_align::lock_align)
```

To configure **lock_align**, for example to turn the checks off in a
release build, set its [CMake options](reference/config/README.md#cmake-options)
before `FetchContent_MakeAvailable`. That configures every file the same
way, which **lock_align** checks when the program starts.

Or install it and use `find_package(lock_align)`, as the
[top-level README](../README.md#using-it) shows. Or add `include/` to your
include path; nothing needs to be built, though a program using
`ordered_thread` must be linked for threads, as with `-pthread` on Linux.

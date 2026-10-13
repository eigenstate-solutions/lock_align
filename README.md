<p align="center">
  <img src="images/LockAlign-Logo-270x75-white.png" alt="LockAlign" width="294" height="99">
</p>

# **lock_align**

**lock_align** is a header-only C\+\+20 library that finds deadlocks before they
happen. It is similar in spirit to the `hierarchical_mutex` from Anthony
Williams's *C++ Concurrency in Action*: each mutex gets a level, and a
thread may only lock upward. Code that locks in the wrong order is reported
the first time it runs, on one thread, with no unlucky timing needed.

**lock_align** takes the idea further:

- It wraps any mutex type, so you keep `std::mutex`, `std::shared_mutex`,
  or your own.
- Waiting on another thread is checked the same way. This covers joining a
  thread, waiting on a future, a latch or a condition variable, and your
  own waits.
- Violations go to a policy you choose: throw, abort, log, or your own.
- Defining `LOCK_ALIGN_DISABLE` compiles every check away.

## Mutexes locked out of order

```cpp
#include <mutex>
#include "lock_align/ordered_mutex.hpp"

using namespace lock_align;

ordered_mutex<std::mutex, 1> accounts{"accounts"};
ordered_mutex<std::mutex, 2> audit_log{"audit log"};

void transfer() {
  std::lock_guard a(accounts);   // level 1
  std::lock_guard l(audit_log);  // then level 2: fine
}

void rotate_log() {
  std::lock_guard l(audit_log);  // level 2
  std::lock_guard a(accounts);   // then level 1: throws lock_order_error
}
```

If one thread runs `transfer()` while another runs `rotate_log()`, each can
end up holding one mutex while waiting for the other. With **lock_align**,
`rotate_log()` is reported as soon as it is called, even with no other
thread running:

```
lock order violation: blocking on mutex 'accounts' (level 1) while holding mutex 'audit log' (level 2)
```

## Waiting on a promise while holding what it needs

```cpp
#include <mutex>
#include "lock_align/obligation.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_promise.hpp"

using namespace lock_align;

ordered_mutex<std::mutex, 2> cache{"cache"};
ordered_promise<int> config(1, "config");

// The loader owes the config, and needs the cache to produce it.
void load() {
  obligation owed = config.take_obligation();
  std::lock_guard g(cache);  // fine: 2 is above the obligation's level, 1
  config.set_value(42);
}

// Any other thread waiting for the config may not hold the cache.
int lookup(ordered_future<int>& f) {
  std::lock_guard g(cache);  // level 2
  return f.get();            // waits at level 1: throws lock_order_error
}
```

A thread in `lookup()` holds the cache while it waits for the loader, and
the loader can't finish without the cache. **lock_align** reports the wait in
`lookup()` whether or not the loader has started:

```
lock order violation: blocking on obligation 'config' (level 1) while holding mutex 'cache' (level 2)
```

## Learning more

- The [documentation](docs/README.md) explains how levels and waits fit
  together, and has a reference page for every header.
- The [examples](examples) are complete programs, each with a page
  explaining it:
  - [layered locks](docs/examples/layered_locks.md);
  - [a thread pool](docs/examples/thread_pool.md);
  - [threads that wait on each other](docs/examples/waiting_on_threads.md);
  - [a custom violation policy](docs/examples/custom_policy.md).

## Using it

**lock_align** is header-only and needs C++20. Include only the headers you
use. With CMake 3.21 or newer:

```cmake
include(FetchContent)
FetchContent_Declare(lock_align
  GIT_REPOSITORY https://github.com/eigenstate-solutions/lock_align.git
  GIT_TAG <tag>)
FetchContent_MakeAvailable(lock_align)

target_link_libraries(your_target PRIVATE lock_align::lock_align)
```

To configure **lock_align**, for example to turn the checks off in a
release build, set its [CMake options](docs/reference/config/README.md#cmake-options)
before `FetchContent_MakeAvailable`. That configures every file the same
way, which **lock_align** checks when the program starts.

Or install it and use `find_package`:

```sh
cmake -B build -DLOCK_ALIGN_BUILD_TESTS=OFF -DLOCK_ALIGN_BUILD_EXAMPLES=OFF
cmake --install build --prefix /where/to/install
```

```cmake
find_package(lock_align 0.1 REQUIRED)
target_link_libraries(your_target PRIVATE lock_align::lock_align)
```

The installed package keeps the configuration options it was installed with.
`LOCK_ALIGN_INSTALL`, on by default only when **lock_align** is the top-level
project, controls whether the install rules are generated.

Or add `include/` to your include path. Through CMake, `lock_align::lock_align`
links the platform's thread library; without CMake, link it yourself where
`std::thread` needs it, as with `-pthread` on Linux.

To build and run the tests and examples, which need CMake 3.24 or newer:

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

Two options apply only to the tests and examples, never to projects using
**lock_align**:

| Option | Default | Effect |
|---|---|---|
| `LOCK_ALIGN_SANITIZE` | empty | Sanitizers to build with, as a list: `address`, `undefined`, `thread`. `thread` can't be combined with `address`, and MSVC offers only `address`. |
| `LOCK_ALIGN_WARNINGS_AS_ERRORS` | `ON` | Treat warnings as errors. Configuring with `cmake --compile-no-warning-as-error` also turns it off. |

For example, `cmake -B build-asan "-DLOCK_ALIGN_SANITIZE=address;undefined"`.

## License

**lock_align** is released into the public domain under the
[Unlicense](UNLICENSE.txt).

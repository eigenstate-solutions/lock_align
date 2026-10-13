# ordered_future

```cpp
template <class T, class Policy = default_policy>
class ordered_future;
```

**Header:** [`<lock_align/ordered_promise.hpp>`](README.md)

A `std::future<T>` obtained from an
[`ordered_promise`](ordered_promise.md). `get()` and `wait()` are checked
as waits on the promise's obligation: the calling thread may hold mutexes
up to and including the promise's level, and obligations below it.

The timed waits aren't checked, because they can't wait forever.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Default: no shared state |
| [`get`](#get-wait) | Checks, then waits for and returns the value |
| [`wait`](#get-wait) | Checks, then waits |
| [`wait_for`, `wait_until`](#wait_for-wait_until) | Waits with a timeout; not checked |
| [`valid`](#valid-level-name) | Whether it refers to a shared state |
| [`level`](#valid-level-name) | The promise's level |
| [`name`](#valid-level-name) | The promise's name |

Movable, not copyable. Only `ordered_promise::get_future` can make a valid
one.

---

### (constructor)

```cpp
ordered_future() noexcept;
```

### get, wait

```cpp
decltype(auto) get();
void wait() const;
```

Checks that the calling thread may wait on an obligation at the promise's
level, then does the same as on `std::future`.

**Exceptions:** whatever `Policy::on_violation` throws, before waiting;
then whatever `std::future` throws, including the exception the producer
set.

### wait_for, wait_until

```cpp
template <class Rep, class Period>
std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout) const;

template <class Clock, class Duration>
std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& deadline) const;
```

The same as on `std::future`. Not checked.

### valid, level, name

```cpp
bool valid() const noexcept;
level_t level() const noexcept;
std::string name() const;
```

The level and name are the promise's. The name is `""` when
[`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is defined.

Returned by value. A default name is stored as the source location and
built into `"file:line"` on each call, so that creating the object doesn't
allocate.

## Example

```cpp
lock_align::ordered_promise<void> done(2);
lock_align::ordered_future<void> f = done.get_future();
lock_align::ordered_mutex<std::mutex, 3> m;

std::lock_guard g(m);  // level 3, above the promise
f.wait();              // throws lock_order_error
```

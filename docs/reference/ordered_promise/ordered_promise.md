# ordered_promise

```cpp
template <class T, class Policy = default_policy,
          obligation_check Check = default_obligation_check>
class ordered_promise;
```

**Header:** [`<lock_align/ordered_promise.hpp>`](README.md)

A `std::promise<T>` with a level. The thread that will set the value calls
[`take_obligation()`](#take_obligation) and holds the result until it has
set the value. Waiting on the [`ordered_future`](ordered_future.md) is
checked against the same level.

So the producer may only block on mutexes above the level, and a consumer
may hold mutexes up to and including it. A consumer can't wait while
holding something the producer needs. By default, `set_value` and
`set_exception` check that the producer holds that obligation; see
[`obligation_check`](../obligation/README.md#obligation_check).

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The value type, as for `std::promise` |
| `Policy` | What to do when a wait on the future, or a missing obligation, is a violation |
| `Check` | Whether setting the value needs the promise's obligation; see [`obligation_check`](../obligation/README.md#obligation_check) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs a promise at a level |
| [`get_future`](#get_future) | Returns the future |
| [`take_obligation`](#take_obligation) | Returns an obligation for the producer to hold |
| [`set_value`](#set_value-set_exception) | Checks for the obligation, then sets the value |
| [`set_exception`](#set_value-set_exception) | Checks for the obligation, then sets an exception |
| [`level`](#level-name) | The promise's level |
| [`name`](#level-name) | The name shown in reports |

Movable, not copyable. An obligation taken from a promise still counts
after the promise is moved: each promise has a number of its own, which
moves with it, and its obligations are recorded under that number. Taking
the number doesn't allocate, and numbers are never reused, so an obligation
that outlives its promise never counts for a later one.

---

### (constructor)

```cpp
template <level_like L>
explicit ordered_promise(L level, std::string_view name = {},
                         std::source_location where = std::source_location::current());

explicit ordered_promise(inherit_level_t, std::string_view name = {},
                         std::source_location where = std::source_location::current());
```

The second form uses the lowest level the creating thread could wait at
while holding what it holds now; see
[`inherit_level`](../level/README.md#inherit_level).

`name` appears in violation reports. If it's empty, the name is
`"file:line"` taken from `where`, the path as the compiler was given it.
Leave `where` defaulted; it records the caller's location.

**Exceptions:** in the first form, [`std::out_of_range`](../level/README.md#to_level) if `level` is negative or
too large for [`level_t`](../level/README.md#level_t).

### get_future

```cpp
ordered_future<T, Policy> get_future();
```

Returns the future, carrying the promise's level and name. Can be called
once, as with `std::promise`.

### take_obligation

```cpp
[[nodiscard]] obligation take_obligation() const;
```

Returns an [`obligation`](../obligation/obligation.md) at the promise's
level and with its name. Call it on the producing thread and keep it until
the value is set:

```cpp
lock_align::obligation duty = promise.take_obligation();
promise.set_value(compute());
```

### set_value, set_exception

```cpp
template <class... Args>
void set_value(Args&&... args);

void set_exception(std::exception_ptr e);
```

With `Check` `required`, checks that the calling thread holds an obligation
taken from this promise, reporting a
[`missing_obligation_violation`](../violation/missing_obligation_violation.md)
if not. Then the same as on `std::promise`. They never block.

A moved-from promise isn't checked: `std::promise` reports that it has no
state.

**Exceptions:** whatever `Policy::on_violation` throws, before setting
anything; then whatever `std::promise` throws, such as `std::future_error`.

### level, name

```cpp
level_t level() const noexcept;
std::string name() const;
```

The promise's level and name. The name is `""` when
[`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is defined.

Returned by value. A default name is stored as the source location and
built into `"file:line"` on each call, so that creating the object doesn't
allocate.

## Example

```cpp
lock_align::ordered_promise<int> answer(2, "answer");
lock_align::ordered_future<int> result = answer.get_future();

std::thread producer([&] {
  lock_align::obligation duty = answer.take_obligation();
  answer.set_value(42);  // may only lock above level 2 before this
});

int value = result.get();  // may hold locks up to level 2
producer.join();
```

## See also

- [`inherit_level`](../level/README.md#inherit_level)
- [Waiting on threads example](../../examples/waiting_on_threads.md)

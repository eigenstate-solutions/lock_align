# `<lock_align/held.hpp>`

Functions to look at the calling thread's record of the mutexes and
obligations it holds. The record itself, and the check every blocking
operation runs against it, are internal.

## Functions

| Function | Description |
|---|---|
| [`current_level`](#current_level) | Highest level this thread holds |
| [`held_count`](#held_count) | How many entries this thread holds |

---

### current_level

```cpp
level_t current_level() noexcept;
```

Returns the highest level of any mutex or obligation the calling thread
holds, or 0 if it holds none. Level-0 mutexes aren't tracked, so they never
count. Always 0 when [`LOCK_ALIGN_DISABLE`](../config/README.md) is
defined.

Useful in assertions:

```cpp
// Callers may hold accounts, but nothing locked after it.
void post(entry e) {
  assert(lock_align::current_level() <= lock_align::to_level(level::accounts));
  ...
}
```

### held_count

```cpp
std::size_t held_count() noexcept;
```

Returns how many entries the calling thread holds: one for each lock of a
tracked mutex (each relock of a recursive mutex counts), and one for each
obligation. Always 0 when [`LOCK_ALIGN_DISABLE`](../config/README.md) is
defined.

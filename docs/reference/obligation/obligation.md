# obligation

```cpp
class obligation;
```

**Header:** [`<lock_align/obligation.hpp>`](README.md)

Declares that the current thread owes work, at some level, that another
thread may be waiting for. While the obligation lives, this thread may only
block on mutexes above its level, and on obligations above its level.
Anyone waiting for the work checks against the same level, with
[`check_wait`](README.md#check_wait) or one of the library's wrappers.

An obligation is recorded on the thread that constructs it, and removed
when it's destroyed, so it should be destroyed on the same thread. It can't
be copied or moved. A function can still return one, as
`ordered_promise::take_obligation` does, because returning a temporary
doesn't copy it.

Level 0 is meaningful: the owing thread may lock any ordered mutex, and a
waiter must hold no ordered mutex.

An obligation returned by a `take_obligation()` is recorded as taken from
that condition variable, latch or promise. That's how the object's
[obligation check](README.md#obligation_check) knows the thread holds one
for it, rather than for something else at the same level. An obligation
taken from a promise still counts after the promise is moved.

### An obligation must not outlive its source

A condition variable's or latch's obligations are recorded under its
address. If the condition variable or latch is destroyed while one of its
obligations is still held, and a new one is later created at the same
address, the old obligation counts for the new one. Notifying or counting
down on the new one without taking its obligation then isn't reported.

That only happens after a lifetime bug, and it can only hide a report,
never cause a false one. Destroy every obligation before the condition
variable or latch it was taken from. Promises don't have this problem:
their obligations are recorded under a number that's never reused.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Records the obligation on this thread |
| [(destructor)](#destructor) | Removes it |
| [`level`](#level-name) | Its level |
| [`name`](#level-name) | Its name |

---

### (constructor)

```cpp
template <level_like L>
explicit obligation(L level, std::string_view name = {},
                    std::source_location where = std::source_location::current());
```

Records an obligation at `level` on the calling thread. Constructing one
is never a violation: taking on work doesn't block.

`name` appears in violation reports. If it's empty, the name is
`"file:line"` taken from `where`, the path as the compiler was given it.
Leave `where` defaulted; it records the caller's location.

**Exceptions:** [`std::out_of_range`](../level/README.md#to_level) if `level` is negative or
too large for [`level_t`](../level/README.md#level_t); `std::bad_alloc` if the thread's record
must [grow](../config/README.md#lock_align_held_slots_reserved) and can't.

### (destructor)

```cpp
~obligation();
```

Removes the obligation from the calling thread's record. Destroy it once
the work is done, typically after setting a promise's value or counting
down a latch.

### level, name

```cpp
level_t level() const noexcept;
std::string name() const;
```

The obligation's level and name. The name is `""` when
[`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is defined.

Returned by value. A default name is stored as the source location and
built into `"file:line"` on each call, so that creating the object doesn't
allocate.

## Example

```cpp
// The I/O thread signals done_event when it has finished. Anyone waiting
// for that may hold locks up to level::io.
std::thread io([&] {
  lock_align::obligation duty(level::io, "io done");
  write_everything();  // may only block on mutexes above level::io
  done_event.signal();
});

lock_align::check_wait(level::io, "io done");
done_event.wait();
io.join();
```

## See also

- [`ordered_promise::take_obligation`](../ordered_promise/ordered_promise.md#take_obligation)
- [`ordered_latch::take_obligation`](../ordered_latch/ordered_latch.md#take_obligation)

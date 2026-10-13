# ordered_latch

```cpp
template <class Policy = default_policy, obligation_check Check = default_obligation_check>
class ordered_latch;
```

**Header:** [`<lock_align/ordered_latch.hpp>`](README.md)

A `std::latch` with a level. Each thread that will count down calls
[`take_obligation()`](#take_obligation) and holds the result until it has
counted down. `wait()` and `arrive_and_wait()` are checked against the same
level. By default, `count_down()` and `arrive_and_wait()` also check that
the thread holds that obligation; see
[`obligation_check`](../obligation/README.md#obligation_check).

A thread that both counts down and waits, through `arrive_and_wait`, keeps
its obligation until the call: arriving fulfils it, so the wait ignores
obligations taken from this latch. A plain `wait()` doesn't, and holding
the latch's obligation there is reported, since it would be waiting on
itself.

## Template parameters

| Parameter | Description |
|---|---|
| `Policy` | What to do when a wait or a missing obligation is a violation |
| `Check` | Whether counting down needs the latch's obligation; see [`obligation_check`](../obligation/README.md#obligation_check) |

A declaration with arguments deduces the default policy, as in
`ordered_latch done(1, 2);`. Where the type is named on its own, such as
a parameter or a container's element type, write `ordered_latch<>`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs a latch at a level |
| [`count_down`](#count_down) | Checks for the obligation, then decrements the counter |
| [`try_wait`](#try_wait) | Whether the counter is zero; not checked |
| [`wait`](#wait) | Checks, then waits for zero |
| [`arrive_and_wait`](#arrive_and_wait) | Checks for the obligation and the wait, then counts down and waits |
| [`take_obligation`](#take_obligation) | Returns an obligation for a counting thread to hold |
| [`level`](#level-name) | The latch's level |
| [`name`](#level-name) | The name shown in reports |

Not copyable or movable.

---

### (constructor)

```cpp
template <level_like L>
explicit ordered_latch(std::ptrdiff_t expected, L level, std::string_view name = {},
                       std::source_location where = std::source_location::current());

explicit ordered_latch(std::ptrdiff_t expected, inherit_level_t, std::string_view name = {},
                       std::source_location where = std::source_location::current());
```

`expected` is the initial count, as for `std::latch`. The second form uses
the lowest level the creating thread could wait at while holding what it
holds now; see [`inherit_level`](../level/README.md#inherit_level). `name` appears in violation reports. If it's empty, the name is
`"file:line"` taken from `where`, the path as the compiler was given it.
Leave `where` defaulted; it records the caller's location.

**Exceptions:** in the first form, [`std::out_of_range`](../level/README.md#to_level) if `level` is negative or
too large for [`level_t`](../level/README.md#level_t).

### count_down

```cpp
void count_down(std::ptrdiff_t n = 1);
```

With `Check` `required`, checks that the calling thread holds an obligation
taken from this latch, reporting a
[`missing_obligation_violation`](../violation/missing_obligation_violation.md)
if not. Then the same as on `std::latch`. Never blocks.

**Exceptions:** whatever `Policy::on_violation` throws, before counting
down.

### try_wait

```cpp
bool try_wait() const noexcept;
```

The same as on `std::latch`. Not checked; it never blocks.

### wait

```cpp
void wait() const;
```

Checks that the calling thread may wait on an obligation at the latch's
level, then does the same as on `std::latch`. Holding an obligation taken
from this latch is reported: the thread would be waiting on itself.

**Exceptions:** whatever `Policy::on_violation` throws, before waiting.

### arrive_and_wait

```cpp
void arrive_and_wait(std::ptrdiff_t n = 1);
```

Counts down and waits, checked as both:

1. as [`count_down`](#count_down), that the thread holds an obligation
   taken from this latch, with `Check` `required`;
2. as [`wait`](#wait), that the thread may wait at the latch's level,
   ignoring obligations taken from this latch, which arriving fulfils.

Both checks come first, so a violation doesn't count down. Release the
obligation once the call returns.

**Exceptions:** whatever `Policy::on_violation` throws, before counting
down.

### take_obligation

```cpp
[[nodiscard]] obligation take_obligation() const;
```

Returns an [`obligation`](../obligation/obligation.md) at the latch's level
and with its name.

Destroy the obligation before the latch. One that outlives it may count
for a new latch created at the same address; see
[An obligation must not outlive its source](../obligation/obligation.md#an-obligation-must-not-outlive-its-source).

### level, name

```cpp
level_t level() const noexcept;
std::string name() const;
```

The latch's level and name. The name is `""` when
[`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is defined.

Returned by value. A default name is stored as the source location and
built into `"file:line"` on each call, so that creating the object doesn't
allocate.

## Example

```cpp
lock_align::ordered_latch ready(1, 2, "ready");

std::thread loader([&] {
  lock_align::obligation duty = ready.take_obligation();
  load();
  ready.count_down();
});

ready.wait();  // may hold locks up to level 2
loader.join();
```

## See also

- [`inherit_level`](../level/README.md#inherit_level)
- [Waiting on threads example](../../examples/waiting_on_threads.md)

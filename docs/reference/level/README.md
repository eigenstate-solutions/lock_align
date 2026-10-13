# `<lock_align/level.hpp>`

Levels: the numbers that put mutexes and obligations in order. See
[How **lock_align** works](../../concepts.md) for the rule they follow.

## Types and concepts

| Name | Description |
|---|---|
| [`level_t`](#level_t) | The type of a level |
| [`level_like`](#level_like) | Types a level can be written as |

## Constants

| Name | Description |
|---|---|
| [`inherit_level`](#inherit_level) | Tag: use the lowest level the creating thread could wait at |

## Functions

| Function | Description |
|---|---|
| [`is_valid_level`](#is_valid_level) | Whether a value is in range for a level |
| [`to_level`](#to_level) | Converts an integer or enum to `level_t` |

---

### level_t

```cpp
using level_t = std::uint32_t;
```

The type of a level. Higher levels are locked later.

### level_like

```cpp
template <class L>
concept level_like = /* see below */;
```

Satisfied by any integer type except `bool` and the character types
(`char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`), and by any enum,
scoped or not, whose underlying type is one of those integers.
`signed char` and `unsigned char` count as integers, so `std::int8_t` and
`std::uint8_t` work. Anywhere **lock_align**
takes a level, it accepts a `level_like` value, so you can keep your levels
in an enum:

```cpp
enum class level { registry = 10, account = 20, log = 90 };
ordered_mutex<std::mutex, level::account> m;
```

A value must also be from 0 to 4294967295, the range of `level_t`; see
[`to_level`](#to_level).

### is_valid_level

```cpp
template <level_like L>
constexpr bool is_valid_level(L level) noexcept;
```

Returns whether `level`, or an enum's underlying value, is from 0 to
4294967295.

### to_level

```cpp
template <level_like L>
constexpr level_t to_level(L level);
```

Returns `level`, or an enum's underlying value, as a `level_t`.

**Exceptions:** `std::out_of_range` if
[`is_valid_level(level)`](#is_valid_level) is false. In a constant
expression that's a compile error instead, so an `ordered_mutex` or
`ordered_condition_variable` with a negative level doesn't compile. Every
constructor that takes a level at run time converts it with `to_level`,
and so throws too.

Use it to compare an enum level with a `level_t`, such as the result of
[`current_level`](../held/README.md#current_level):

```cpp
assert(current_level() <= to_level(level::account));
```

### inherit_level

```cpp
struct inherit_level_t { explicit inherit_level_t() = default; };
inline constexpr inherit_level_t inherit_level{};
```

Passed in place of a level to an
[`ordered_thread`](../ordered_thread/ordered_thread.md), an
[`ordered_promise`](../ordered_promise/ordered_promise.md) or an
[`ordered_latch`](../ordered_latch/ordered_latch.md). The level used
is the lowest one the creating thread could wait at while still holding
everything it holds now:

| Creating thread's highest entry | Level used |
|---|---|
| nothing | 0 |
| a mutex at level N | N |
| an obligation at level N | N + 1 |

The level is worked out once, when the thread, promise or latch is
created.

Use it when the creating thread will wait for the thread, promise or latch
before releasing what it holds. If it will release some locks first, a lower
explicit level is better: the thread doing the work may only lock above
its level, so inheriting makes it stricter than it needs to be.

```cpp
std::lock_guard g(index_mutex);  // level 3
lock_align::ordered_thread helper(lock_align::inherit_level, "helper", [&] {
  std::lock_guard s(stats_mutex);  // level 4: fine
});
helper.join();  // fine: helper is at level 3
```

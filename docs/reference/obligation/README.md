# `<lock_align/obligation.hpp>`

Obligations: how waiting on another thread fits into the lock order.

Whoever will do some work holds an obligation at level N for as long as
someone may be waiting for it. While it does, it may only block on mutexes
above N. Waiting for that work is a blocking acquire at level N, so the
waiter may hold mutexes up to and including N, never above. See
[How **lock_align** works](../../concepts.md#waiting-on-other-threads).

[`ordered_thread`](../ordered_thread/ordered_thread.md),
[`ordered_promise`](../ordered_promise/ordered_promise.md) and
[`ordered_latch`](../ordered_latch/ordered_latch.md) use obligations
for you. Use this header directly for waits they don't cover.

## Classes

| Class | Description |
|---|---|
| [`obligation`](obligation.md) | Declares that this thread owes work at a level |

## Types and constants

| Name | Description |
|---|---|
| [`obligation_check`](#obligation_check) | Whether signalling needs the obligation |
| [`default_obligation_check`](#default_obligation_check) | The setting used when none is given |

## Functions

| Function | Description |
|---|---|
| [`check_wait`](#check_wait) | Checks a wait the library doesn't wrap |

---

### obligation_check

```cpp
enum class obligation_check { unchecked, required };
```

A template parameter of
[`ordered_condition_variable`](../ordered_condition_variable/ordered_condition_variable.md),
[`ordered_latch`](../ordered_latch/ordered_latch.md) and
[`ordered_promise`](../ordered_promise/ordered_promise.md). With
`required`, the operations that release waiters report a thread that
doesn't hold an obligation taken from that object, through the object's
policy, as a
[`missing_obligation_violation`](../violation/missing_obligation_violation.md):

| Class | Checked operations |
|---|---|
| `ordered_condition_variable` | `notify_one`, `notify_all` |
| `ordered_latch` | `count_down`, `arrive_and_wait` |
| `ordered_promise` | `set_value`, `set_exception` |

An obligation is what checks the signalling thread's side of a wait. If
nothing requires it, forgetting it is silent, and a deadlock with a waiter
shows up only when the timing goes wrong.

The check can only tell that the obligation is held *at* the operation,
not that it was taken early enough. Take it before anyone could start
waiting for you, and keep it until you've signalled. Taking it just before
the operation satisfies the check but checks nothing.

With `unchecked`, those operations aren't checked, and forgetting the
obligation is silent.

### default_obligation_check

```cpp
inline constexpr obligation_check default_obligation_check =
    obligation_check::LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK;
```

What each class uses when its `obligation_check` parameter is left out.
`required` unless
[`LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK`](../config/README.md#lock_align_default_obligation_check)
is defined.

### check_wait

```cpp
template <class Policy = default_policy, level_like L>
void check_wait(L level, std::string_view name = {},
                std::source_location where = std::source_location::current());
```

Call before blocking on something another thread completes while holding an
[`obligation`](obligation.md) at `level`. Checks that everything the
calling thread holds ranks below an obligation at that level: any mutex up
to and including `level`, and any obligation below it. If not,
`Policy::on_violation` is called; `name` appears in the report as the
thing being waited on. If `name` is empty, the report uses `"file:line"`
taken from `where`. Leave `where` defaulted; it records the caller's
location.

`level` can be any integer or enum value; see
[`level_like`](../level/README.md#level_like).

**Exceptions:** [`std::out_of_range`](../level/README.md#to_level) if `level` is negative or
too large for [`level_t`](../level/README.md#level_t), unless
[`LOCK_ALIGN_DISABLE`](../config/README.md#lock_align_disable) is defined;
whatever `Policy::on_violation` throws.

```cpp
// The I/O thread holds an obligation at level::io until it signals done.
lock_align::check_wait(level::io, "io done");
done_event.wait();  // a primitive lock_align doesn't wrap
```

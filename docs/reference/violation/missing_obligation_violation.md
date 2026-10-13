# missing_obligation_violation

```cpp
class missing_obligation_violation final : public violation;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

A thread notified a condition variable, counted down a latch, or set a
promise's value or exception without holding an
[obligation](../obligation/obligation.md) taken from it. Reported only by
resources whose [`obligation_check`](../obligation/README.md#obligation_check)
is `required`, which is the default.

The obligation is what checks the signalling thread's side: while it's
held, the thread can't block on anything a waiter may hold. Without it,
that side isn't checked at all, so a deadlock with a waiter shows up only
when the timing goes wrong.

Thrown as a [`missing_obligation_error`](missing_obligation_error.md) by
[`throw_on_violation`](throw_on_violation.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs from the resource and the operation |
| [`resource`](#resource) | The condition variable, latch or promise |
| [`needed_for`](#needed_for) | What the thread was doing |
| [`message`](#message) | Describes the violation in one line |
| [`throw_error`](#throw_error) | Throws a `missing_obligation_error` |

---

### (constructor)

```cpp
missing_obligation_violation(resource_info resource, obligation_needed_for needed_for);
```

### resource

```cpp
const resource_info& resource() const noexcept;
```

The condition variable, latch or promise, as a
[`resource_info`](resource_info.md): its name and level, with kind
`resource_kind::obligation`.

### needed_for

```cpp
obligation_needed_for needed_for() const noexcept;
```

The [operation](README.md#obligation_needed_for) that needed the
obligation.

### message

```cpp
std::string message() const override;
```

For example:

```
missing obligation: notifying 'ready' (level 1) without holding its obligation
missing obligation: counting down 'loaded' (level 2) without holding its obligation
missing obligation: setting the value of 'answer' (level 2) without holding its obligation
missing obligation: setting an exception in 'answer' (level 2) without holding its obligation
```

### throw_error

```cpp
[[noreturn]] void throw_error() const override;
```

Throws [`missing_obligation_error(*this)`](missing_obligation_error.md).

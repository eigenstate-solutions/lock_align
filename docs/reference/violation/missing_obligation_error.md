# missing_obligation_error

```cpp
class missing_obligation_error final : public violation_error;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

The exception [`throw_on_violation`](throw_on_violation.md), the default
policy, throws for a
[`missing_obligation_violation`](missing_obligation_violation.md). It's
thrown before the operation, so nothing was notified, counted down or set.

`what()` returns [`info().message()`](missing_obligation_violation.md#message).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs from a `missing_obligation_violation` |
| [`info`](#info) | The violation |
| `what` | Inherited from `std::logic_error` |

---

### (constructor)

```cpp
explicit missing_obligation_error(missing_obligation_violation info);
```

### info

```cpp
const missing_obligation_violation& info() const noexcept override;
```

Returns the [`missing_obligation_violation`](missing_obligation_violation.md)
the error was built from.

## Example

```cpp
lock_align::ordered_condition_variable<1> ready{"ready"};

try {
  ready.notify_one();  // no obligation taken from ready
} catch (const lock_align::missing_obligation_error& e) {
  std::cerr << e.what() << '\n';
}

{
  lock_align::obligation duty = ready.take_obligation();
  ready.notify_one();  // fine
}
```

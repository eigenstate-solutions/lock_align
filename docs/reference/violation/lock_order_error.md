# lock_order_error

```cpp
class lock_order_error final : public violation_error;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

The exception [`throw_on_violation`](throw_on_violation.md), the default
policy, throws for a [`lock_order_violation`](lock_order_violation.md). It's
thrown before the thread blocks, so whatever it was about to lock or wait
for is untouched, and everything it already held is still held.

`what()` returns [`info().message()`](lock_order_violation.md#message).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs from a `lock_order_violation` |
| [`info`](#info) | The violation |
| `what` | Inherited from `std::logic_error` |

---

### (constructor)

```cpp
explicit lock_order_error(lock_order_violation info);
```

### info

```cpp
const lock_order_violation& info() const noexcept override;
```

Returns the [`lock_order_violation`](lock_order_violation.md) the error was
built from.

## Example

```cpp
try {
  std::lock_guard g(low);  // while holding a higher-level mutex
} catch (const lock_align::lock_order_error& e) {
  std::cerr << e.what() << '\n';
  if (e.info().held().kind == lock_align::resource_kind::obligation) { /* ... */ }
}
```

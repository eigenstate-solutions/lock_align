# lock_order_violation

```cpp
class lock_order_violation final : public violation;
```

**Header:** [`<lock_align/violation.hpp>`](README.md)

A thread was about to block on a resource while holding one ranked at or
above it: a mutex lock, or a wait on another thread. The *requested*
resource is the one it was about to block on; the *held* one is the
highest-ranked resource it already held.

Thrown as a [`lock_order_error`](lock_order_error.md) by
[`throw_on_violation`](throw_on_violation.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Constructs from both resources |
| [`requested`](#requested) | The resource the thread was about to block on |
| [`held`](#held) | The highest-ranked resource the thread held |
| [`message`](#message) | Describes the violation in one line |
| [`throw_error`](#throw_error) | Throws a `lock_order_error` |

---

### (constructor)

```cpp
lock_order_violation(resource_info requested, resource_info held);
```

### requested

```cpp
const resource_info& requested() const noexcept;
```

The [resource](resource_info.md) the thread was about to block on.

### held

```cpp
const resource_info& held() const noexcept;
```

The [resource](resource_info.md) the thread held that made blocking a violation: the
highest-ranked one, apart from any the operation ignores, such as the mutex
a condition variable's wait releases.

### message

```cpp
std::string message() const override;
```

For example:

```
lock order violation: blocking on mutex 'catalog' (level 1) while holding mutex 'widget' (level 2)
```

### throw_error

```cpp
[[noreturn]] void throw_error() const override;
```

Throws [`lock_order_error(*this)`](lock_order_error.md).

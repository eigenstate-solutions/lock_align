# ordered_scoped_lock

```cpp
template <class... M>
class ordered_scoped_lock;
```

**Header:** [`<lock_align/ordered_lock.hpp>`](README.md)

Locks several ordered mutexes for the lifetime of a scope, like
`std::scoped_lock`, but locks them with
[`ordered_lock`](README.md#ordered_lock): in rising order of level, so the
same set is checked the same way whatever the argument order or timing.

Each `M` must be an [`ordered_mutex`](../ordered_mutex/ordered_mutex.md)
with a level other than 0. The template arguments are deduced from the
constructor, as in `ordered_scoped_lock both(a, b);`.

## Member types

| Type | Definition |
|---|---|
| `mutex_type` | `M`, only when there is exactly one mutex |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](#constructor) | Locks the mutexes, or adopts them |
| [(destructor)](#destructor) | Unlocks the mutexes |

Not copyable or movable.

---

### (constructor)

```cpp
explicit ordered_scoped_lock(M&... m);
explicit ordered_scoped_lock(std::adopt_lock_t, M&... m) noexcept;
```

The first form calls `ordered_lock(m...)`. The second takes over mutexes
the calling thread has already locked.

**Exceptions:** the first form, whatever `ordered_lock` throws. No mutex is
left locked.

### (destructor)

```cpp
~ordered_scoped_lock();
```

Unlocks every mutex, highest level first: the reverse of the order they
were locked in.

## Example

```cpp
ordered_mutex<std::mutex, 1> accounts("accounts");
ordered_mutex<std::mutex, 2> from("from"), to("to");

void transfer() {
  // Locks accounts, then from and to together.
  lock_align::ordered_scoped_lock all(to, from, accounts);
  move_money();
}
```

## See also

- [`ordered_mutex`](../ordered_mutex/ordered_mutex.md)
- [How **lock_align** works](../../concepts.md)

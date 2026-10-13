# `<lock_align/ordered_lock.hpp>`

Locking several ordered mutexes at once, in a known order.

`std::lock` and `std::scoped_lock` work with
[`ordered_mutex`](../ordered_mutex/ordered_mutex.md) and don't deadlock
among themselves, but which member they block on depends on the argument
order and on timing. Only that blocking lock is checked, so whether a bad
set is reported depends on both. Holding a mutex at level 2,
`std::scoped_lock(three, one)` blocks on `three` and only tries `one`: the
mistake goes unreported unless `one` happens to be busy. The standard
doesn't specify the algorithm either, so another standard library could
behave differently.

`ordered_lock` and `ordered_scoped_lock` lock in rising order of level,
so the same set is checked the same way every time.

## Classes

| Class | Description |
|---|---|
| [`ordered_scoped_lock`](ordered_scoped_lock.md) | Holds several ordered mutexes for a scope, like `std::scoped_lock` |

## Functions

| Function | Description |
|---|---|
| [`ordered_lock`](#ordered_lock) | Locks several ordered mutexes, like `std::lock` |

---

### ordered_lock

```cpp
template <class... M>
void ordered_lock(M&... m);
```

Locks every mutex in `m`:

1. Groups them by level and locks the groups from the lowest level up.
   A group of one is locked with its blocking `lock()`.
2. Within a group of mutexes at the same level, blocks on one while
   holding none of the others, then `try_lock`s the rest. If one is busy,
   it releases the group and blocks on that one instead. So mutexes at one
   level can be locked together, which one after the other can't.

Every blocking lock is an ordinary `lock()` on a member, checked and
reported through that mutex's own policy. Each is made while holding only
lower levels from the set, so the set never conflicts with itself. If the
lowest level passes the check against what the thread already holds,
every higher level does too: a call reports at most one violation, and it
always names a lowest-level member.

Each `M` must be an [`ordered_mutex`](../ordered_mutex/ordered_mutex.md)
with a level other than 0; anything else doesn't compile. Level-0 and plain
mutexes aren't tracked, so they can't be put in order. Use `std::lock` for
those.

Passing the same mutex twice is undefined, as with `std::lock`.

With no arguments, does nothing.

**Exceptions:** whatever a member's `lock()` or `try_lock()` throws,
including what its `Policy::on_violation` throws. The mutexes this call had
already locked are unlocked first, so the thread holds none of the set.

```cpp
ordered_mutex<std::mutex, 1> accounts;
ordered_mutex<std::mutex, 2> ledger_a, ledger_b;

lock_align::ordered_lock(ledger_b, accounts, ledger_a);
// accounts was locked first, then ledger_a and ledger_b together.
```

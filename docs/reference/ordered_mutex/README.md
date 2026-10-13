# `<lock_align/ordered_mutex.hpp>`

A mutex wrapper that checks lock ordering.

## Classes

| Class | Description |
|---|---|
| [`ordered_mutex`](ordered_mutex.md) | Wraps any mutex type and checks every blocking lock |
| [`is_recursive_mutex`](is_recursive_mutex.md) | Trait: whether a mutex type may be relocked by its owner |

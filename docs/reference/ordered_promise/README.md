# `<lock_align/ordered_promise.hpp>`

A promise and future whose waits are checked like locks.

## Classes

| Class | Description |
|---|---|
| [`ordered_promise`](ordered_promise.md) | A `std::promise` with a level; the producer holds its obligation |
| [`ordered_future`](ordered_future.md) | A `std::future` whose `get()` and `wait()` are checked |

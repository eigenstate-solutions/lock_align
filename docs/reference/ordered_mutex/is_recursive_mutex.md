# is_recursive_mutex

```cpp
template <class Mutex>
struct is_recursive_mutex : std::false_type {};

template <> struct is_recursive_mutex<std::recursive_mutex> : std::true_type {};
template <> struct is_recursive_mutex<std::recursive_timed_mutex> : std::true_type {};
```

**Header:** [`<lock_align/ordered_mutex.hpp>`](README.md)

Tells [`ordered_mutex`](ordered_mutex.md) whether `Mutex` may be locked
again by the thread that already holds it. When it may,
`ordered_mutex::lock()` skips the check for that relock. Each lock is still
recorded, and each `unlock()` removes one.

Specialize it for recursive mutex types of your own:

```cpp
template <>
struct lock_align::is_recursive_mutex<my::recursive_spinlock> : std::true_type {};
```

# Layered locks

Source: [`examples/layered_locks.cpp`](../../examples/layered_locks.cpp)

A catalog holds items. The catalog has a mutex, and so does each item. Code
that needs both should lock the catalog first. This example shows one
function that does, and one that doesn't.

## The levels

```cpp
// Lower levels are locked first.
enum class level { catalog = 1, item = 2 };
```

The catalog's mutex is `ordered_mutex<std::mutex, level::catalog>`; each
item's is `ordered_mutex<std::mutex, level::item>`, named after the item.

## Right order

`restock` locks the catalog to find the item, then the item to change it:

```cpp
void restock(const std::string& name, int count) {
  std::lock_guard g(mutex_);
  item& it = items_.at(name);
  std::lock_guard gi(it.mutex);
  it.stock += count;
}
```

## Wrong order

`sell` locks the item, then the catalog to record the sale:

```cpp
void sell(item& it) {
  std::lock_guard gi(it.mutex);
  --it.stock;
  std::lock_guard g(mutex_);  // reported here, before it can block
  ++sales_;
}
```

If one thread were in `sell` and another in `restock` for the same item,
each could end up holding the lock the other needs. That only happens when
the timing is unlucky, so a test could pass a thousand times and still miss
it.

**lock_align** doesn't need the unlucky timing. This program has one thread and
calls `sell` once, and the second lock is refused right away. The default
policy throws `lock_order_error` before locking. The item's lock guard
releases the item as the exception leaves `sell`, and `sales_` is never
changed.

## Output

```
restocked: 5 widgets
lock order violation: blocking on mutex 'catalog' (level 1) while holding mutex 'widget' (level 2)
sales recorded: 0
```

## The fix

Lock the catalog first in `sell`, or record the sale after releasing the
item.

## Building and running

From the top of the repository, with CMake:

```sh
cmake -S . -B build
cmake --build build --target layered_locks
./build/examples/layered_locks
```

Or with one compiler command (add `-pthread` on Linux):

```sh
c++ -std=c++20 -Iinclude examples/layered_locks.cpp -o layered_locks
./layered_locks
```

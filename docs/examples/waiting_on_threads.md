# Waiting on threads

Source: [`examples/waiting_on_threads.cpp`](../../examples/waiting_on_threads.cpp)

Two ways a thread can wait for others while holding something they need,
and how **lock_align** reports both before anything blocks.

## The levels

```cpp
enum class level {
  threads = 1,  // every thread in this program
  loaded = 2,   // the latch, and both services' promises
  summary = 3,  // what the loaders write
  output = 4,   // printing, always taken last
};
```

## Part 1: a latch

Three loader threads each add to a shared summary, under `summary_mutex`,
then count down an [`ordered_latch`](../reference/ordered_latch/ordered_latch.md).
Each holds the latch's obligation until it has counted down:

```cpp
threads.emplace_back(level::threads, "loader", [&, i] {
  obligation duty = loaded.take_obligation();
  {
    std::lock_guard g(summary_mutex);
    summary += i * 10;
  }
  loaded.count_down();
});
```

Each loader holds two obligations at once: its thread's, at
`level::threads`, for as long as it runs, and the latch's, at
`level::loaded`, until it counts down. A report while it holds both names
the latch's obligation, the higher of the two.

The main thread then waits on the latch while holding `summary_mutex`:

```cpp
{
  std::lock_guard g(summary_mutex);
  loaded.wait();  // the loaders need summary_mutex to finish
}
```

Any loader that hasn't finished is stuck on `summary_mutex`, so the latch
would never open. **lock_align** refuses the wait: the latch is at level 2,
and the main thread holds a level-3 mutex. The example catches the error,
releases the mutex, and waits again.

## Part 2: two promises

Two services each owe the other an answer through an
[`ordered_promise`](../reference/ordered_promise/ordered_promise.md). Each
takes its own promise's obligation, and first tries to wait for the other's
answer:

```cpp
void service(const std::string& name, ordered_promise<int>& mine, ordered_future<int>& theirs,
             int answer) {
  {
    obligation duty = mine.take_obligation();
    try {
      theirs.wait();  // the other service is waiting for our answer
    } catch (const lock_order_error& e) {
      say(name + ": " + e.what());
    }
    mine.set_value(answer);
  }
  say(name + " heard " + std::to_string(theirs.get()));
}
```

If both waited, neither would ever answer. Both promises are at level 2.
A thread holding an obligation at level 2 may only wait on obligations
above level 2, so both waits are refused. Each service then answers,
releases its obligation, and waits again. That works.

## Output

The order of the lines from part 2 can vary.

```
lock order violation: blocking on obligation 'loaded' (level 2) while holding mutex 'summary' (level 3)
summary: 60
left: lock order violation: blocking on obligation 'right's answer' (level 2) while holding obligation 'left's answer' (level 2)
right: lock order violation: blocking on obligation 'left's answer' (level 2) while holding obligation 'right's answer' (level 2)
right heard 1
left heard 2
```

## Building and running

From the top of the repository, with CMake:

```sh
cmake -S . -B build
cmake --build build --target waiting_on_threads
./build/examples/waiting_on_threads
```

Or with one compiler command (add `-pthread` on Linux):

```sh
c++ -std=c++20 -Iinclude examples/waiting_on_threads.cpp -o waiting_on_threads
./waiting_on_threads
```

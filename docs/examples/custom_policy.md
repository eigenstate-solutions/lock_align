# Custom policy

Source: [`examples/custom_policy.cpp`](../../examples/custom_policy.cpp)

The default policy throws at the first violation. In a test suite, you may
prefer to see every violation from one run. This example writes a policy
that records each violation, lets the lock go ahead, and reports them all
at the end.

## The policy

```cpp
struct record_violations {
  // A plain std::mutex: the policy must not take an ordered mutex, because it
  // runs in the middle of an ordering check.
  static inline std::mutex mutex;
  static inline std::vector<std::string> found;

  static void on_violation(const violation& v) {
    std::lock_guard g(mutex);
    found.push_back(v.message());
  }
};
```

A policy is any type with a static `on_violation` that accepts each kind of
violation, and one taking `const violation&` does; see
[policy requirements](../reference/violation/README.md#policy-requirements).
Because this one returns, the lock goes ahead after it's recorded.

## Using it

The policy is the last template parameter of each **lock_align** class.
Aliases keep that out of the rest of the code:

```cpp
template <auto Level>
using checked_mutex = ordered_mutex<std::mutex, Level, record_violations>;
using checked_thread = ordered_thread<record_violations>;
```

To use a policy everywhere instead, define
[`LOCK_ALIGN_DEFAULT_POLICY`](../reference/config/README.md#lock_align_default_policy).

## The violation

One thread locks `accounts` (level 1) and then `audit log` (level 2).
Another locks them the other way round. The threads run one after the
other, so this run can't deadlock. Run at the same time, they could. The
second thread's lock of `accounts` is recorded, and goes ahead.

## Output

```
1 violation(s)
  lock order violation: blocking on mutex 'accounts' (level 1) while holding mutex 'audit log' (level 2)
```

The program exits with status 1 when it found a violation, so a test
runner treats it as a failure. In the project's own test run, this example
is marked as expected to fail.

## Building and running

From the top of the repository, with CMake:

```sh
cmake -S . -B build
cmake --build build --target custom_policy
./build/examples/custom_policy
```

Or with one compiler command (add `-pthread` on Linux):

```sh
c++ -std=c++20 -Iinclude examples/custom_policy.cpp -o custom_policy
./custom_policy
```

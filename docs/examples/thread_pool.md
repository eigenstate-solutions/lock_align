# Thread pool

Source: [`examples/thread_pool.cpp`](../../examples/thread_pool.cpp)

A small thread pool built from **lock_align**'s pieces:

- an [`ordered_thread`](../reference/ordered_thread/ordered_thread.md)
  for each worker;
- an [`ordered_mutex`](../reference/ordered_mutex/ordered_mutex.md) and an
  [`ordered_condition_variable`](../reference/ordered_condition_variable/ordered_condition_variable.md)
  for the job queue;
- a [`task_scope`](../reference/task_scope/task_scope.md) around each job.

The main thread holds a `config` lock for as long as the pool runs. One
job tries to take that lock.

## The levels

```cpp
enum class level {
  config = 1,     // may be held while the pool shuts down
  workers = 2,    // the pool's threads; tasks may only lock above this
  job_ready = 3,  // the condition variable workers wait on, below its mutex
  results = 4,    // shared by tasks
  queue = 5,      // the pool's own queue
  output = 6,     // printing, always taken last
};
```

The level of the workers is the key choice. Each worker holds an
obligation at `level::workers` for as long as it runs. That has two
effects:

- Shutting the pool down joins the workers, and that's checked as a wait at
  `level::workers`. The thread doing it may hold `config`, but nothing above
  `level::workers`.
- Every job runs under the worker's obligation, so a job may only lock
  above `level::workers`. It can't take `config`, which whoever shuts the
  pool down may be holding.

The queue is at `level::queue`, above the workers, so a worker can lock it
to fetch its next job.

Workers wait for jobs on a condition variable at `level::job_ready`. Its
level has two limits:

- It must be above `level::workers`. A worker waits while holding its own
  obligation, and an obligation only allows waiting on something higher.
- It must be below `level::queue`, the level of the mutex it's used with. A
  thread that notifies may hold the condition variable's obligation, and it
  still has to lock the queue. The compiler checks this one.

A worker waiting for a job may hold nothing above `level::job_ready` except
the queue mutex, which the wait releases. So it can't sit waiting while
holding something a submitter needs before it can notify.

## Notifying

A thread that notifies `ready_` has to hold its obligation, or the notify
is reported as a missing obligation:

```cpp
void submit(std::string name, std::function<void()> work) {
  obligation duty = ready_.take_obligation();
  {
    std::lock_guard g(mutex_);
    jobs_.push_back({std::move(name), std::move(work)});
  }
  ready_.notify_one();
}

~thread_pool() {
  {
    obligation duty = ready_.take_obligation();
    {
      std::lock_guard g(mutex_);
      stopping_ = true;
    }
    ready_.notify_all();
  }
  for (ordered_thread<>& w : workers_) w.join();
}
```

`submit` takes it first, so everything it blocks on before notifying is
checked against `level::job_ready`. In the destructor the obligation ends
before the joins: a thread owing work at `level::job_ready` can't wait for
the workers at `level::workers`, below it.

## The worker loop

```cpp
void run() {
  for (;;) {
    job next;
    {
      std::unique_lock lk(mutex_);
      ready_.wait(lk, [this] { return stopping_ || !jobs_.empty(); });
      if (jobs_.empty()) return;
      next = std::move(jobs_.front());
      jobs_.pop_front();
    }
    try {
      task_scope task(next.name);
      next.work();
    } catch (const violation_error& e) {
      say("task '" + next.name + "' failed: " + e.what());
    }
  }
}
```

The queue lock is released before the job runs. The `task_scope` checks
that: it reports a worker that starts a job while holding a mutex, and a
job that ends without releasing something it took.

## The deadlock it prevents

```cpp
std::lock_guard hold_config(config);
{
  thread_pool pool(2);
  // ... four jobs that add to a total under the results lock ...
  pool.submit("read config", [&] { std::lock_guard g(config); });
}  // ~thread_pool joins the workers while config is still held
```

Without **lock_align**, the `read config` job would block forever on `config`.
The main thread would block forever in `~thread_pool` waiting for that
worker. With **lock_align**, the job's lock is refused before it blocks. The
worker reports it and moves on, and the pool shuts down normally.

## Output

The worker number depends on which worker picks up the job.

```
task 'read config' failed: lock order violation: blocking on mutex 'config' (level 1) while holding obligation 'worker 1' (level 2)
total: 10
```

## Notes

- `~thread_pool` joins the workers. If a join were a violation,
  `throw_on_violation` would throw from a destructor and end the program.
  A pool that might be shut down from anywhere could use
  `ordered_thread<abort_on_violation>` to make that explicit.
- It's sample code, not a production pool. The comment at the top of the
  source lists what it leaves out: the constructor isn't exception-safe, a
  task that throws something other than a `violation_error` ends the
  program, and `submit` accepts jobs after shutdown has begun, which may
  then never run.

## Building and running

From the top of the repository, with CMake:

```sh
cmake -S . -B build
cmake --build build --target thread_pool
./build/examples/thread_pool
```

Or with one compiler command (add `-pthread` on Linux):

```sh
c++ -std=c++20 -Iinclude examples/thread_pool.cpp -o thread_pool
./thread_pool
```

// A small thread pool built from ordered_thread, ordered_mutex,
// ordered_condition_variable and task_scope.
//
// The main thread holds a config lock while it shuts the pool down, so it is
// waiting for the workers while holding that lock. A task that tries to take
// the same lock would deadlock the shutdown; lock_align refuses it instead.
//
// This is sample code, kept short to show lock_align at work. Don't copy it
// as a production thread pool. Among other things:
//
//   - The constructor isn't exception-safe. If starting a worker throws
//     (std::system_error, or std::bad_alloc), the workers already started are
//     still joinable when workers_ is destroyed, and destroying a joinable
//     ordered_thread calls std::terminate. A real pool would stop and join
//     the started workers before rethrowing.
//   - The destructor joins the workers, and with throw_on_violation a join
//     that breaks the lock order throws. Destructors are noexcept, so that
//     ends the program: shutting the pool down while holding a lock above
//     level::workers terminates rather than reporting. A real pool would
//     offer an explicit shutdown() that can throw, and only join in the
//     destructor as a fallback.
//   - A task that throws anything other than a violation_error escapes the
//     worker thread, which also calls std::terminate.
//   - submit() doesn't check whether the pool is stopping. A job submitted
//     once shutdown has begun runs only if a worker hasn't exited yet;
//     otherwise it is silently dropped. A real pool would refuse it, by
//     throwing or returning false.

#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "lock_align/obligation.hpp"
#include "lock_align/ordered_condition_variable.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_thread.hpp"
#include "lock_align/task_scope.hpp"
#include "lock_align/violation.hpp"

using namespace lock_align;

enum class level {
  config = 1,     // may be held while the pool shuts down
  workers = 2,    // the pool's threads; tasks may only lock above this
  job_ready = 3,  // the condition variable workers wait on, below its mutex
  results = 4,    // shared by tasks
  queue = 5,      // the pool's own queue
  output = 6,     // printing, always taken last
};

ordered_mutex<std::mutex, level::output> output_mutex{"output"};

void say(const std::string& line) {
  std::lock_guard g(output_mutex);
  std::puts(line.c_str());
}

class thread_pool {
 public:
  explicit thread_pool(int threads) {
    for (int i = 0; i < threads; ++i)
      workers_.emplace_back(level::workers, "worker " + std::to_string(i), [this] { run(); });
  }

  // Joining a worker is checked like locking a level::workers mutex: the
  // caller may hold locks at that level or below, never above.
  ~thread_pool() {
    {
      // Notifying needs ready_'s obligation. It ends before the joins: a
      // thread owing work at job_ready can't wait for the workers below it.
      obligation duty = ready_.take_obligation();
      {
        std::lock_guard g(mutex_);
        stopping_ = true;
      }
      ready_.notify_all();
    }
    for (ordered_thread<>& w : workers_) w.join();
  }

  void submit(std::string name, std::function<void()> work) {
    obligation duty = ready_.take_obligation();
    {
      std::lock_guard g(mutex_);
      jobs_.push_back({std::move(name), std::move(work)});
    }
    ready_.notify_one();
  }

 private:
  struct job {
    std::string name;
    std::function<void()> work;
  };

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

  ordered_mutex<std::mutex, level::queue> mutex_{"queue"};
  ordered_condition_variable<level::job_ready> ready_{"job ready"};
  std::deque<job> jobs_;
  bool stopping_ = false;
  std::vector<ordered_thread<>> workers_;
};

int main() {
  ordered_mutex<std::mutex, level::config> config{"config"};
  ordered_mutex<std::mutex, level::results> results{"results"};
  int total = 0;

  std::lock_guard hold_config(config);
  {
    thread_pool pool(2);
    for (int i = 1; i <= 4; ++i) {
      pool.submit("add " + std::to_string(i), [&, i] {
        std::lock_guard g(results);
        total += i;
      });
    }
    // The main thread holds config until the pool has shut down, so this
    // task could never get it.
    pool.submit("read config", [&] { std::lock_guard g(config); });
  }  // ~thread_pool joins the workers while config is still held

  say("total: " + std::to_string(total));
}

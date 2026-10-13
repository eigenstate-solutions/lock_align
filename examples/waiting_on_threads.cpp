// Waiting for other threads, checked like locking.
//
// Part 1: loader threads count down a latch, and they need the summary lock
// to do it. Waiting on the latch while holding the summary lock would
// deadlock, so that wait is refused.
//
// Part 2: two services each owe the other an answer through a promise. If
// each waited for the other before answering, neither would ever answer.
// Both waits are refused; each service answers first and then waits.

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "lock_align/obligation.hpp"
#include "lock_align/ordered_latch.hpp"
#include "lock_align/ordered_mutex.hpp"
#include "lock_align/ordered_promise.hpp"
#include "lock_align/ordered_thread.hpp"
#include "lock_align/violation.hpp"

using namespace lock_align;

enum class level {
  threads = 1,  // every thread in this program
  loaded = 2,   // the latch, and both services' promises
  summary = 3,  // what the loaders write
  output = 4,   // printing, always taken last
};

ordered_mutex<std::mutex, level::output> output_mutex{"output"};

void say(const std::string& line) {
  std::lock_guard g(output_mutex);
  std::puts(line.c_str());
}

void load_in_parallel() {
  constexpr int loaders = 3;
  ordered_latch loaded(loaders, level::loaded, "loaded");
  ordered_mutex<std::mutex, level::summary> summary_mutex{"summary"};
  int summary = 0;

  std::vector<ordered_thread<>> threads;
  for (int i = 1; i <= loaders; ++i) {
    threads.emplace_back(level::threads, "loader", [&, i] {
      obligation duty = loaded.take_obligation();
      {
        std::lock_guard g(summary_mutex);
        summary += i * 10;
      }
      loaded.count_down();
    });
  }

  {
    std::lock_guard g(summary_mutex);
    try {
      loaded.wait();  // the loaders need summary_mutex to finish
    } catch (const lock_order_error& e) {
      say(e.what());
    }
  }
  loaded.wait();  // holding nothing now
  {
    std::lock_guard g(summary_mutex);
    say("summary: " + std::to_string(summary));
  }
  for (ordered_thread<>& t : threads) t.join();
}

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

void exchange_answers() {
  ordered_promise<int> to_left(level::loaded, "left's answer");
  ordered_promise<int> to_right(level::loaded, "right's answer");
  ordered_future<int> from_left = to_left.get_future();
  ordered_future<int> from_right = to_right.get_future();

  ordered_thread left(level::threads, "left", [&] { service("left", to_left, from_right, 1); });
  ordered_thread right(level::threads, "right", [&] { service("right", to_right, from_left, 2); });
  left.join();
  right.join();
}

int main() {
  load_in_parallel();
  exchange_answers();
}

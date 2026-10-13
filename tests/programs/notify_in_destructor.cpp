// A destructor that notifies without the obligation: under the throwing
// policy the exception leaves a destructor, so the program terminates.

#include <cstdio>
#include <cstdlib>
#include <exception>

#include "lock_align/ordered_condition_variable.hpp"

// Prints the exception that ended the program, as GCC's and Clang's runtimes
// do by themselves and MSVC's doesn't.
[[noreturn]] void report_and_abort() noexcept {
  if (std::exception_ptr e = std::current_exception()) {
    try {
      std::rethrow_exception(e);
    } catch (const std::exception& x) {
      std::fprintf(stderr, "terminated by: %s\n", x.what());
    } catch (...) {
    }
  }
  std::abort();
}

struct queue {
  lock_align::ordered_condition_variable<1> ready{"ready"};
  ~queue() { ready.notify_all(); }  // no obligation held
};

int main() {
  std::set_terminate(report_and_abort);
  queue q;
}

#pragma once

// Checks after every test case that the main thread holds nothing. A test
// that fails partway can leave a mutex or obligation in the thread's record,
// and every later test in the same run would then fail for no reason of its
// own. A listener can't fail a test that has already ended, so this reports
// the test and stops the run.
//
// Include it in each test file, after the lock_align headers, so that it uses
// that file's configuration. Each test program is one file, so it is
// registered once.

#include <catch2/catch_test_case_info.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <cstdio>
#include <cstdlib>

#include "lock_align/detail/held_stack.hpp"
#include "lock_align/held.hpp"

namespace lock_align_test {

class held_listener : public Catch::EventListenerBase {
 public:
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseEnded(const Catch::TestCaseStats& stats) override {
    if (lock_align::held_count() == 0) return;
    std::fprintf(stderr, "test '%s' ended with %zu entries still held:\n",
                 stats.testInfo->name.c_str(), lock_align::held_count());
    // Only levels and kinds: the names belong to objects that may be gone.
    for (const lock_align::detail::held_entry& e : lock_align::detail::held.entries())
      std::fprintf(stderr, "  %s at level %lu\n",
                   lock_align::detail::is_obligation(e.rank) ? "obligation" : "mutex",
                   static_cast<unsigned long>(lock_align::detail::level_of(e.rank)));
    std::fprintf(stderr, "Stopping: every later test would see them too.\n");
    std::fflush(stderr);
    std::exit(EXIT_FAILURE);
  }
};

}  // namespace lock_align_test

CATCH_REGISTER_LISTENER(lock_align_test::held_listener)

#pragma once

// Helpers shared by the lock_align tests.

#include <atomic>
#include <source_location>
#include <string>

#include "lock_align/violation.hpp"

namespace lock_align_test {

// Catch2 assertions aren't thread-safe, so other threads use this and report
// back through atomics.
template <class F>
bool throws_order_error(F&& f) {
  try {
    f();
  } catch (const lock_align::lock_order_error&) {
    return true;
  }
  return false;
}

// The default name of something made at where, as the docs describe it:
// "file:line", with the file as the compiler was given it. Built here rather
// than with the library's own helper, so the tests check the format.
inline std::string location_name(const std::source_location& where) {
  return std::string(where.file_name()) + ":" + std::to_string(where.line());
}

// A policy that counts violations and lets the operation go ahead. Each test
// names its own instantiation with a tag declared on the spot,
//   using policy = counting_policy<struct tag>;
// which is a different type in every test, so each count starts at zero and
// no other test can change it, whatever order the tests run in.
template <class Tag>
struct counting_policy {
  static inline std::atomic<int> violations{0};
  static void on_violation(const lock_align::violation&) noexcept { ++violations; }
};

}  // namespace lock_align_test

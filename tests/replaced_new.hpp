#pragma once

// Replaces the global operator new and delete, so a test can count
// allocations or make the next one fail. Include it in one test file per
// program at most.
//
// Sanitizers bring their own allocator, which these would clash with: on
// Linux, Clang's static TSan runtime defines the same operators, and ASan
// reports memory that its own nothrow new allocated and these freed. So with
// a sanitizer (LOCK_ALIGN_TEST_SANITIZED) nothing is replaced, and the tests
// that need it skip themselves:
//   if (!replaced_new) SKIP(no_replaced_new);

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace lock_align_test {

#ifdef LOCK_ALIGN_TEST_SANITIZED
inline constexpr bool replaced_new = false;
#else
inline constexpr bool replaced_new = true;
#endif
inline constexpr const char* no_replaced_new = "a sanitizer replaces operator new itself";

// Every allocation through operator new or new[] in this program.
inline std::atomic<long> allocations{0};
// Makes the next allocation throw std::bad_alloc.
inline std::atomic<bool> fail_next_allocation{false};

}  // namespace lock_align_test

#ifndef LOCK_ALIGN_TEST_SANITIZED

// GCC sees free() called on memory from operator new, not knowing that this
// operator new is malloc.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

void* operator new(std::size_t size) {
  ++lock_align_test::allocations;
  if (lock_align_test::fail_next_allocation.exchange(false)) throw std::bad_alloc();
  if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
// The library's record grows with new[], and some standard libraries don't
// route the array forms through the ones above, so these are replaced too.
void* operator new[](std::size_t size) { return operator new(size); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif

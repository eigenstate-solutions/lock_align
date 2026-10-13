#pragma once

// Compile-time configuration. Define these before including any lock_align
// header, and the same way in every file of a program (the lock_align CMake
// options do that for you):
//   LOCK_ALIGN_DISABLE        turn every check off; names aren't built or
//                             stored, so wrappers keep at most a level
//   LOCK_ALIGN_HELD_SLOTS_RESERVED
//                             entries each thread can hold before its record
//                             moves to the heap (default 32); there is no
//                             limit. Must be a plain integer literal
//   LOCK_ALIGN_DEFAULT_POLICY violation policy used when none is given
//                             (default ::lock_align::throw_on_violation)
//   LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK
//                             required or unchecked: whether notifying,
//                             counting down or setting a value needs the
//                             thread to hold the obligation (default required)
//   LOCK_ALIGN_NO_CONFIG_CHECK
//                             don't check at startup that every file was
//                             built with the same configuration
//   LOCK_ALIGN_ALLOW_MIXED_DEFAULTS
//                             check only the macros that change layouts, not
//                             the two defaults
//
// Everything is declared in an inline namespace inside lock_align whose name
// encodes the macros that change layouts, such as lock_align::v1_on_s32.
// Files built with different layouts then get different types instead of
// silently sharing one. Write lock_align::ordered_mutex as usual; don't name
// the inline namespace or declare anything in lock_align yourself.

#include <cstddef>

#ifndef LOCK_ALIGN_HELD_SLOTS_RESERVED
#define LOCK_ALIGN_HELD_SLOTS_RESERVED 32
#endif

#ifndef LOCK_ALIGN_DEFAULT_POLICY
#define LOCK_ALIGN_DEFAULT_POLICY ::lock_align::throw_on_violation
#endif

#ifndef LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK
#define LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK required
#endif

// Whether checks are compiled in, fixed when this header is first included.
// The other headers test this rather than LOCK_ALIGN_DISABLE, and
// detail/config_guard.hpp reports a LOCK_ALIGN_DISABLE that changes later.
#ifdef LOCK_ALIGN_DISABLE
#define LOCK_ALIGN_DETAIL_ENABLED 0
#else
#define LOCK_ALIGN_DETAIL_ENABLED 1
#endif

// The inline namespace: v<layout version>_<on|off>_s<slots>. Bump the version
// whenever a release changes a layout. The slot count is pasted into the
// name, so it must be a plain integer literal.
#if LOCK_ALIGN_DETAIL_ENABLED
#define LOCK_ALIGN_DETAIL_ABI_NAME(slots) v1_on_s##slots
#else
#define LOCK_ALIGN_DETAIL_ABI_NAME(slots) v1_off_s##slots
#endif
#define LOCK_ALIGN_DETAIL_ABI_EXPAND(slots) LOCK_ALIGN_DETAIL_ABI_NAME(slots)
#define LOCK_ALIGN_DETAIL_ABI LOCK_ALIGN_DETAIL_ABI_EXPAND(LOCK_ALIGN_HELD_SLOTS_RESERVED)

#define LOCK_ALIGN_DETAIL_STR_(x) #x
#define LOCK_ALIGN_DETAIL_STR(x) LOCK_ALIGN_DETAIL_STR_(x)

// [[no_unique_address]], as each compiler honors it. MSVC ignores the
// standard spelling, to keep its existing ABI, and honors only its own: cl
// from VS 2019 16.10 (_MSC_VER 1929), although __has_cpp_attribute only
// reports it from VS 2022 17.1, so cl is tested by version. clang-cl warns
// about the standard spelling and honors MSVC's where __has_cpp_attribute
// says so; its _MSC_VER describes the MSVC it imitates, not itself. Anything
// else, including older MSVC, gets the standard spelling.
#if defined(_MSC_VER) && !defined(__clang__) && _MSC_VER >= 1929
#define LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif defined(_MSC_VER) && __has_cpp_attribute(msvc::no_unique_address)
#define LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

// Brackets each header's code. Once the optimizer inlines a policy that
// always throws, MSVC warns that the code after the call is unreachable
// (C4702), in the header, at the warning level many projects build with. MSVC
// applies the warning state in effect where a function is defined, so turning
// it off around the header's own code is enough, and the pop leaves the
// including file's state as it was.
#ifdef _MSC_VER
#define LOCK_ALIGN_DETAIL_SUPPRESS_WARNINGS __pragma(warning(push)) __pragma(warning(disable : 4702))
#define LOCK_ALIGN_DETAIL_RESTORE_WARNINGS __pragma(warning(pop))
#else
#define LOCK_ALIGN_DETAIL_SUPPRESS_WARNINGS
#define LOCK_ALIGN_DETAIL_RESTORE_WARNINGS
#endif

// MSVC's linker reports object files built with different layouts, unless
// the file opts out of the configuration checks.
#if defined(_MSC_VER) && !defined(LOCK_ALIGN_NO_CONFIG_CHECK)
#pragma detect_mismatch("lock_align_layout", LOCK_ALIGN_DETAIL_STR(LOCK_ALIGN_DETAIL_ABI))
#endif

// The configuration this file started with, for detail/config_guard.hpp.
// Internal linkage, so each file has its own.
namespace lock_align::config_detail {
static constexpr std::size_t first_slots = LOCK_ALIGN_HELD_SLOTS_RESERVED;
}  // namespace lock_align::config_detail

// An error on the next line ("pasting formed an invalid token") means
// LOCK_ALIGN_HELD_SLOTS_RESERVED isn't a plain integer literal, such as 64.
namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

inline constexpr bool enabled = LOCK_ALIGN_DETAIL_ENABLED;

// Entries (mutexes plus obligations) each thread can hold without allocating.
inline constexpr std::size_t held_slots_reserved = LOCK_ALIGN_HELD_SLOTS_RESERVED;

}  // namespace lock_align

#include "lock_align/detail/config_guard.hpp"

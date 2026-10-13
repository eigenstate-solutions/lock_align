#pragma once

// The startup check that every file of a program was built with the same
// configuration. Each file that includes lock_align registers its own
// configuration during static initialization; the first registration is
// kept, and any later one that differs prints both and aborts.
//
// The inline namespace already keeps files with different layouts from
// sharing lock_align's own code. It can't stop a type of yours that contains
// a lock_align type from having two layouts, or two files from keeping
// separate records of what each thread holds. This catches both.
//
// The registry is outside the inline namespace, so that every configuration
// shares it, and nothing in it depends on the configuration.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"

namespace lock_align::config_detail {

// T's name as the compiler spells it, so that two spellings of one type
// compare equal.
template <class T>
constexpr std::string_view type_name() noexcept {
#if defined(__clang__) || defined(__GNUC__)
  constexpr std::string_view f = __PRETTY_FUNCTION__;  // "... [T = name]" or "[with T = name; ...]"
  constexpr std::size_t start = f.find("T = ") + 4;
  return f.substr(start, f.find_first_of(";]", start) - start);
#elif defined(_MSC_VER)
  constexpr std::string_view f = __FUNCSIG__;  // "... type_name<name>(void) noexcept"
  constexpr std::size_t start = f.find("type_name<") + 10;
  return f.substr(start, f.rfind(">(void)") - start);
#else
  return "(unknown)";
#endif
}

struct configuration {
  std::string_view layout;  // the inline namespace's name
  bool enabled;
  std::size_t slots;
  std::string_view policy;
  std::string_view obligation_check;
  bool compare_defaults;  // false with LOCK_ALIGN_ALLOW_MIXED_DEFAULTS
  const char* file;
};

inline void print(const char* file, const std::string& layout_and_defaults) {
  std::fprintf(stderr, "  %s: %s\n", file, layout_and_defaults.c_str());
}

inline std::string describe(const configuration& c) {
  return std::string(c.enabled ? "checks on" : "checks off") + ", " + std::to_string(c.slots) +
         " held slots, default policy " + std::string(c.policy) + ", obligations " +
         std::string(c.obligation_check);
}

// Registers one file's configuration. The first is kept; a later one that
// differs is reported, and the program aborts.
inline void register_configuration(const configuration& c) noexcept {
  static std::mutex guard;
  static bool registered = false;
  static std::string layout, policy, obligation_check, file, description;
  static bool compare_defaults = true;
  try {
    std::lock_guard g(guard);
    if (!registered) {
      layout = c.layout;
      policy = c.policy;
      obligation_check = c.obligation_check;
      file = c.file;
      description = describe(c);
      compare_defaults = c.compare_defaults;
      registered = true;
      return;
    }
    const bool same_layout = layout == c.layout;
    const bool same_defaults = !(compare_defaults && c.compare_defaults) ||
                               (policy == c.policy && obligation_check == c.obligation_check);
    if (same_layout && same_defaults) return;
    std::fprintf(stderr, "lock_align: configuration mismatch between translation units:\n");
    print(file.c_str(), description);
    print(c.file, describe(c));
    std::fprintf(stderr,
                 "Define the configuration macros once for the whole program, for example "
                 "with the lock_align CMake options.\n");
  } catch (...) {
    return;  // out of memory during static initialization: skip the check
  }
  std::abort();
}

}  // namespace lock_align::config_detail

#pragma once

// How resources are named in violation reports when the caller gives no name.

#include <source_location>
#include <string>
#include <string_view>

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"

LOCK_ALIGN_DETAIL_SUPPRESS_WARNINGS

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI::detail {

inline std::string location_name(const std::source_location& where) {
  return std::string(where.file_name()) + ":" + std::to_string(where.line());
}

// The name of a default-constructed object, which has no call site to name it.
inline constexpr const char* unnamed = "(unnamed)";

// A resource's name, as wrappers store it. A name given by the caller is
// copied; otherwise only the source location is kept, which points to static
// storage, and "file:line" is built only when a name is asked for, such as
// when a violation is reported. So a resource with a default name doesn't
// allocate.
//
// With LOCK_ALIGN_DISABLE nothing is ever reported, so it stores nothing,
// takes no space with LOCK_ALIGN_DETAIL_NO_UNIQUE_ADDRESS, and reads as "".
#if !LOCK_ALIGN_DETAIL_ENABLED
class resource_name {
 public:
  resource_name() noexcept = default;
  resource_name(std::string_view, const std::source_location&) noexcept {}
  std::string str() const { return {}; }
};
#else
class resource_name {
 public:
  // "(unnamed)": a default-constructed object has no call site.
  resource_name() noexcept = default;

  // name, or the file:line of where if name is empty.
  resource_name(std::string_view name, const std::source_location& where)
      : given_(name), where_(where) {}

  std::string str() const {
    if (!given_.empty()) return given_;
    // A default-constructed source_location, with line 0, is no location.
    return where_.line() != 0 ? location_name(where_) : std::string(unnamed);
  }

 private:
  std::string given_;  // empty, and so not allocated, for a default name
  std::source_location where_;
};
#endif

// Lets one wrapper check against another's name, as a condition variable
// does with its mutex's. Each wrapper with a name befriends it.
struct name_access {
  template <class T>
  static const resource_name& of(const T& t) noexcept {
    return t.name_;
  }
};

}  // namespace lock_align::detail

LOCK_ALIGN_DETAIL_RESTORE_WARNINGS

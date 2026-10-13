#pragma once

// Declarations of the violation classes, for a policy of your own that must
// be declared before LOCK_ALIGN_DEFAULT_POLICY is used. Include this rather
// than declaring them yourself: they live in an inline namespace inside
// lock_align (see config.hpp).

#include "lock_align/config.hpp"
#include "lock_align/detail/config_guard.hpp"

namespace lock_align::inline LOCK_ALIGN_DETAIL_ABI {

class violation;
class lock_order_violation;
class task_violation;
class task_entry_violation;
class task_exit_violation;
class missing_obligation_violation;

}  // namespace lock_align

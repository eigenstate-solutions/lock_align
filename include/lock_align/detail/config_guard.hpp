// No include guard: every lock_align header includes this, so that a
// configuration macro changed between two lock_align includes in one file is
// an error rather than two halves of the library disagreeing.

#include "lock_align/config.hpp"

#if defined(LOCK_ALIGN_DISABLE) != !LOCK_ALIGN_DETAIL_ENABLED
#error "lock_align: LOCK_ALIGN_DISABLE changed after the first lock_align header was included"
#endif

static_assert(::lock_align::config_detail::first_slots == LOCK_ALIGN_HELD_SLOTS_RESERVED,
              "lock_align: LOCK_ALIGN_HELD_SLOTS_RESERVED changed after the first lock_align "
              "header was included");

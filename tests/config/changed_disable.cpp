// LOCK_ALIGN_DISABLE defined between two lock_align includes: an error.

#include "lock_align/ordered_mutex.hpp"
#define LOCK_ALIGN_DISABLE
#include "lock_align/task_scope.hpp"

int main() {}

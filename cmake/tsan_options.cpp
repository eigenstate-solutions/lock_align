// Linked into lock_align's own test and example programs when they are built
// with ThreadSanitizer. lock_align checks lock order itself, and the tests and
// examples break it on purpose; TSan's own deadlock detector would report
// those breaks too, and it can't track more than 64 locks held by one thread
// (on Linux it then fails an internal check and hangs). Its race detection is
// unaffected. A TSAN_OPTIONS set in the environment still overrides this.

extern "C" const char* __tsan_default_options() { return "detect_deadlocks=0"; }

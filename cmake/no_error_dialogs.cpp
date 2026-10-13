// Linked into lock_align's own test and example programs on MSVC. Its debug
// runtime reports abort() and failed assertions in a dialog box, which waits
// for someone to close it and so hangs a run with no one there, such as CI.
// This sends those reports to stderr and leaves abort() silent, as on other
// platforms. init_seg(lib) runs it before ordinary static initializers, one
// of which may abort: lock_align's startup configuration check.

#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>

#pragma warning(disable : 4073)  // init_seg(lib) is meant for libraries
#pragma init_seg(lib)

namespace {

struct no_error_dialogs {
  no_error_dialogs() noexcept {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  }
};

const no_error_dialogs instance;

}  // namespace
#endif

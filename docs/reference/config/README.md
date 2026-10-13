# `<lock_align/config.hpp>`

Compile-time settings. Every other **lock_align** header includes this one.

The macros must be defined before the first **lock_align** header is included,
and must have the same value in every file of a program; see
[One configuration per program](#one-configuration-per-program). The
simplest way is the [CMake options](#cmake-options), which set them for
every file that links `lock_align::lock_align`.

## Macros

| Macro | Default | Effect |
|---|---|---|
| [`LOCK_ALIGN_DISABLE`](#lock_align_disable) | not defined | Turns every check off |
| [`LOCK_ALIGN_HELD_SLOTS_RESERVED`](#lock_align_held_slots_reserved) | `32` | Entries each thread can hold before its record moves to the heap |
| [`LOCK_ALIGN_DEFAULT_POLICY`](#lock_align_default_policy) | `::lock_align::throw_on_violation` | The policy used when none is given |
| [`LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK`](#lock_align_default_obligation_check) | `required` | Whether signalling needs the obligation, when not given |
| [`LOCK_ALIGN_NO_CONFIG_CHECK`](#lock_align_no_config_check) | not defined | Skips the startup check that every file agrees |
| [`LOCK_ALIGN_ALLOW_MIXED_DEFAULTS`](#lock_align_allow_mixed_defaults) | not defined | Lets files differ in the two default macros |

## Constants

| Constant | Value |
|---|---|
| [`enabled`](#enabled) | `false` if `LOCK_ALIGN_DISABLE` is defined, otherwise `true` |
| [`held_slots_reserved`](#held_slots_reserved) | `LOCK_ALIGN_HELD_SLOTS_RESERVED` |

---

### LOCK_ALIGN_DISABLE

```cpp
#define LOCK_ALIGN_DISABLE
```

When defined, nothing is checked or recorded:

- Names are neither built nor stored, and every `name()` returns `""`.
- An `ordered_mutex` is the same size as the mutex it wraps, and an
  `ordered_condition_variable` the same size as a
  `std::condition_variable_any`.
- Joins and waits on ordered threads, futures, latches and condition
  variables aren't checked. Threads, promises, futures and latches still
  store their level, so `level()` keeps working.
- Notifying, counting down and setting values aren't checked for an
  obligation, and a condition variable's `notify_one` and `notify_all` are
  `noexcept`.
- Obligations and task scopes do nothing. An obligation stores only its
  level, and a task scope stores nothing.
- `current_level()` and `held_count()` always return 0.

The sizes rely on `[[no_unique_address]]`. MSVC honors only its own
spelling, `[[msvc::no_unique_address]]`, which **lock_align** uses from
Visual Studio 2019 16.10 on. With older MSVC the wrappers still work, but
each is a few bytes bigger than what it wraps.

Code that uses **lock_align** compiles the same way either way.

### LOCK_ALIGN_HELD_SLOTS_RESERVED

```cpp
#define LOCK_ALIGN_HELD_SLOTS_RESERVED 32
```

How many mutexes and obligations each thread can hold before its record
of them moves to the heap. Each lock of a recursive mutex counts
separately. There's no limit: past this many, the record grows as needed.

Each thread's record starts as an array of this many entries, so holding
up to this many never allocates. Beyond that, the entries move to heap
storage that doubles as it fills, and that storage is freed when the
thread holds nothing again. A thread that exits while still holding more
than this many leaks that storage, but exiting while holding a lock is
already a bug.

Room for each new entry is made before the mutex is locked or the
obligation is taken, so if the allocation fails, `std::bad_alloc` is
thrown and nothing is locked.

Raise it if your threads routinely hold more, to avoid the allocation.
Each entry is 24 bytes on a 64-bit platform.

It must be a plain integer literal, such as `64`, because it becomes part
of the name of the [inline namespace](#one-configuration-per-program). An
expression such as `(2 * 32)` doesn't compile: the error is "pasting
formed an invalid token", in `config.hpp`.

### LOCK_ALIGN_DEFAULT_POLICY

```cpp
#define LOCK_ALIGN_DEFAULT_POLICY ::lock_align::throw_on_violation
```

The violation policy used by every **lock_align** template whose policy
parameter is left out. It becomes
[`default_policy`](../violation/README.md#default_policy).

The macro is expanded inside `violation.hpp`, so the type it names must be
declared before then. Declare a policy of your own after including
`<lock_align/violation_fwd.hpp>`, which declares `violation` and its
derived classes, and before any other **lock_align** header:

```cpp
#define LOCK_ALIGN_DEFAULT_POLICY ::my::policy
#include "lock_align/violation_fwd.hpp"
namespace my { struct policy { static void on_violation(const lock_align::violation&); }; }
#include "lock_align/ordered_mutex.hpp"
```

Don't declare `violation` yourself with `namespace lock_align { class
violation; }`. The real one is in an inline namespace inside `lock_align`,
so that declares a different class, and using the name becomes ambiguous.

### LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK

```cpp
#define LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK required
```

`required` or `unchecked`: the
[`obligation_check`](../obligation/README.md#obligation_check) used by
condition variables, latches and promises whose parameter is left out. It
becomes [`default_obligation_check`](../obligation/README.md#default_obligation_check).

Define it as `unchecked` to let threads notify, count down and set values
without an obligation unless an object asks for the check:

```cpp
#define LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK unchecked
#include "lock_align/ordered_condition_variable.hpp"
```

Like the other macros, define it the same way in every translation unit.

### LOCK_ALIGN_NO_CONFIG_CHECK

```cpp
#define LOCK_ALIGN_NO_CONFIG_CHECK
```

Leaves this file out of the [startup check](#the-startup-check). Define it
if your project forbids static initializers in headers, as Clang's
`-Wglobal-constructors` does, or if you knowingly build some files with a
different configuration and those files share no types with the rest.
Each file still gets the [inline namespace](#the-inline-namespace) of its
own configuration.

### LOCK_ALIGN_ALLOW_MIXED_DEFAULTS

```cpp
#define LOCK_ALIGN_ALLOW_MIXED_DEFAULTS
```

Makes the [startup check](#the-startup-check) compare only the macros
that change layouts, `LOCK_ALIGN_DISABLE` and
`LOCK_ALIGN_HELD_SLOTS_RESERVED`. Files that both define it may then use
different `LOCK_ALIGN_DEFAULT_POLICY` and
`LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK` values. That's only safe if no type
that relies on a default, such as a class with an `ordered_latch<>`
member, is shared between them.

### enabled

```cpp
inline constexpr bool enabled;
```

Whether checks are compiled in. Useful in `if constexpr` around code of
your own that only makes sense when checking is on.

### held_slots_reserved

```cpp
inline constexpr std::size_t held_slots_reserved = LOCK_ALIGN_HELD_SLOTS_RESERVED;
```

How many entries each thread can hold without allocating.

---

## One configuration per program

`LOCK_ALIGN_DISABLE` changes the size of every wrapper, and
`LOCK_ALIGN_HELD_SLOTS_RESERVED` the layout of each thread's record of
what it holds. The two default macros change which type a name like
`ordered_latch<>` refers to. If two files of one program disagree, the
compiler and linker can't tell: the files share **lock_align**'s code, or
your own types that contain it, with two different meanings. That's
undefined behavior. **lock_align** guards against it in three ways.

### The inline namespace

Everything is declared in an inline namespace inside `lock_align` whose
name encodes the layout, such as `lock_align::v1_on_s32`: layout version 1,
checks on, 32 slots. You still write `lock_align::ordered_mutex`, and you
can still specialize
[`is_recursive_mutex`](../ordered_mutex/is_recursive_mutex.md) as
`lock_align::is_recursive_mutex<...>`. The name shows up in compiler errors
and in a debugger.

Files with different layouts then use different types, so they never share
**lock_align**'s code. A function of yours that takes a **lock_align**
type, defined in one and called from the other, fails to link:

```
Undefined symbols: "use_mutex(lock_align::v1_on_s32::ordered_mutex<std::mutex, 1, ...>&)"
```

Don't name the inline namespace, and don't declare anything in
`lock_align` yourself; include the headers instead.

On MSVC, the linker also reports object files with different layouts
(`LNK2038`), using `#pragma detect_mismatch`.

### The startup check

The inline namespace can't help with a type of your own that contains a
**lock_align** type: `struct account { ordered_mutex<std::mutex, 1> m; };`
still has two layouts. So every file that includes **lock_align**
registers its configuration when the program starts, and if two differ,
the program prints both and aborts before `main`:

```
lock_align: configuration mismatch between translation units:
  src/ledger.cpp: checks on, 32 held slots, default policy lock_align::throw_on_violation, obligations required
  src/fast_path.cpp: checks off, 32 held slots, default policy lock_align::throw_on_violation, obligations required
Define the configuration macros once for the whole program, for example with the lock_align CMake options.
```

The policy is compared by type, so two spellings of the same type agree.
The check costs one static initializer per file, and runs in disabled
builds too, since mixing disabled and checked files is the likeliest
mistake. See [`LOCK_ALIGN_NO_CONFIG_CHECK`](#lock_align_no_config_check)
and [`LOCK_ALIGN_ALLOW_MIXED_DEFAULTS`](#lock_align_allow_mixed_defaults)
to relax it.

Shared libraries are only covered where their inline variables are merged
into one: on ELF platforms with default symbol visibility, and not for
libraries loaded with `RTLD_LOCAL` or for Windows DLLs. The same goes for
each thread's record of what it holds, so a lock taken in one such library
isn't seen by checks in another.

### Within one file

Every **lock_align** header checks that `LOCK_ALIGN_DISABLE` and
`LOCK_ALIGN_HELD_SLOTS_RESERVED` haven't changed since the first one was
included, and fails to compile if they have. The other macros are given
defaults by the first header, so defining one later is a "macro redefined"
warning.

## CMake options

The `lock_align` CMake project turns these cache variables into compile
definitions on the `lock_align::lock_align` target, so every file that
links it gets the same configuration:

| Variable | Default | Defines |
|---|---|---|
| `LOCK_ALIGN_DISABLE` | `OFF` | `LOCK_ALIGN_DISABLE` |
| `LOCK_ALIGN_HELD_SLOTS_RESERVED` | empty, meaning 32 | `LOCK_ALIGN_HELD_SLOTS_RESERVED`; must be an integer |
| `LOCK_ALIGN_DEFAULT_POLICY` | empty, meaning `::lock_align::throw_on_violation` | `LOCK_ALIGN_DEFAULT_POLICY` |
| `LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK` | empty, meaning `required` | `LOCK_ALIGN_DEFAULT_OBLIGATION_CHECK`; `required` or `unchecked` |

For example, a release build without checks:

```cmake
set(LOCK_ALIGN_DISABLE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lock_align)
```

A custom default policy still has to be declared before the first
**lock_align** header in each file; see
[`LOCK_ALIGN_DEFAULT_POLICY`](#lock_align_default_policy).

An installed **lock_align** keeps these definitions: `find_package(lock_align)`
gives a `lock_align::lock_align` configured as the installed copy was, so
every project using that installation shares one configuration. A project
that needs another, such as a release build without checks next to a checked
debug build, installs **lock_align** once for each into separate prefixes.

**lock_align**'s own tests choose configurations file by file, so they
need these options left at their defaults.

/* miniz's compile-time options, in the one place everything that reads the header goes through.
 *
 * ==Why this file exists==
 *
 * **A C library's compile-time options are part of its ABI, and there are three translation units
 * here that include its header.** `miniz.c` compiles the implementation, `shim.c` compiles the three
 * functions sysl cannot reach, and the `c const` block in `c.sysl` is measured from a probe
 * translation unit of the compiler's own making -- a third file, with its own preprocessor state,
 * that includes what `@include` names.
 *
 * Naming this file in all three makes a mismatch impossible rather than merely unlikely. Every one
 * of the options below either deletes symbols or changes the size of a struct this package exports
 * as a constant, so a translation unit that disagreed would compile, link, and be wrong somewhere
 * nothing checks.
 *
 * **There is no other place to put them.** sysl compiles a package's C declaratively and runs no
 * build script, so nothing supplies a `-D` -- and a `-D` would not be enough here anyway, which is
 * the note in `miniz.h` about `TDEFL_LESS_MEMORY`.
 *
 * ==The options, and why these values==
 *
 * `MINIZ_NO_MALLOC` is the one that makes this package what it is. Without it, miniz's zlib-style
 * API allocates its own state and the caller-owned arrangement this binding is built on becomes one
 * of two ways to do the same thing. With it, the heap-returning helpers are not merely unused but
 * absent, so `requires {}` is a claim the linker enforces.
 *
 * `MINIZ_NO_STDIO` removes the `FILE *` API. A package that reads a file on the caller's behalf
 * would need the capability to do it, and this one does not want it -- a stream arrives as a slice.
 *
 * `MINIZ_NO_ARCHIVE_APIS` removes the `.zip` reader and writer, which are a different library
 * wearing the same name: they carry their own I/O callbacks, their own allocation, and 3,000 lines
 * that a program wanting to inflate a firmware image will never call.
 *
 * `MINIZ_NO_TIME` removes `<time.h>` and the archive timestamps that need it, so nothing here reads
 * a clock.
 *
 * `TDEFL_LESS_MEMORY` takes the compressor's state from 319,352 bytes to 167,800, for slightly worse
 * ratios and rather more frequent raw blocks. It is set because the compressor is already the half
 * that will not fit a small microcontroller and 151,552 bytes is the difference between fitting an
 * RP2350 with room to work in and not.
 *
 * **All five are asserted by the tests**, through the state sizes and the bound they produce,
 * because a `#define` is invisible to the linker and those assertions are the only thing standing
 * between a setting that stopped applying and a silent one.
 */
#ifndef SYSL_MINIZ_OPTIONS_H
#define SYSL_MINIZ_OPTIONS_H

#define MINIZ_NO_MALLOC
#define MINIZ_NO_STDIO
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_TIME
#define TDEFL_LESS_MEMORY 1

#include "miniz.h"

#endif

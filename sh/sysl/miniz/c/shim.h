#ifndef SYSL_MINIZ_SHIM_H
#define SYSL_MINIZ_SHIM_H

/* `options.h` rather than `miniz.h`, for the reason that file gives: the options decide what miniz
 * declares, and every translation unit here has to agree about them. */
#include "options.h"

/* What sysl cannot reach in miniz, which is three small things.
 *
 * The first two are miniz's macro entry points. `tinfl_init` and `tinfl_get_adler32` are #defines
 * rather than functions -- the third of the C-only shapes, a macro having no symbol to link --
 * while every other function in the deflate and inflate surface is declared directly in `c.sysl`. */

void sysl_miniz_tinfl_init(tinfl_decompressor *r);
mz_uint32 sysl_miniz_tinfl_get_adler32(tinfl_decompressor *r);

/* The third is alignment, which has to be answered in C because sysl has no pointer-to-integer
 * cast.
 *
 * Both state structs hold pointers, so a byte slice standing in for one has to be aligned as C
 * would have aligned the struct, and a `[]u8` promises nothing about that. Every slice observed
 * while writing this package happened to be 8-aligned, which is not the same as being guaranteed
 * one -- so the storage is over-sized by `alignment - 1` and the state is placed at the first
 * aligned address inside it. That is why the byte counts this package exports are a `sizeof` plus
 * seven rather than the `sizeof` itself. */
void *sysl_miniz_align(void *p, size_t alignment);

/* Whether an address is already aligned, which the raw layer's tests assert on. */
int sysl_miniz_is_aligned(const void *p, size_t alignment);

#endif

#ifndef SYSL_MINIZ_SHIM_H
#define SYSL_MINIZ_SHIM_H

#include "miniz.h"

/* What sysl cannot reach in miniz, which is two macro entry points.
 *
 * `tinfl_init` and `tinfl_get_adler32` are #defines rather than functions -- the third of the C-only
 * shapes, a macro having no symbol to link -- while every other function in the deflate and inflate
 * surface is declared directly in `c.sysl`.
 *
 * There were four of these until miniz 0.2.2. The other two computed alignment, on the stated
 * grounds that sysl had no pointer-to-integer cast; it has had one since v0.0.10, and the two are
 * `sysl.slices.align_up` and `sysl.slices.is_aligned` now, in sysl. What they were for is unchanged
 * and is worth keeping: both state structs hold pointers, so a byte slice standing in for one has to
 * be aligned as C would have aligned the struct, and a `[]u8` promises nothing about that. Every
 * slice observed while writing this package happened to be 8-aligned, which is not the same as being
 * guaranteed one -- so the storage is over-sized by `alignment - 1` and the state is placed at the
 * first aligned address inside it. That is why the byte counts this package exports are a `sizeof`
 * plus seven rather than the `sizeof` itself. */

void sysl_miniz_tinfl_init(tinfl_decompressor *r);
mz_uint32 sysl_miniz_tinfl_get_adler32(tinfl_decompressor *r);

#endif

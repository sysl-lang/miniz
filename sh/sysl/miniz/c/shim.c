#include "shim.h"
#include <stdint.h>

void sysl_miniz_tinfl_init(tinfl_decompressor *r) { tinfl_init(r); }

mz_uint32 sysl_miniz_tinfl_get_adler32(tinfl_decompressor *r) { return tinfl_get_adler32(r); }

int sysl_miniz_is_aligned(const void *p, size_t alignment)
{
    return (int)(((uintptr_t)p % alignment) == 0);
}

void *sysl_miniz_align(void *p, size_t alignment)
{
    uintptr_t a = (uintptr_t)p;

    return (void *)((a + (alignment - 1)) & ~(uintptr_t)(alignment - 1));
}

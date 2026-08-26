#include "shim.h"

void sysl_miniz_tinfl_init(tinfl_decompressor *r) { tinfl_init(r); }

mz_uint32 sysl_miniz_tinfl_get_adler32(tinfl_decompressor *r) { return tinfl_get_adler32(r); }

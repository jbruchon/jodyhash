/* Jody Bruchon's fast hashing function
 *
 * This function was written to generate a fast hash that also has a
 * fairly low collision rate. The collision rate is much higher than
 * a secure hash algorithm, but the calculation is drastically simpler
 * and faster.
 *
 * Copyright (C) 2014-2024 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#include <stdlib.h>
#include "jody_hash.h"
#include "jody_hash_simd.h"

#if (!defined NO_SSE2 || !defined NO_AVX2)
const union UINT256 vec_constant = {
	.v64[0] = JODY_HASH_CONSTANT,
	.v64[1] = JODY_HASH_CONSTANT,
	.v64[2] = JODY_HASH_CONSTANT,
	.v64[3] = JODY_HASH_CONSTANT };
const union UINT256 vec_constant_ror2 = {
	.v64[0] = JODY_HASH_CONSTANT_ROR2,
	.v64[1] = JODY_HASH_CONSTANT_ROR2,
	.v64[2] = JODY_HASH_CONSTANT_ROR2,
	.v64[3] = JODY_HASH_CONSTANT_ROR2 };
#endif

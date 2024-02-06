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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jody_hash.h"
#include "jody_hash_simd.h"

#ifndef NO_SSE2

int jody_block_hash_sse2(jodyhash_t **data, jodyhash_t *hash, const size_t count, size_t *length)
{
	size_t vec_allocsize;
	__m128i *aligned_data;
	__m128i v1, v2, v3, v4, v5, v6;
	__m128 vzero;
	__m128i vec_const, vec_ror2;
	jodyhash_t qhash = *hash;

#if defined __GNUC__ || defined __clang__
	__builtin_cpu_init ();
	if (__builtin_cpu_supports ("avx")) {
		asm volatile ("vzeroall" : : :
			"ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
			"ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15");
	}
#endif /* __GNUC__ || __clang__ */

	/* Constants preload */
	vec_const = _mm_load_si128(&vec_constant.v128[0]);
	vec_ror2  = _mm_load_si128(&vec_constant_ror2.v128[0]);
	vzero = _mm_setzero_ps();

	/* How much memory do we need to align the data? */
	vec_allocsize = count & 0xffffffffffffffe0U;
	/* Only alloc/copy if not already aligned */
	if (((uintptr_t)*data & (uintptr_t)0x0fULL) != 0) {
		aligned_data  = (__m128i *)aligned_alloc(16, vec_allocsize);
		if (!aligned_data) return 1;
		memcpy(aligned_data, *data, vec_allocsize);
	} else aligned_data = (__m128i *)*data;

	for (size_t i = 0; i < (vec_allocsize / 16); i++) {
		v1  = _mm_load_si128(&aligned_data[i]);
		v3  = _mm_load_si128(&aligned_data[i]);
		i++;
		v4  = _mm_load_si128(&aligned_data[i]);
		v6  = _mm_load_si128(&aligned_data[i]);

		/* "element2" gets RORed (two logical shifts ORed together) */
		v1  = _mm_srli_epi64(v1, JODY_HASH_SHIFT);
		v2  = _mm_slli_epi64(v3, (64 - JODY_HASH_SHIFT));
		v1  = _mm_or_si128(v1, v2);
		v1  = _mm_xor_si128(v1, vec_ror2);  // XOR against the ROR2 constant
		v4  = _mm_srli_epi64(v4, JODY_HASH_SHIFT);
		v5  = _mm_slli_epi64(v6, (64 - JODY_HASH_SHIFT));
		v4  = _mm_or_si128(v4, v5);
		v4  = _mm_xor_si128(v4, vec_ror2);  // XOR against the ROR2 constant

		/* Add the constant to "element" */
		v3  = _mm_add_epi64(v3,  vec_const);
		v6  = _mm_add_epi64(v6,  vec_const);

		/* Perform the rest of the hash */
		for (int j = 0; j < 4; j++) {
			uint64_t ep1, ep2;
			switch (j) {
				default:
				case 0:
				/* Lower v1-v3 */
				ep1 = (uint64_t)_mm_cvtsi128_si64(v3);
				ep2 = (uint64_t)_mm_cvtsi128_si64(v1);
				break;

				case 1:
				/* Upper v1-v3 */
				ep1 = (uint64_t)_mm_cvtsi128_si64(_mm_castps_si128(_mm_movehl_ps(vzero, _mm_castsi128_ps(v3))));
				ep2 = (uint64_t)_mm_cvtsi128_si64(_mm_castps_si128(_mm_movehl_ps(vzero, _mm_castsi128_ps(v1))));
				break;

				case 2:
				/* Lower v4-v6 */
				ep1 = (uint64_t)_mm_cvtsi128_si64(v6);
				ep2 = (uint64_t)_mm_cvtsi128_si64(v4);
				break;

				case 3:
				/* Upper v4-v6 */
				ep1 = (uint64_t)_mm_cvtsi128_si64(_mm_castps_si128(_mm_movehl_ps(vzero, _mm_castsi128_ps(v6))));
				ep2 = (uint64_t)_mm_cvtsi128_si64(_mm_castps_si128(_mm_movehl_ps(vzero, _mm_castsi128_ps(v4))));
				break;
			}
			qhash += ep1;
			qhash ^= ep2;
			qhash = JH_ROL2(qhash);
			qhash += ep1;
			}  // End of hash finish loop
		}  // End of main SSE for loop
	*data += vec_allocsize / sizeof(jodyhash_t);
	if (((uintptr_t)*data & (uintptr_t)0x0fULL) != 0) ALIGNED_FREE(aligned_data);
	*length = (count - vec_allocsize) / sizeof(jodyhash_t);
	*hash = qhash;
	return 0;
}

#endif /* NO_SSE2 */

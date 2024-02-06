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

#ifndef NO_AVX2

int jody_block_hash_avx2(jodyhash_t **data, jodyhash_t *hash, const size_t count, size_t *length)
{
	size_t vec_allocsize;
	__m256i *aligned_data;
	/* Regs used in groups of 3; 1=ROR/XOR work, 2=temp, 3=data+constant */
	__m256i vx1, vx2, vx3;
	__m256i avx_const, avx_ror2;
	jodyhash_t qhash = *hash;

	/* Constants preload */
	avx_const = _mm256_load_si256(&vec_constant.v256);
	avx_ror2  = _mm256_load_si256(&vec_constant_ror2.v256);

	/* How much memory do we need to align the data? */
	vec_allocsize = count & 0xffffffffffffffe0U;
	/* Only alloc/copy if not already aligned */
	if (((uintptr_t)*data & (uintptr_t)0x1fULL) != 0) {
		aligned_data  = (__m256i *)aligned_alloc(32, vec_allocsize);
		if (!aligned_data) return 1;
		memcpy(aligned_data, *data, vec_allocsize);
	} else aligned_data = (__m256i *)*data;

	for (size_t i = 0; i < (vec_allocsize / 32); i++) {
		vx1  = _mm256_load_si256(&aligned_data[i]);
		vx3  = _mm256_load_si256(&aligned_data[i]);

		/* "element2" gets RORed (two logical shifts ORed together) */
		vx1  = _mm256_srli_epi64(vx1, JODY_HASH_SHIFT);
		vx2  = _mm256_slli_epi64(vx3, (64 - JODY_HASH_SHIFT));
		vx1  = _mm256_or_si256(vx1, vx2);
		vx1  = _mm256_xor_si256(vx1, avx_ror2);  // XOR against the ROR2 constant

		/* Add the constant to "element" */
		vx3  = _mm256_add_epi64(vx3,  avx_const);

		/* Perform the rest of the hash */
		for (int j = 0; j < 4; j++) {
			uint64_t ep1, ep2;
			switch (j) {
				default:
				case 0:
				ep1 = (uint64_t)_mm256_extract_epi64(vx3, 0);
				ep2 = (uint64_t)_mm256_extract_epi64(vx1, 0);
				break;
				case 1:
				ep1 = (uint64_t)_mm256_extract_epi64(vx3, 1);
				ep2 = (uint64_t)_mm256_extract_epi64(vx1, 1);
				break;
				case 2:
				ep1 = (uint64_t)_mm256_extract_epi64(vx3, 2);
				ep2 = (uint64_t)_mm256_extract_epi64(vx1, 2);
				break;
				case 3:
				ep1 = (uint64_t)_mm256_extract_epi64(vx3, 3);
				ep2 = (uint64_t)_mm256_extract_epi64(vx1, 3);
				break;
			}
			qhash += ep1;
			qhash ^= ep2;
			qhash = JH_ROL2(qhash);
			qhash += ep1;
		}  // End of hash finish loop
	}  // End of main AVX for loop
	*data += vec_allocsize / sizeof(jodyhash_t);
	if (((uintptr_t)*data & (uintptr_t)0x1fULL) != 0) ALIGNED_FREE(aligned_data);
	*length = (count - vec_allocsize) / sizeof(jodyhash_t);
	*hash = qhash;
	return 0;
}

#endif /* NO_AVX2 */

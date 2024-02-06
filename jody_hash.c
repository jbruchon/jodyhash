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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jody_hash.h"
#include "jody_hash_simd.h"
#include "likely_unlikely.h"

/* Rolling hash block size (4K by default) */
#ifndef ROLLBSIZE
 #define ROLLBSIZE 4096
#endif
#define ROLLBSIZEW (ROLLBSIZE / sizeof(jodyhash_t))


static const jodyhash_t jh_s_constant = JH_ROR2(JODY_HASH_CONSTANT);

/* Hash a block of arbitrary size; must be divisible by sizeof(jodyhash_t)
 * The first block should pass an initial hash of zero.
 * All blocks after the first should pass hash as the value
 * returned by the last call to this function. This allows hashing
 * of any amount of data. If data is not divisible by the size of
 * jodyhash_t, it is MANDATORY that the caller provide a data buffer
 * which is divisible by sizeof(jodyhash_t). */
extern int jody_block_hash(jodyhash_t *data, jodyhash_t *hash, const size_t count)
{
	jodyhash_t element, element2;
	size_t length = 0;

	/* Don't bother trying to hash a zero-length block */
	if (unlikely(count == 0)) return 0;

#ifndef NO_AVX2
#if defined __GNUC__ || defined __clang__
	__builtin_cpu_init ();
	if (__builtin_cpu_supports ("avx2")) {
#endif /* __GNUC__ || __clang__ */
		if (count >= 32) {
			if (jody_block_hash_avx2(&data, hash, count, &length) != 0) return 1;
			goto skip_sse2;
		} else length = count / sizeof(jodyhash_t);
#if defined __GNUC__ || defined __clang__
	} else length = count / sizeof(jodyhash_t);
#endif
#else
	length = count / sizeof(jodyhash_t);
#endif /* NO_AVX2 */


#ifndef NO_SSE2
#if defined __GNUC__ || defined __clang__
	__builtin_cpu_init ();
	if (__builtin_cpu_supports ("sse2")) {
#endif /* __GNUC__ || __clang__ */
		if (count >= 32) {
			if (jody_block_hash_sse2(&data, hash, count, &length) != 0) return 1;
		}
		else length = count / sizeof(jodyhash_t);
#if defined __GNUC__ || defined __clang__
	} else length = count / sizeof(jodyhash_t);
#endif
#else
	length = count / sizeof(jodyhash_t);
#endif /* NO_SSE2 */

#ifndef NO_AVX2
skip_sse2:
#endif
	/* Hash everything (normal) or remaining small tails (SSE2) */
	for (; length > 0; length--) {
		element = *data;
		element2 = JH_ROR(element);
		element2 ^= jh_s_constant;
		element += JODY_HASH_CONSTANT;
		*hash += element;
		*hash ^= element2;
		*hash = JH_ROL2(*hash);
		*hash += element;
		data++;
	}

	/* Handle data tail (for blocks indivisible by sizeof(jodyhash_t)) */
	length = count & (sizeof(jodyhash_t) - 1);
	if (length) {
		element = *data & tail_mask[length];
		element2 = JH_ROR(element);
		element2 ^= jh_s_constant;
		element += JODY_HASH_CONSTANT;
		*hash += element;
		*hash ^= element2;
		*hash = JH_ROL2(*hash);
		*hash += element2;
	}

	return 0;
}


extern int jody_rolling_block_hash(jodyhash_t *data, jodyhash_t *hash, const size_t count)
{
	jodyhash_t rollhash;
	size_t blocks = (count & ~((uint64_t)ROLLBSIZE - 1)) / ROLLBSIZE;
	for (unsigned int i = 0; i < blocks; i++) {
//fprintf(stderr, "rolling([%u] %p, %016" PRIx64 ", %d)\n", i, (void *)data, *hash, ROLLBSIZE);
		rollhash = 0;
		if (jody_block_hash(data, &rollhash, ROLLBSIZE)) return 1;
		*hash ^= rollhash;
		data += ROLLBSIZEW;
	}
	/* Hash the last block */
	blocks = count - (blocks * ROLLBSIZE);
	if (blocks > 0) {
//fprintf(stderr, "rolltail(%p, %016" PRIx64 ", %d)\n", (void *)data, *hash, ROLLBSIZE);
		rollhash = 0;
		if (jody_block_hash(data, &rollhash, blocks)) return 1;
		*hash ^= rollhash;
	}
	return 0;
}

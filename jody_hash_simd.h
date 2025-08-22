/* Jody Bruchon's fast hashing function (headers)
 * See jody_hash.c for license information */

#ifndef JODY_HASH_SIMD_H
#define JODY_HASH_SIMD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "jody_hash.h"

/* Disable SIMD if not 64-bit width or not 64-bit x86 code */
#if JODY_HASH_WIDTH != 64 || !defined __x86_64__ || SIZE_MAX == 0xffffffff || (defined NO_SSE2 && defined NO_AVX2)
 #ifndef NO_SSE2
  #define NO_SSE2
 #endif
 #ifndef NO_AVX2
  #define NO_AVX2
 #endif
 #ifndef NO_SIMD
  #define NO_SIMD
 #endif
#endif

/* Use SIMD by default */
#if !defined NO_SIMD
 #if (__STDC_VERSION__ < 201112L)
  #include "c99_aligned_alloc.h"
 #endif
 #if defined __APPLE__
  /* Mac OS X < 10.15 don't support aligned_alloc */
  #include <x86intrin.h>
  #define aligned_alloc(a,b) malloc(b)
  #define ALIGNED_FREE(a) free(a)
 #elif defined _MSC_VER || defined _WIN32 || defined __MINGW32__
  /* Microsoft C/C++-compatible compiler */
  #include <intrin.h>
  #define aligned_alloc(a,b) _aligned_malloc(b,a)
  #define ALIGNED_FREE(a) _aligned_free(a)
 #elif (defined __GNUC__  || defined __clang__ ) && (defined __x86_64__  || defined __i386__ )
  /* GCC or Clang targeting x86/x86-64 */
  #include <x86intrin.h>
  #define ALIGNED_FREE(a) free(a)
 #endif
#endif /* !NO_SIMD */

#if !defined NO_SSE2 || !defined NO_AVX2
union UINT256 {
	__m256i  v256;
	__m128i  v128[2];
	uint64_t v64[4];
};

extern const union UINT256 vec_constant, vec_constant_ror2;
#endif

extern int jody_block_hash_avx2(jodyhash_t **data, jodyhash_t *hash, const size_t count, size_t *length);
extern int jody_block_hash_sse2(jodyhash_t **data, jodyhash_t *hash, const size_t count, size_t *length);

#ifdef __cplusplus
}
#endif

#endif	/* JODY_HASH_SIMD_H */

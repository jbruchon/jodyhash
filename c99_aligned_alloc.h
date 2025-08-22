/* Implementation of C11 aligned_alloc() using POSIX 2001 posix_memalign()
 *
 * Created by Jody Bruchon <jody@jodybruchon.com>
 *
 * Released into the public domain, and under the Creative Commons 0 license:
 * https://creativecommons.org/public-domain/cc0/
 */

#ifndef C99_ALIGNED_ALLOC_H
#define C99_ALIGNED_ALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Required for uint64_t */
#include <stdlib.h>

#if !(_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
 #error POSIX 2001 compliant environment is required
#endif

extern void *aligned_alloc(size_t alignment, size_t size);

#ifdef __cplusplus
}
#endif

#endif	/* C99_ALIGNED_ALLOC_H */

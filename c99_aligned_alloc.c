/* Implementation of C11 aligned_alloc() using POSIX 2001 posix_memalign()
 *
 * Created by Jody Bruchon <jody@jodybruchon.com>
 *
 * Released into the public domain, and under the Creative Commons 0 license:
 * https://creativecommons.org/public-domain/cc0/
 */

#include <stdlib.h>
#include "c99_aligned_alloc.h"

void *aligned_alloc(size_t alignment, size_t size)
{
	void *mem;
	int i;

	i = posix_memalign(&mem, alignment, size);
	if (i != 0) mem = NULL;

	return mem;
}

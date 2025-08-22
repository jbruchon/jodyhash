# jodyhash Makefile

CC ?= gcc
#COMPILER_OPTIONS = -Os -flto -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables
COMPILER_OPTIONS += -O2 -g
#COMPILER_OPTIONS += -Og -g3
COMPILER_OPTIONS += -I. -D_FILE_OFFSET_BITS=64 -fstrict-aliasing -pipe
COMPILER_OPTIONS += -Wall -Wextra -Wwrite-strings -Wcast-align -Wstrict-aliasing -pedantic -Wstrict-overflow -Wstrict-prototypes -Wpointer-arith -Wundef
COMPILER_OPTIONS += -Wshadow -Wfloat-equal -Wstrict-overflow=5 -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wformat=2 -Winit-self -Wconversion
#LINK_OPTIONS=-s -Wl,--gc-sections
#LINK_OPTIONS=

PREFIX ?= /usr/local
EXEC_PREFIX ?= ${PREFIX}
BINDIR ?= ${exec_PREFIX}/bin
MANDIR ?= ${PREFIX}/man
DATAROOTDIR ?= ${PREFIX}/share
DATADIR ?= ${datarootdir}
SYSCONFDIR ?= ${PREFIX}/etc

ifeq ($(OS), Windows_NT)
# MinGW needs this for printf() conversions to work
	WIN_CFLAGS += -D__USE_MINGW_ANSI_STDIO=1 -municode
	EXT=.exe
# Windows resources for Windows XP and NT6+
	ifeq ($(UNAME_S), MINGW32_NT-5.1)
		OBJS += winres_xp.o
	else
		OBJS += winres.o
	endif
endif

UNAME_S       = $(shell uname -s)
UNAME_M       = $(shell uname -m)
CROSS_DETECT  = $(shell echo "$(CC)" | grep -- '-' | grep -v x86_64 || echo "none")

ifneq ($(UNAME_M), x86_64)
NO_SIMD=1
endif
ifneq ($(CROSS_DETECT), none)
NO_SIMD=1
endif


# SIMD SSE2/AVX2 implementations may need these extra flags
ifdef NO_SIMD
COMPILER_OPTIONS += -DNO_SIMD -DNO_SSE2 -DNO_AVX2
else
SIMD_OBJS += jody_hash_simd.o
ifdef NO_SSE2
COMPILER_OPTIONS += -DNO_SSE2
else
SIMD_OBJS += jody_hash_sse2.o
endif
ifdef NO_AVX2
COMPILER_OPTIONS += -DNO_AVX2
else
SIMD_OBJS += jody_hash_avx2.o
endif
endif

# C standard selection
ifndef USE_C99
COMPILER_OPTIONS += -std=gnu11
else
COMPILER_OPTIONS += -std=gnu99
#ifndef NO_SIMD
 OBJS += c99_aligned_alloc.o
#endif
endif


ifdef PERFBENCHMARK
COMPILER_OPTIONS += -DPERFBENCHMARK
endif

CFLAGS += $(COMPILER_OPTIONS) $(WIN_CFLAGS) $(CFLAGS_EXTRA)
LDFLAGS += $(LINK_OPTIONS)

all: jodyhash
	-@test "$(CROSS_DETECT)" != "none" && echo "WARNING: SIMD disabled: cross-compiler !x86_64 detected (CC = $(CC))" || true

benchmark: jody_hash.o benchmark.o
	$(CC) -c benchmark.c $(CFLAGS) -o benchmark.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o benchmark jody_hash.o benchmark.o
	./benchmark 1000000

jodyhash: jody_hash.o utility.o $(OBJS) $(SIMD_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(WIN_CFLAGS) -o jodyhash jody_hash.o utility.o $(OBJS) $(SIMD_OBJS)

jody_hash_simd.o:
	$(CC) $(CFLAGS) $(WIN_CFLAGS) -mavx2 -msse2 -c -o jody_hash_simd.o jody_hash_simd.c

jody_hash_avx2.o: jody_hash_simd.o
	$(CC) $(CFLAGS) $(WIN_CFLAGS) -mavx2 -c -o jody_hash_avx2.o jody_hash_avx2.c

jody_hash_sse2.o: jody_hash_simd.o
	$(CC) $(CFLAGS) $(WIN_CFLAGS) -msse2 -c -o jody_hash_sse2.o jody_hash_sse2.c

.c.o:
	$(CC) -c $(CFLAGS) $(WIN_CFLAGS) $<

winres.o: winres.rc winres.manifest.xml
	./tune_winres.sh
	windres winres.rc winres.o

winres_xp.o: winres_xp.rc
	./tune_winres.sh
	windres winres_xp.rc winres_xp.o

stripped: jodyhash
	strip jodyhash$(EXT)

test: jodyhash
	./test.sh

clean:
	rm -f *.o *~ .*un~ benchmark jodyhash$(SUFFIX) debug.log *.?.gz

distclean: clean
	rm -f *.pkg.tar.* *.zip
	rm -rf jodyhash-*-*/

install: all
	install -D -o root -g root -m 0755 -s jodyhash $(DESTDIR)/$(bindir)/jodyhash

chrootpackage:
	+./chroot_build.sh

package:
	+./generate_packages.sh

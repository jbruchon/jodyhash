/*
 * Jody Bruchon hashing function command-line utility
 *
 * Copyright (C) 2014-2024 by Jody Bruchon <jody@jodybruchon.com>
 * Released under the MIT License (see LICENSE for details)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include "likely_unlikely.h"
#include "jody_hash.h"
#include "jody_hash_simd.h"
#include "version.h"

/* Linux perf benchmarking*/
#if defined(__linux__) && defined(PERFBENCHMARK)
 #define _GNU_SOURCE
 #include <sys/ioctl.h>
 #include <linux/perf_event.h>
 #include <sys/syscall.h>
 #include <sys/types.h>
 #include <errno.h>
 #define USE_PERF_CODE
#endif

/* Detect Windows and modify as needed */
#if defined _WIN32 || defined __CYGWIN__
 #define ON_WINDOWS 1
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <io.h>
 /* Output end of error string with Win32 Unicode as needed */
 #define ERR(a,b) { _setmode(_fileno(stderr), _O_U16TEXT); \
		 fwprintf(stderr, L"%S\n", a); \
		 _setmode(_fileno(stderr), _O_TEXT); }
#else
 #define ERR(a,b) fprintf(stderr, "%s\n", b);
#endif

#if JODY_HASH_WIDTH == 64
#define PRINTHASH(a) printf("%016" PRIx64,a)
#endif
#if JODY_HASH_WIDTH == 32
#define PRINTHASH(a) printf("%08" PRIx32,a)
#endif
#if JODY_HASH_WIDTH == 16
#define PRINTHASH(a) printf("%04" PRIx16,a)
#endif

#ifndef BSIZE
#define BSIZE 32768
#endif

static int error = EXIT_SUCCESS;
static char *progname;

static void usage(int detailed)
{
	printf("Jody Bruchon's hashing utility %s (%s) [%d bit width]%s\n",
		VER, VERDATE, JODY_HASH_WIDTH,
#if !defined NO_AVX2 && !defined NO_SSE2
		" AVX2/SSE2 accelerated"
#elif !defined NO_AVX2
		" AVX2 accelerated"
#elif !defined NO_SSE2
		" SSE2 accelerated"
#else
		" standard"
#endif
		);
	if (detailed == 0) return;
	printf("usage: %s [-b|s|n|l|L] [file_to_hash]\n", progname);
	printf("Specifying no name or '-' as the name reads from stdin\n");
	printf("  -b|-s  Output in md5sum binary style instead of bare hashes\n");
	printf("  -n     Output just the file name after the hash\n");
	printf("  -l     Generate a hash for each text input line\n");
	printf("  -L     Same as -l but also prints hashed text after the hash\n");
	printf("  -B     Output a hash for every 4096 byte block of the file\n");
	printf("  -r     Output a rolling 4K hash\n");
	return;
}

#ifdef UNICODE
/* Copy Windows wide character arguments to UTF-8 */
static void widearg_to_argv(int argc, wchar_t **wargv, char **argv)
{
	char temp[PATH_MAX + 1];
	int len;

	if (!argv) goto error_bad_argv;
	for (int counter = 0; counter < argc; counter++) {
		len = WideCharToMultiByte(CP_UTF8, 0, wargv[counter],
				-1, (LPSTR)&temp, PATH_MAX * 2, NULL, NULL);
		if (unlikely(len < 1)) goto error_wc2mb;

		argv[counter] = (char *)malloc((size_t)len + 1);
		if (!argv[counter]) goto error_oom;
		strncpy(argv[counter], temp, (size_t)len + 1);
	}
	return;

error_bad_argv:
	fprintf(stderr, "fatal: bad argv pointer\n");
	exit(EXIT_FAILURE);
error_wc2mb:
	fprintf(stderr, "fatal: WideCharToMultiByte failed\n");
	exit(EXIT_FAILURE);
error_oom:
	fprintf(stderr, "out of memory\n");
	exit(EXIT_FAILURE);
}


/* Unicode on Windows requires wmain() and the -municode compiler switch */
int wmain(int argc, wchar_t **wargv)
#else
int main(int argc, char **argv)
#endif /* UNICODE */
{
	static jodyhash_t blk[(BSIZE / sizeof(jodyhash_t))];
	static char name[PATH_MAX + 1];
	static size_t i;
	static FILE *fp;
	static jodyhash_t hash;
	static int argnum = 1;
	static int outmode = 0;
	static int read_err = 0;
	//intmax_t bytes = 0;

#ifdef USE_PERF_CODE
	struct perf_event_attr pe;
	long long pcnt;
	int fd;
	/* From man perf_event_open(2) */
	memset(&pe, 0, sizeof(struct perf_event_attr));
	pe.type = PERF_TYPE_HARDWARE;
	pe.size = sizeof(struct perf_event_attr);
	pe.config = PERF_COUNT_HW_CPU_CYCLES;
	pe.disabled = 1;
	pe.exclude_kernel = 1;
	// Don't count hypervisor events.
	pe.exclude_hv = 1;
	errno = 0;
	fd = (int)syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
	if (fd == -1) {
		fprintf(stderr, "Error opening perf %llx: %s\n", pe.config, strerror(errno));
		exit(EXIT_FAILURE);
	}
	ioctl(fd, PERF_EVENT_IOC_RESET, 0);
#endif /* USE_PERF_CODE */

#ifdef UNICODE
	static wchar_t wname[PATH_MAX];
	/* Create a UTF-8 **argv from the wide version */
	static char **argv;
	argv = (char **)malloc(sizeof(char *) * (unsigned int)argc);
	if (!argv) goto error_oom;
	widearg_to_argv(argc, wargv, argv);
#endif /* UNICODE */

	progname = argv[0];

	/* Process options */
	if (argc > 1) {
		if (!strcmp("-v", argv[1]) || !strcmp("--version", argv[1])) {
			usage(0);
			exit(EXIT_SUCCESS);
		}
		if (!strcmp("-h", argv[1]) || !strcmp("--help", argv[1])) {
			usage(1);
			exit(EXIT_SUCCESS);
		}
	}
	if (argc > 2) {
		if (!strcmp("-s", argv[1]) || !strcmp("-b", argv[1])) outmode = 1;
		if (!strcmp("-l", argv[1])) outmode = 2;
		if (!strcmp("-L", argv[1])) outmode = 3;
		if (!strcmp("-n", argv[1])) outmode = 4;
		if (!strcmp("-B", argv[1])) outmode = 5;
		if (!strcmp("-r", argv[1])) outmode = 6;
		if (outmode > 0 || !strcmp("--", argv[1])) argnum++;
	}

	do {
		hash = 0;
		/* Read from stdin */
		if (argc == 1 || !strcmp("-", argv[argnum])) {
			strncpy(name, "-", PATH_MAX);
#ifdef ON_WINDOWS
			_setmode(_fileno(stdin), _O_BINARY);
#endif
			fp = stdin;
		} else {
			strncpy(name, argv[argnum], PATH_MAX);
#ifdef UNICODE
			fp = _wfopen(wargv[argnum], L"rbS");
			if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, PATH_MAX)) goto error_mb2wc;
#else
			fp = fopen(argv[argnum], "rb");
#endif /* UNICODE */
		}

		if (!fp) {
			fprintf(stderr, "error: cannot open: ");
			ERR(wname, name);
			error = EXIT_FAILURE;
			argnum++;
			continue;
		}

		/* Line-by-line hashing with -l/-L */
		if (outmode == 2 || outmode == 3) {
			while (fgets((char *)blk, BSIZE, fp) != NULL) {
				hash = 0;
				if (ferror(fp)) {
					fprintf(stderr, "error reading file: ");
					goto error_loop1;
				}
				/* Skip empty lines */
				i = strlen((char *)blk);
				if (i < 2 || *(char *)blk == '\n') continue;
				/* Strip \r\n and \n and \r newlines */
				if (((char *)blk)[i - 2] == '\r') ((char *)blk)[i - 2] = '\0';
				else ((char *)blk)[i - 1] = '\0';

				if (jody_block_hash(blk, &hash, i - 1) != 0) {
					fprintf(stderr, "error hashing file: ");
					goto error_loop1;
				}

				PRINTHASH(hash);
				if (outmode == 3) printf(" '%s'\n", (char *)blk);
				else printf("\n");

				if (feof(fp)) break;
				continue;
error_loop1:
				ERR(wname, name);
				error = EXIT_FAILURE; read_err = 1;
				break;
			}
			goto close_file;
		}

		while ((i = fread((void *)blk, 1, BSIZE, fp))) {
			if (ferror(fp)) {
				ERR(wname, name);
				error = EXIT_FAILURE; read_err = 1;
				break;
			}
			//bytes += i;
			if (outmode == 5) {
				/* Hash sub-blocks in the file */
				const int kbsize = 4096;
				const int kboffsize = kbsize / (int)sizeof(jodyhash_t);
				int kblk = 0;
				size_t kbdrop;

				while (i > 0) {
					hash = 0;
					kbdrop = (i > kbsize) ? kbsize : i;
					if (jody_block_hash(blk + (kblk * kboffsize), &hash, kbdrop) != 0) goto error_loop2;
					PRINTHASH(hash); printf("\n");
					kblk++;
					i -= kbdrop;
					continue;
error_loop2:
					fprintf(stderr, "error hashing file: ");
					ERR(wname, name);
					error = EXIT_FAILURE; read_err = 1;
					break;
				}
			} else {
#ifdef USE_PERF_CODE
				/* perf benchmarked code */
				ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
				if ((outmode == 6 ? jody_rolling_block_hash(blk, &hash, i)
						: jody_block_hash(blk, &hash, i)) != 0) {
					fprintf(stderr, "error hashing file: ");
					ERR(wname, name);
					error = EXIT_FAILURE; read_err = 1;
					break;
				}
				ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
#else
				/* non-benchmarked code */
//if (outmode == 6) fprintf(stderr, "doing a rolling hash of %lu bytes\n", i);
				if ((outmode == 6 ? jody_rolling_block_hash(blk, &hash, i)
						: jody_block_hash(blk, &hash, i)) != 0) {
					fprintf(stderr, "error hashing file: ");
					ERR(wname, name);
					error = EXIT_FAILURE; read_err = 1;
					break;
				}
#endif /* USE_PERF_CODE */
			}
			if (feof(fp)) break;
		}

#ifdef USE_PERF_CODE
		read(fd, &pcnt, sizeof(long long));
		fprintf(stderr, "CPU cycles: %lld\n", pcnt);
		close(fd);
#endif /* USE_PERF_CODE */

		/* Loop without result on read errors */
		if (read_err == 1) {
			read_err = 0;
			goto close_file;
		}

		if (outmode != 5) PRINTHASH(hash);

#ifdef UNICODE
		_setmode(_fileno(stdout), _O_U16TEXT);
		if (outmode == 1) wprintf(L" *%S", wargv[argnum]);
		else if (outmode == 4) wprintf(L" %S", wargv[argnum]);
		_setmode(_fileno(stdout), _O_TEXT);
		printf("\n");
#else
		if (outmode == 1) printf(" *%s\n", name);
		else if (outmode == 4) printf(" %s\n", name);
		else printf("\n");
#endif /* UNICODE */
		//fprintf(stderr, "processed %jd bytes\n", bytes);
close_file:
		fclose(fp);
		argnum++;
	} while (argnum < argc);

	exit(error);

#ifdef UNICODE
error_oom:
	fprintf(stderr, "out of memory\n");
	exit(EXIT_FAILURE);
error_mb2wc:
	fprintf(stderr, "fatal: MultiByteToWideChar failed\n");
	exit(EXIT_FAILURE);
#endif
}


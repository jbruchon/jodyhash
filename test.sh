#!/bin/sh

[ -z "$JODYHASH" ] && JODYHASH=./jodyhash
[ ! -x "$JODYHASH" ] && echo "Please build the program first, or set env JODYHASH=/path/to/jodyhash_program" && exit 128

TESTDIR=testdata
ERR=0
for X in 1_million_numbers all_english_words_lcuc \
	short1 short2 short3 short4 short5 short6 short7 short8 short9 \
	short10 short15 short16 short17
	do if [ "$($JODYHASH "$TESTDIR/${X}.txt")" = "$(cat "$TESTDIR/hash_${X}.txt")" ]
		then echo "PASSED: $X"
		else echo "FAILED: $X"; ERR=1
	fi
done
exit $ERR

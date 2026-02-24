#!/bin/sh

NL='
'

export LANG="en_US.UTF-8"

export VIX_PATH=.
[ -z "$VIX" ] && VIX="../../vix"
[ -z "$SAM" ] && SAM="sam"
[ -z "$PLAN9" ] && PLAN9="/usr/local/plan9/bin"

for SAM in "$SAM" "$PLAN9/sam" /usr/lib/plan9/bin/sam 9; do
	if type "$SAM" >/dev/null 2>&1; then
		break
	fi
done

type "$SAM" >/dev/null 2>&1 || {
	echo "sam(1) not found, skipping tests"
	exit 0
}

[ "$SAM" = "9" ] && SAM="9 sam"

echo "$SAM"
$VIX -v

if ! $VIX -v | grep '+lua' >/dev/null 2>&1; then
	echo "vix compiled without lua support, skipping tests"
	exit 0
fi

TESTS=$1
[ -z "$TESTS" ] && TESTS=$(find . -name '*.cmd' | sed 's/\.cmd$//g')

TESTS_RUN=0
TESTS_OK=0

for t in $TESTS; do
	IN="$t.in"
	SAM_OUT="$t.sam.out"
	SAM_ERR="$t.sam.err"
	VIX_OUT="$t.vix.out"
	VIX_ERR="$t.vix.err"
	REF="$t.ref"
	rm -f "$SAM_OUT" "$SAM_ERR" "$VIX_OUT" "$VIX_ERR"
	printf "Running test %s with sam ... " "$t"

	{
		echo ',{'
		cat "$t.cmd"
		echo '}'
		echo ,
	} | $SAM -d "$IN" > "$SAM_OUT" 2>/dev/null

	if [ $? -ne 0 ]; then
		printf "ERROR\n"
	elif [ -e "$REF" ]; then
		if cmp -s "$REF" "$SAM_OUT"; then
			printf "OK\n"
		else
			printf "FAIL\n"
			diff -u "$REF" "$SAM_OUT" > "$SAM_ERR"
		fi
	elif [ -e "$SAM_OUT" ]; then
		REF="$SAM_OUT"
		printf "OK\n"
	fi

	if [ ! -e "$REF" ]; then
		printf " No reference solution, skipping.\n"
		continue
	fi

	TESTS_RUN=$((TESTS_RUN+1))

	$VIX '+qall!' "$IN" </dev/null 2>/dev/null
	RETURN_CODE=$?

	printf "Running test %s with vix ... " "$t"
	if [ $RETURN_CODE -ne 0 -o ! -e "$VIX_OUT" ]; then
		printf "ERROR\n"
	elif cmp -s "$REF" "$VIX_OUT"; then
		printf "OK\n"
		TESTS_OK=$((TESTS_OK+1))
	else
		printf "FAIL\n"
		diff -u "$REF" "$VIX_OUT" > "$VIX_ERR"
	fi
done

printf "Tests ok %d/%d\n" $TESTS_OK $TESTS_RUN

# set exit status
[ $TESTS_OK -eq $TESTS_RUN ]

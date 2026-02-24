#!/bin/sh

export VIX_PATH=.
export PATH="$(pwd)/../..:$PATH"
export LANG="en_US.UTF-8"
[ -z "$VIX" ] && VIX="../../vix"
$VIX -v

if ! $VIX -v | grep '+lua' >/dev/null 2>&1; then
	echo "vix compiled without lua support, skipping tests"
	exit 0
fi

TESTS_OK=0
TESTS_RUN=0

if [ $# -gt 0 ]; then
	test_files=$*
else
	printf ':help\n:/ Lua paths/,$ w help\n:qall\n' | $VIX 2> /dev/null && cat help && rm -f help
	test_files="$(find . -type f -name '*.in')"
fi

for t in $test_files; do
	TESTS_RUN=$((TESTS_RUN + 1))
	t=${t%.in}
	t=${t#./}
	$VIX '+qall!' "$t".in < /dev/null 2> "$t".log
	RETURN_CODE=$?

	printf "%-50s" "$t"
	if [ $RETURN_CODE -eq 0 -a -e "$t".out ]; then
		if cmp -s "$t".ref "$t".out 2> /dev/null; then
			printf "PASS\n"
			TESTS_OK=$((TESTS_OK + 1))
		else
			printf "FAIL\n"
			diff -u "$t".ref "$t".out > "$t".err
			if [ -s "$t".log ]; then
				echo "--- STDERR ---" >> "$t".err
				cat "$t".log >> "$t".err
			fi
		fi
	else
		printf "ERROR\n"
		cat "$t".log > "$t".err
	fi
done

printf "Tests ok %d/%d\n" $TESTS_OK $TESTS_RUN

# set exit status
[ $TESTS_OK -eq $TESTS_RUN ]

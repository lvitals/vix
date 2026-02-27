#!/bin/bash

VIX="./vix"
TEST_FILE="test/perf_data.txt"
LOG_FILE="test/perf_results.log"

# Ensure test file exists (1M lines ~50MB)
if [ ! -f "$TEST_FILE" ]; then
    echo "Generating test file..."
    seq 1 1000000 | sed 's/$/: test line for vix editor performance check/' > "$TEST_FILE"
fi

echo "--- VIX PERFORMANCE BENCHMARK ---"
echo "File: $TEST_FILE ($(du -h "$TEST_FILE" | cut -f1))"
echo "---------------------------------"

run_bench() {
    local label="$1"
    local cmd="$2"
    echo -n "Executing: $label ... "
    
    echo "[$label]" >> "$LOG_FILE"
    
    START=$(date +%s%N)
    "$VIX" -headless "$TEST_FILE" "+$cmd" "+qall!" >/dev/null 2>&1
    END=$(date +%s%N)
    
    DIFF=$(( (END - START) / 1000000 ))
    echo "${DIFF}ms"
    echo "Time: ${DIFF}ms" >> "$LOG_FILE"
    echo "---------------------------------"
}

# Clear previous log
> "$LOG_FILE"

# Performance Tests
run_bench "Cold Load" " "
run_bench "Jump to End" ":$"
run_bench "Forward Search (near end)" "/999990:"
run_bench "Backward Search (near start)" "?100:"
run_bench "Forward Search (NOT FOUND - Worst Case)" "/NOMATCH_STRING_XYZ/"
run_bench "Backward Search (NOT FOUND - Worst Case)" "?NOMATCH_STRING_XYZ?"

echo "Benchmark finished. See details in $LOG_FILE"

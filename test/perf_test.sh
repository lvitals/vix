#!/bin/bash

VIX="./vix"
TEST_FILE="test/perf_data.txt"
LOG_FILE="test/perf_results.log"

# Ensure test file exists (1M lines ~57MB)
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
    
    # Run vix and capture real time
    # The qall! command ensures exit.
    # No timeout used here to measure REAL elapsed time.
    START=$(date +%s%N)
    "$VIX" -headless "$TEST_FILE" "+$cmd" "+qall!" >/dev/null 2>&1
    END=$(date +%s%N)
    
    # Calculate milliseconds
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
run_bench "Repeated Jumps (Start -> End -> Start)" ":1 :$ :1"
run_bench "Forward Regex Search (near end)" "/999990:"
run_bench "Backward Regex Search (near start)" "?100:"

echo "Benchmark finished. See details in $LOG_FILE"

#!/bin/bash

# Ensure we are in the project root
cd "$(dirname "$0")/.."

# Requires Valgrind (install: sudo apt install valgrind)
# Performs comprehensive memory analysis across all suítes.

# Output colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

LOG_DIR="valgrind_logs"
mkdir -p "$LOG_DIR"
rm -f "$LOG_DIR"/*

echo -e "${GREEN}Recompiling vix and test suites with debug symbols...${NC}"
make clean > /dev/null
make DEBUG=1 -j$(nproc) > /dev/null
make -C test DEBUG=1 -j$(nproc) > /dev/null

# Statistics counters
TOTAL_TESTS=0
TOTAL_PASSED=0
TOTAL_FAILED=0

INT_COUNT=0
INT_PASSED=0
INT_FAILED=0

UNIT_COUNT=0
UNIT_PASSED=0
UNIT_FAILED=0

LUA_COUNT=0
LUA_PASSED=0
LUA_FAILED=0

FAILED_LIST=()

# Function to run Valgrind on a specific command
run_valgrind() {
    local cmd="$1"
    local log="$2"
    # --errors-for-leak-kinds=definite marks real leaks as errors
    valgrind --leak-check=full \
             --show-leak-kinds=definite,possible \
             --errors-for-leak-kinds=definite,possible \
             --track-origins=yes \
             --log-file="$log" --error-exitcode=1 $cmd > /dev/null 2>&1
    return $?
}

# 1. Integration Tests (VIX and VIM)
echo -e "\n${BLUE}>>> Analyzing Integration Tests (.in files)${NC}"
INTEGRATION_TESTS=$(find test/vix test/vim -name "*.in" | sort)
for t in $INTEGRATION_TESTS; do
    ((TOTAL_TESTS++))
    ((INT_COUNT++))
    # Create unique log name by replacing slashes with underscores
    LOG_NAME=$(echo "$t" | tr '/' '_')
    LOG="$LOG_DIR/${LOG_NAME}.log"
    printf "Analyzing %-55s " "$t"
    
    # Export VIX_PATH to load the correct vixrc.lua
    export VIX_PATH=$(dirname "$t")
    
    if run_valgrind "./vix -headless +qall! $t" "$LOG"; then
        echo -e "${GREEN}OK${NC}"
        ((TOTAL_PASSED++))
        ((INT_PASSED++))
        rm "$LOG"
    else
        echo -e "${RED}LEAK/ERROR${NC}"
        ((TOTAL_FAILED++))
        ((INT_FAILED++))
        FAILED_LIST+=("Integration: $t")
    fi
done

# 2. Unit Tests (C Binaries: core, util, sam)
echo -e "\n${BLUE}>>> Analyzing Unit Tests (C Binaries)${NC}"
UNIT_TESTS=$(find test/core test/util test/sam -executable -type f ! -name "*.sh" ! -name "*.lua" ! -name "keys" | sort)
for t in $UNIT_TESTS; do
    ((TOTAL_TESTS++))
    ((UNIT_COUNT++))
    LOG_NAME=$(echo "$t" | tr '/' '_')
    LOG="$LOG_DIR/unit_${LOG_NAME}.log"
    printf "Analyzing %-55s " "$t"
    if run_valgrind "./$t" "$LOG"; then
        echo -e "${GREEN}OK${NC}"
        ((TOTAL_PASSED++))
        ((UNIT_PASSED++))
        rm "$LOG"
    else
        echo -e "${RED}LEAK/ERROR${NC}"
        ((TOTAL_FAILED++))
        ((UNIT_FAILED++))
        FAILED_LIST+=("Unit: $t")
    fi
done

# 3. Specific Lua Tests (test/lua folder)
echo -e "\n${BLUE}>>> Analyzing Lua Tests (test/lua/*.lua)${NC}"
LUA_TESTS=$(find test/lua -name "*.lua" ! -name "vixrc.lua" | sort)
for t in $LUA_TESTS; do
    ((TOTAL_TESTS++))
    ((LUA_COUNT++))
    LOG_NAME=$(echo "$t" | tr '/' '_')
    LOG="$LOG_DIR/lua_${LOG_NAME}.log"
    printf "Analyzing %-55s " "$t"
    
    # Export VIX_PATH to test/lua to use the test vixrc.lua which loads the .lua file
    export VIX_PATH="test/lua"
    
    # Run vix with the .in file (and ensure it exits with +qall!)
    IN_FILE="${t%.lua}.in"
    if [ ! -f "$IN_FILE" ]; then
        touch "$IN_FILE"
        run_valgrind "./vix -headless +qall! $IN_FILE" "$LOG"
        ST=$?
        rm "$IN_FILE"
    else
        run_valgrind "./vix -headless +qall! $IN_FILE" "$LOG"
        ST=$?
    fi

    if [ $ST -eq 0 ]; then
        echo -e "${GREEN}OK${NC}"
        ((TOTAL_PASSED++))
        ((LUA_PASSED++))
        rm "$LOG"
    else
        echo -e "${RED}LEAK/ERROR${NC}"
        ((TOTAL_FAILED++))
        ((LUA_FAILED++))
        FAILED_LIST+=("Lua: $t")
    fi
done

# --- Final Statistics Report ---
echo -e "\n${YELLOW}${BOLD}================================================================${NC}"
echo -e "${YELLOW}${BOLD}                 VALGRIND MEMORY LEAK REPORT                    ${NC}"
echo -e "${YELLOW}${BOLD}================================================================${NC}"
printf "%-30s | %8s | %8s | %8s\n" "Test Suite" "Total" "Passed" "Failed"
echo "----------------------------------------------------------------"
printf "%-30s | %8d | %8d | %8d\n" "Integration (VIX/VIM)" "$INT_COUNT" "$INT_PASSED" "$INT_FAILED"
printf "%-30s | %8d | %8d | %8d\n" "Unit (C Binaries)" "$UNIT_COUNT" "$UNIT_PASSED" "$UNIT_FAILED"
printf "%-30s | %8d | %8d | %8d\n" "Lua Extensions" "$LUA_COUNT" "$LUA_PASSED" "$LUA_FAILED"
echo "----------------------------------------------------------------"
printf "${BOLD}%-30s | %8d | %8d | %8d${NC}\n" "OVERALL" "$TOTAL_TESTS" "$TOTAL_PASSED" "$TOTAL_FAILED"
echo -e "${YELLOW}${BOLD}================================================================${NC}"

if [ ${#FAILED_LIST[@]} -ne 0 ]; then
    echo -e "\n${RED}${BOLD}FAILED TESTS:${NC}"
    for fail in "${FAILED_LIST[@]}"; do
        echo -e "  ${RED}✖ $fail${NC}"
    done
    echo -e "\n${RED}Check detailed logs in: $LOG_DIR/${NC}"
    exit 1
else
    echo -e "\n${GREEN}${BOLD}✔ SUCCESS: ALL TESTS PASSED WITHOUT CRITICAL LEAKS!${NC}"
    rmdir "$LOG_DIR"
    exit 0
fi

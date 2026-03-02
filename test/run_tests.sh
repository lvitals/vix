#!/bin/bash

# --- Vix Professional Test Orchestrator ---

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Flags
DEBUG=0
[ "$1" == "-d" ] || [ "$1" == "--debug" ] && DEBUG=1

# Stats
PASSED=0
FAILED=0
TOTAL=0
FAILED_LIST=()
LOG_FILE="/tmp/vix_test_debug.log"

# Header
echo -e "${BOLD}${MAGENTA}================================================================${NC}"
echo -e "${BOLD}${MAGENTA}                 VIX EDITOR - TEST INFRASTRUCTURE               ${NC}"
echo -e "${BOLD}${MAGENTA}================================================================${NC}"
[ $DEBUG -eq 1 ] && echo -e "${YELLOW}DEBUG MODE ENABLED - Streaming full output${NC}"
echo -e "OS: $(uname -s) | Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""

# Spinner Function
show_spinner() {
    local pid=$1
    local delay=0.1
    local spinstr='|/-\'
    while [ "$(ps -p $pid -o state= 2>/dev/null)" ]; do
        local temp=${spinstr#?}
        printf " [%c]  " "$spinstr"
        local spinstr=$temp${spinstr%"$temp"}
        sleep $delay
        printf "\b\b\b\b\b\b"
    done
    printf "    \b\b\b\b"
}

# Function to run a test suite
run_suite() {
    local name=$1
    local description=$2
    local dir=$3
    local target=$4
    
    echo -e "${BOLD}${BLUE}>> STEP: $name${NC}"
    echo -e "${CYAN}Testing: $description${NC}"
    
    ((TOTAL++))
    
    local status=0

    if [ $DEBUG -eq 1 ]; then
        # Debug mode: Direct execution
        make -C "$dir" "$target"
        status=$?
    else
        # Normal mode: Background execution with Spinner
        echo -n "Processing... "
        make -C "$dir" "$target" > "$LOG_FILE" 2>&1 &
        local pid=$!
        
        # Start spinner animation
        show_spinner $pid
        
        # Get exit status
        wait $pid
        status=$?
    fi

    if [ $status -eq 0 ]; then
        echo -e "${GREEN}✓ $name PASSED${NC}\n"
        ((PASSED++))
    else
        echo -e "${RED}✗ $name FAILED${NC}"
        if [ $DEBUG -eq 0 ]; then
            echo -e "${YELLOW}Last output from $name:${NC}"
            tail -n 10 "$LOG_FILE"
            echo -e "${CYAN}Tip: Run 'make test DEBUG=1' for full logs.${NC}"
        fi
        echo ""
        ((FAILED++))
        FAILED_LIST+=("$name")
        
        # Critical failure for core
        if [ "$name" == "CORE_UNIT" ]; then
            echo -e "${RED}${BOLD}FATAL: Core tests failed. Aborting.${NC}"
            display_summary
            exit 1
        fi
    fi
}

display_summary() {
    echo -e "${BOLD}${YELLOW}================================================================${NC}"
    echo -e "${BOLD}${YELLOW}                     FINAL TEST SUMMARY                         ${NC}"
    echo -e "${BOLD}${YELLOW}================================================================${NC}"
    
    printf "%-30s | %s\n" "TEST SUITE" "STATUS"
    echo "----------------------------------------------------------------"
    
    local suites=("CORE_UNIT" "UTIL_TOOLS" "LUA_API" "VIX_FUNCTIONAL" "VIM_COMPAT" "SAM_REGEX")
    for s in "${suites[@]}"; do
        if [[ " ${FAILED_LIST[@]} " =~ " ${s} " ]]; then
            printf "%-30s | ${RED}FAILED${NC}\n" "$s"
        else
            printf "%-30s | ${GREEN}PASSED${NC}\n" "$s"
        fi
    done

    echo -e "${BOLD}${YELLOW}----------------------------------------------------------------${NC}"
    if [ $FAILED -eq 0 ]; then
        echo -e "${BOLD}${GREEN}✔ SUCCESS: ALL $PASSED/$TOTAL SYSTEMS OPERATIONAL${NC}"
    else
        echo -e "${BOLD}${RED}✘ FAILURE: $FAILED/$TOTAL SUITES FAILED${NC}"
    fi
    echo -e "${BOLD}${YELLOW}================================================================${NC}"
}

# Execution Flow
run_suite "CORE_UNIT" "Low-level C structures (Buffer/Map/Text)" "core" "test"
run_suite "UTIL_TOOLS" "Internal helper utilities (Keys Translator)" "util" "keys"
run_suite "LUA_API" "Lua extension engine and API bindings" "lua" "test"
run_suite "VIX_FUNCTIONAL" "Vix specific features and multi-cursors" "vix" "test"
run_suite "VIM_COMPAT" "Vim parity and standard editing motions" "vim" "test"
run_suite "SAM_REGEX" "Sam structural regular expressions" "sam" "test"

display_summary

[ $FAILED -eq 0 ]

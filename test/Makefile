# --- Vix Test Makefile Entry Point ---

# make test DEBUG=1
# Check if DEBUG is set in the environment or command line
DEBUG_FLAG =
ifeq ($(DEBUG),1)
	DEBUG_FLAG = --debug
endif

test:
	@./run_tests.sh $(DEBUG_FLAG)

perf:
	@echo ">> STEP: PERFORMANCE BENCHMARK"
	@./perf_test.sh

check_leaks:
	@echo ">> STEP: MEMORY LEAK ANALYSIS (VALGRIND)"
	@./check_leaks.sh

clean:
	@echo "Cleaning all test artifacts..."
	@$(MAKE) -C core clean > /dev/null
	@$(MAKE) -C fuzz clean > /dev/null
	@$(MAKE) -C sam clean > /dev/null
	@$(MAKE) -C lua clean > /dev/null
	@$(MAKE) -C util clean > /dev/null
	@$(MAKE) -C vix clean > /dev/null
	@$(MAKE) -C vim clean > /dev/null
	@rm -f *.out *.err *.status

.PHONY: test clean perf check_leaks

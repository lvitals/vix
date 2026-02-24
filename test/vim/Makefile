test: ../../vix ../util/keys
	@./test.sh

../../vix: ../../*.[ch]
	@echo Compiling vix
	@$(MAKE) -C ../..

../util/keys: ../util/keys.c
	@$(MAKE) -C ../util

clean:
	@echo cleaning
	@find . -name '*.out' -o -name '*.err' | xargs rm -f

.PHONY: clean test

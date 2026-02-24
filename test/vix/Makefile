test: ../../vix clean
	@./test.sh

../../vix: ../../*.[ch]
	@echo Compiling vix
	@$(MAKE) -C ../..

clean:
	@echo cleaning
	@find . -name '*.out' -o -name '*.err' | xargs rm -f

.PHONY: clean test

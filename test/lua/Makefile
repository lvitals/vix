all: clean ../../vix test

../../vix: ../../*.[ch]
	@echo Compiling vix
	@$(MAKE) -C ../..

clean:
	@rm -f *.out *.err

test:
	@./test.sh

%: %.in
	@./test.sh $@

.PHONY: all clean test

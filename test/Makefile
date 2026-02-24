test:
	@$(MAKE) -C core
	@$(MAKE) -C lua
	@$(MAKE) -C vix
	@$(MAKE) -C sam
	@$(MAKE) -C vim

clean:
	@$(MAKE) -C core clean
	@$(MAKE) -C lua clean
	@$(MAKE) -C vix clean
	@$(MAKE) -C sam clean
	@$(MAKE) -C vim clean
	@$(MAKE) -C util clean

.PHONY: test clean

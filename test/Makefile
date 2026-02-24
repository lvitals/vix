test:
	@$(MAKE) -C core
	@$(MAKE) -C fuzz
	@$(MAKE) -C sam
	@$(MAKE) -C lua
	@$(MAKE) -C util
	@$(MAKE) -C vix
	@$(MAKE) -C vim
	
clean:
	@$(MAKE) -C core clean
	@$(MAKE) -C fuzz clean
	@$(MAKE) -C sam clean
	@$(MAKE) -C lua clean
	@$(MAKE) -C util clean
	@$(MAKE) -C vix clean
	@$(MAKE) -C vim clean

.PHONY: test clean

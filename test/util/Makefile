PKG_CONFIG ?= pkg-config
LIBS_TERMKEY := $(shell $(PKG_CONFIG) --libs termkey || echo "-ltermkey")
INCS_TERMKEY := $(shell $(PKG_CONFIG) --cflags termkey || echo "")

keys: keys.c
	@echo Compiling keys utility
	$(CC) $(CFLAGS) $(INCS_TERMKEY) keys.c $(LDFLAGS) $(LIBS_TERMKEY) -o keys

clean:
	@echo cleaning
	@rm -f keys

.PHONY: clean

CFLAGS += -I../..

keys: keys.c ../../termkey.c ../../termkey.h
	@echo Compiling keys utility
	$(CC) $(CFLAGS) keys.c ../../termkey.c $(LDFLAGS) -o keys

clean:
	@echo cleaning
	@rm -f keys

.PHONY: clean

# vix - vi-like editor based on Plan 9's structural regular expressions
VERSION = 0.1

# --- Paths ---
PREFIX      ?= /usr/local
MANPREFIX    = ${PREFIX}/share/man
SHAREPREFIX  = ${PREFIX}/share
VIX_PATH     = ${SHAREPREFIX}/vix

# --- Features (0 to disable, 1 to enable) ---
ENABLE_LUA     ?= 1
ENABLE_TRE     ?= 0
ENABLE_ACL     ?= 0
ENABLE_SELINUX ?= 0
ENABLE_CURSES  ?= 0
DEBUG          ?= 0
ASAN           ?= 0

# --- Dependencies & pkg-config ---
PKG_CONFIG ?= pkg-config

# Standard system libs
INCS = -I.
LIBS = -lc -lm

# Essential TUI dependencies
ifeq ($(ENABLE_CURSES), 1)
TUI_CFLAGS := $(shell $(PKG_CONFIG) --cflags termkey ncursesw || echo "-I/usr/include")
TUI_LIBS   := $(shell $(PKG_CONFIG) --libs termkey ncursesw || echo "-ltermkey -lncursesw")
else
TUI_CFLAGS := $(shell $(PKG_CONFIG) --cflags termkey || echo "-I/usr/include")
TUI_LIBS   := $(shell $(PKG_CONFIG) --libs termkey || echo "-ltermkey")
endif
INCS      += ${TUI_CFLAGS}
LIBS      += ${TUI_LIBS}

# Lua support
ifeq ($(ENABLE_LUA), 1)
LUA_PKG ?= $(shell $(PKG_CONFIG) --exists lua5.4 && echo lua5.4 || echo lua)
ifeq ($(strip $(LUA_PKG)),)
LUA_PKG := lua
endif
LUA_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(LUA_PKG))
LUA_LIBS   := $(shell $(PKG_CONFIG) --libs $(LUA_PKG))
INCS      += ${LUA_CFLAGS}
LIBS      += ${LUA_LIBS}
endif

# TRE support
ifeq ($(ENABLE_TRE), 1)
TRE_CFLAGS := $(shell $(PKG_CONFIG) --cflags tre)
TRE_LIBS   := $(shell $(PKG_CONFIG) --libs tre)
INCS      += ${TRE_CFLAGS}
LIBS      += ${TRE_LIBS}
endif

# --- Toolchain & Flags ---
CC       ?= cc
CPPFLAGS  = -DVERSION='"$(VERSION)"' \
            -D_POSIX_C_SOURCE=200809L \
            -DCONFIG_CURSES=$(ENABLE_CURSES) \
            -DCONFIG_HELP=1 \
            -DCONFIG_LUA=$(ENABLE_LUA) \
            -DCONFIG_TRE=$(ENABLE_TRE) \
            -DVIX_PATH='"$(VIX_PATH)"'

CFLAGS   += -std=c99 -pedantic -Wall ${INCS} ${CPPFLAGS}
LDFLAGS  += ${LIBS}

ifeq ($(DEBUG), 1)
CFLAGS   += -g -O0
else
CFLAGS   += -Os
endif

ifeq ($(ASAN), 1)
CFLAGS   += -fsanitize=address -fno-omit-frame-pointer
LDFLAGS  += -fsanitize=address
endif

# Export variables for sub-makes (tests)
export CC CFLAGS LDFLAGS

# --- Target Files ---
ELF           = vix vix-menu vix-digraph
MANUALS       = vix.1 vix-menu.1 vix-digraph.1
DOCUMENTATION = LICENSE README.md

# --- Build Rules ---
all: config.h $(ELF)

config.h:
	cp config.def.h config.h

vix: main.c config.h
	${CC} ${CFLAGS} main.c ${LDFLAGS} -o $@

vix-menu: vix-menu.c
	${CC} ${CFLAGS} $< ${LDFLAGS} -o $@

vix-digraph: vix-digraph.c
	${CC} ${CFLAGS} $< ${LDFLAGS} -o $@

test: all
	@$(MAKE) -C test

clean:
	rm -f $(ELF) config.h
	@$(MAKE) -C test clean

install: all
	@echo "Installing bin to ${DESTDIR}${PREFIX}/bin"
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@for e in ${ELF}; do cp -f "$$e" ${DESTDIR}${PREFIX}/bin && chmod 755 ${DESTDIR}${PREFIX}/bin/"$$e"; done
	cp -f vix-clipboard ${DESTDIR}${PREFIX}/bin/vix-clipboard
	cp -f vix-complete ${DESTDIR}${PREFIX}/bin/vix-complete
	cp -f vix-open ${DESTDIR}${PREFIX}/bin/vix-open
	@echo "Installing man to ${DESTDIR}${MANPREFIX}/man1"
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@for m in ${MANUALS}; do cp "man/$$m" "${DESTDIR}${MANPREFIX}/man1/$$m" && chmod 644 "${DESTDIR}${MANPREFIX}/man1/$$m"; done
	@echo "Installing lua scripts to ${DESTDIR}${VIX_PATH}"
	@mkdir -p ${DESTDIR}${VIX_PATH}
	@cp -r lua/* ${DESTDIR}${VIX_PATH}

uninstall:
	@for e in ${ELF} vix-clipboard vix-complete vix-open; do rm -f ${DESTDIR}${PREFIX}/bin/"$$e"; done
	@for m in ${MANUALS}; do rm -f ${DESTDIR}${MANPREFIX}/man1/"$$m"; done
	rm -rf ${DESTDIR}${VIX_PATH}

.PHONY: all clean install uninstall test

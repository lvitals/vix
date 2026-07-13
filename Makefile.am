AUTOMAKE_OPTIONS = foreign

vixdatadir = $(datadir)/vix

AM_CPPFLAGS = \
	-I$(srcdir) \
	-DVERSION='"$(PACKAGE_VERSION)"' \
	-D_POSIX_C_SOURCE=200809L \
	-DCONFIG_CURSES=$(CONFIG_CURSES) \
	-DCONFIG_HELP=1 \
	-DCONFIG_LUA=$(CONFIG_LUA) \
	-DCONFIG_LPEG=$(CONFIG_LPEG) \
	-DCONFIG_TRE=$(CONFIG_TRE) \
	-DCONFIG_ACL=$(CONFIG_ACL) \
	-DCONFIG_SELINUX=$(CONFIG_SELINUX) \
	-DVIX_PATH='"$(vixdatadir)"'

AM_CFLAGS = \
	-std=c99 \
	-pedantic \
	-Wall \
	$(OPT_CFLAGS) \
	$(ASAN_CFLAGS) \
	$(NCURSESW_CFLAGS) \
	$(LUA_CFLAGS) \
	$(TRE_CFLAGS)

AM_LDFLAGS = $(ASAN_LDFLAGS)

LDADD = \
	$(NCURSESW_LIBS) \
	$(LUA_LIBS) \
	$(LPEG_LIBS) \
	$(TRE_LIBS) \
	$(ACL_LIBS) \
	$(SELINUX_LIBS) \
	-lm

bin_PROGRAMS = vix vix-menu vix-digraph
dist_bin_SCRIPTS = vix-clipboard vix-complete vix-open

vix_SOURCES = main.c termkey.c termkey.h
vix_menu_SOURCES = vix-menu.c
vix_digraph_SOURCES = vix-digraph.c

BUILT_SOURCES = config.h
CLEANFILES = config.h

config.h:
	cp $(srcdir)/config.def.h $@

main.$(OBJEXT): config.h

man_MANS = \
	man/vix.1 \
	man/vix-menu.1 \
	man/vix-digraph.1 \
	man/vix-clipboard.1 \
	man/vix-complete.1 \
	man/vix-open.1

EXTRA_DIST = \
	LICENSE \
	config.def.h \
	docs \
	lua \
	man \
	test

install-data-local:
	$(MKDIR_P) "$(DESTDIR)$(vixdatadir)"
	cp -R "$(srcdir)/lua/." "$(DESTDIR)$(vixdatadir)"

uninstall-local:
	rm -rf "$(DESTDIR)$(vixdatadir)"

test: all
	$(MAKE) -C "$(srcdir)/test" DEBUG=$(if $(findstring -g -O0,$(OPT_CFLAGS)),1,0) test

check-local: test

perf: all
	$(MAKE) -C "$(srcdir)/test" perf

check_leaks: all
	$(MAKE) -C "$(srcdir)/test" check_leaks

clean-local:
	$(MAKE) -C "$(srcdir)/test" clean

.PHONY: test perf check_leaks

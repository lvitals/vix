AC_PREREQ([2.69])
AC_INIT([vix], [0.1], [https://github.com/vix-editor/vix/issues])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

AC_ARG_ENABLE([lua],
	AS_HELP_STRING([--disable-lua], [disable Lua support]),
	[], [enable_lua=yes])
AC_ARG_WITH([lua-pkg],
	AS_HELP_STRING([--with-lua-pkg=NAME], [pkg-config module to use for Lua]),
	[lua_pkg=$withval], [lua_pkg=lua])

AC_ARG_ENABLE([tre],
	AS_HELP_STRING([--disable-tre], [disable TRE regular expression support]),
	[], [enable_tre=yes])

AC_ARG_ENABLE([curses],
	AS_HELP_STRING([--enable-curses], [enable ncursesw terminal backend]),
	[], [enable_curses=no])

AC_ARG_ENABLE([acl],
	AS_HELP_STRING([--enable-acl], [enable POSIX ACL support]),
	[], [enable_acl=no])

AC_ARG_ENABLE([selinux],
	AS_HELP_STRING([--enable-selinux], [enable SELinux support]),
	[], [enable_selinux=no])

AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug], [build with debug flags]),
	[], [enable_debug=no])

AC_ARG_ENABLE([asan],
	AS_HELP_STRING([--enable-asan], [build with AddressSanitizer]),
	[], [enable_asan=no])

CONFIG_CURSES=0
AS_IF([test "x$enable_curses" = xyes], [
	PKG_CHECK_MODULES([NCURSESW], [ncursesw])
	CONFIG_CURSES=1
])

CONFIG_LUA=0
CONFIG_LPEG=0
AS_IF([test "x$enable_lua" = xyes], [
	PKG_CHECK_MODULES([LUA], [$lua_pkg])
	AC_PATH_PROGS([LUA_INTERPRETER], [lua lua5.5 lua5.4 lua5.3 lua5.2 lua5.1])
	AS_IF([test "x$LUA_INTERPRETER" = x], [
		AC_MSG_ERROR([Lua support requires a Lua interpreter for configure-time module checks.
Install the Lua interpreter package matching the Lua development package, or rerun configure with --disable-lua.])
	])
	AC_MSG_CHECKING([for Lua module lpeg])
	AS_IF([$LUA_INTERPRETER -e 'require("lpeg")' >/dev/null 2>&1], [
		AC_MSG_RESULT([yes])
	], [
		AC_MSG_RESULT([no])
		AC_MSG_ERROR([Lua support requires the LPeg module for the same Lua version used by this build.
Install the package that provides require("lpeg") for your Lua interpreter.
Common package names include lua-lpeg, lua5.4-lpeg, lua54-lpeg, lua53-lpeg, lua52-lpeg, or lua51-lpeg.
If Lua support is not needed, rerun configure with --disable-lua.])
	])

	AC_MSG_CHECKING([whether luaopen_lpeg links with Lua alone])
	vix_save_LIBS=$LIBS
	LIBS="$LUA_LIBS $LIBS"
	AC_LINK_IFELSE(
		[AC_LANG_PROGRAM([[extern int luaopen_lpeg(void *);]], [[return luaopen_lpeg(0);]])],
		[AC_MSG_RESULT([yes]); LPEG_LIBS=],
		[
			AC_MSG_RESULT([no])
			AC_MSG_CHECKING([for LPeg shared object])
			LPEG_LIBS=`$LUA_INTERPRETER -e 'local p = package.searchpath("lpeg", package.cpath); if p then io.write(p) end' 2>/dev/null`
			AS_IF([test "x$LPEG_LIBS" = x], [
				AC_MSG_RESULT([no])
				AC_MSG_ERROR([Lua support found require("lpeg"), but configure could not locate the LPeg shared object needed for linking.
Install LPeg development/link files for the selected Lua version, or rerun configure with --disable-lua.])
			])
			AC_MSG_RESULT([$LPEG_LIBS])
			LIBS="$LPEG_LIBS $LUA_LIBS $vix_save_LIBS"
			AC_MSG_CHECKING([whether luaopen_lpeg links from LPeg])
			AC_LINK_IFELSE(
				[AC_LANG_PROGRAM([[extern int luaopen_lpeg(void *);]], [[return luaopen_lpeg(0);]])],
				[AC_MSG_RESULT([yes])],
				[
					AC_MSG_RESULT([no])
					AC_MSG_ERROR([The LPeg module was found, but the build could not link luaopen_lpeg from it.
Install LPeg development/link files for the selected Lua version, or rerun configure with --disable-lua.])
				])
		])
	LIBS=$vix_save_LIBS
	CONFIG_LUA=1
	CONFIG_LPEG=1
])

CONFIG_TRE=0
AS_IF([test "x$enable_tre" = xyes], [
	PKG_CHECK_MODULES([TRE], [tre], [], [
		AC_MSG_ERROR([TRE regular expression support requires the TRE development package.
Install the package that provides both the header tre/tre.h and the library libtre.
Common package names include tre, libtre-dev, tre-devel, or libtre-devel.
If TRE support is not needed, rerun configure with --disable-tre.])
	])
	CONFIG_TRE=1
])

CONFIG_ACL=0
ACL_LIBS=
AS_IF([test "x$enable_acl" = xyes], [
	AC_CHECK_HEADERS([sys/acl.h], [],
		[AC_MSG_ERROR([sys/acl.h is required for --enable-acl])])
	AC_SEARCH_LIBS([acl_get_file], [acl], [],
		[AC_MSG_ERROR([libacl is required for --enable-acl])])
	ACL_LIBS="$ac_cv_search_acl_get_file"
	AS_IF([test "x$ACL_LIBS" = "xnone required"], [ACL_LIBS=])
	CONFIG_ACL=1
])

CONFIG_SELINUX=0
SELINUX_LIBS=
AS_IF([test "x$enable_selinux" = xyes], [
	AC_CHECK_HEADERS([selinux/selinux.h], [],
		[AC_MSG_ERROR([selinux/selinux.h is required for --enable-selinux])])
	AC_SEARCH_LIBS([getfilecon], [selinux], [],
		[AC_MSG_ERROR([libselinux is required for --enable-selinux])])
	SELINUX_LIBS="$ac_cv_search_getfilecon"
	AS_IF([test "x$SELINUX_LIBS" = "xnone required"], [SELINUX_LIBS=])
	CONFIG_SELINUX=1
])

OPT_CFLAGS=-Os
AS_IF([test "x$enable_debug" = xyes], [OPT_CFLAGS="-g -O0"])

ASAN_CFLAGS=
ASAN_LDFLAGS=
AS_IF([test "x$enable_asan" = xyes], [
	ASAN_CFLAGS="-fsanitize=address -fno-omit-frame-pointer"
	ASAN_LDFLAGS="-fsanitize=address"
])

AC_SUBST([CONFIG_CURSES])
AC_SUBST([CONFIG_LUA])
AC_SUBST([CONFIG_LPEG])
AC_SUBST([CONFIG_TRE])
AC_SUBST([LPEG_LIBS])
AC_SUBST([CONFIG_ACL])
AC_SUBST([CONFIG_SELINUX])
AC_SUBST([ACL_LIBS])
AC_SUBST([SELINUX_LIBS])
AC_SUBST([OPT_CFLAGS])
AC_SUBST([ASAN_CFLAGS])
AC_SUBST([ASAN_LDFLAGS])

AC_OUTPUT

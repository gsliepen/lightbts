origcxxflags="$CXXFLAGS"
AC_PREREQ(2.61)
AC_INIT([LightBTS], [0.1])
AM_INIT_AUTOMAKE([1.11 std-options subdir-objects nostdinc silent-rules -Wall])
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])

AM_PROG_AR
AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_INSTALL
AC_PROG_LIBTOOL

PKG_INSTALLDIR

AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_11

AX_CODE_COVERAGE
AS_IF([test "x$enable_code_coverage" = "xyes" -a "x$origcxxflags" = "x"], [CXXFLAGS=""])

AX_BOOST_BASE
AX_BOOST_SYSTEM
AX_BOOST_FILESYSTEM

PKG_CHECK_MODULES([mimesis], [mimesis])
PKG_CHECK_MODULES([sqlite3], [sqlite3])

AX_CHECK_LIBRARY([b2], [blake2.h], [b2], [], [AC_MSG_ERROR([Missing required library libb2])])
AX_CHECK_LIBRARY([fmt3], [fmt/format.h], [fmt], [], [AC_MSG_ERROR([Missing required library libfmt])])

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile test/Makefile])
AC_OUTPUT

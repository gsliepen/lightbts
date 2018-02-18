AUTOMAKE_OPTIONS = gnu

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = templates

CODE_COVERAGE_LCOV_OPTIONS=--no-external --quiet
@CODE_COVERAGE_RULES@

SUBDIRS = src test

ChangeLog:
	git log >ChangeLog

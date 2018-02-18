bin_PROGRAMS = lbts
noinst_PROGRAMS = gentemplates

lbts_SOURCES = \
	cli.cpp cli.hpp \
	create.cpp create.hpp \
	edit.cpp edit.hpp \
	list.cpp list.hpp \
	lightbts.cpp lightbts.hpp \
	config.cpp config.hpp \
	show.cpp show.hpp \
	pager.cpp pager.hpp \
	sqlite3.hpp

BUILT_SOURCES = \
	templates.inl

lbts_LDADD = \
	$(mimesis_LIBS) \
	$(sqlite3_LIBS) \
	$(BOOST_SYSTEM_LIB) $(BOOST_FILESYSTEM_LIB) \
	$(b2_LDFLAGS) -lb2 \
	$(fmt3_LDFLAGS) -lfmt \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
	$(mimesis_CFLAGS) \
	$(sqlite3_CFLAGS) \
	$(BOOST_CPPFLAGS) \
	$(b2_CPPFLAGS) \
	$(fmt3_CPPFLAGS) \
	-iquote $(builddir) \
	$(CODE_COVERAGE_CPPFLAGS)

AM_CXXFLAGS = \
	$(CODE_COVERAGE_CXXFLAGS)

gentemplates_SOURCES = \
	gentemplates.cpp

templates.inl: gentemplates $(top_srcdir)/templates/*
	$(AM_V_GEN)./gentemplates $(top_srcdir)/templates/*

MOSTLYCLEANFILES = \
	templates.inl

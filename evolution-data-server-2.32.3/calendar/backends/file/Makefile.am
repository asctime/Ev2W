extension_LTLIBRARIES = libecalbackendfile.la

libecalbackendfile_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"libecalbackendfile\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/calendar			\
	-I$(top_builddir)/calendar			\
	$(EVOLUTION_CALENDAR_CFLAGS)

noinst_PROGRAMS = test-interval-searches

libecalbackendfile_la_SOURCES =		\
	e-cal-backend-file-factory.c	\
	e-cal-backend-file-factory.h	\
	e-cal-backend-file-events.c	\
	e-cal-backend-file-events.h	\
	e-cal-backend-file-journal.c	\
	e-cal-backend-file-journal.h	\
	e-cal-backend-file-todos.c	\
	e-cal-backend-file-todos.h	\
	e-cal-backend-file.c		\
	e-cal-backend-file.h

libecalbackendfile_la_LIBADD =							\
	$(top_builddir)/calendar/libecal/libecal-1.2.la				\
	$(top_builddir)/calendar/libedata-cal/libedata-cal-1.2.la		\
	$(top_builddir)/libedataserver/libedataserver-1.2.la			\
	$(EVOLUTION_CALENDAR_LIBS)

libecalbackendfile_la_CFLAGS =	$(AM_CFLAGS)

libecalbackendfile_la_LDFLAGS =		\
	-module -avoid-version $(NO_UNDEFINED)

test_interval_searches_SOURCES = e-cal-backend-file.c

test_interval_searches_LDADD = \
	$(top_builddir)/calendar/libecal/libecal-1.2.la				\
	$(top_builddir)/calendar/libedata-cal/libedata-cal-1.2.la		\
	$(top_builddir)/libedataserver/libedataserver-1.2.la			\
	$(EVOLUTION_CALENDAR_LIBS)

test_interval_searches_CPPFLAGS = \
	$(AM_CPPFLAGS)			\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/calendar	\
	$(EVOLUTION_CALENDAR_CFLAGS)	\
	-DTEST_QUERY_RESULT=1

-include $(top_srcdir)/git.mk

extension_LTLIBRARIES = libebookbackendfile.la

libebookbackendfile_la_CPPFLAGS = \
	$(AM_CPPFLAGS)					\
	-DG_LOG_DOMAIN=\"libebookbackend\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/addressbook			\
	-I$(top_builddir)/addressbook			\
	$(DB_CFLAGS)					\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

libebookbackendfile_la_SOURCES =			\
	e-book-backend-file.c				\
	e-book-backend-file.h				\
	e-book-backend-file-factory.c

libebookbackendfile_la_LIBADD =						\
	$(top_builddir)/addressbook/libebook/libebook-1.2.la		\
	$(top_builddir)/addressbook/libedata-book/libedata-book-1.2.la	\
	$(top_builddir)/libedataserver/libedataserver-1.2.la		\
	$(top_builddir)/libebackend/libebackend-1.2.la			\
	$(DB_LIBS)							\
	$(EVOLUTION_ADDRESSBOOK_LIBS)

libebookbackendfile_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

-include $(top_srcdir)/git.mk

LIBS = $(SHELL_LIBS)

@EVO_PLUGIN_RULE@

plugin_DATA = org-gnome-evolution-profiler.eplug

plugin_LTLIBRARIES = liborg-gnome-evolution-profiler.la

liborg_gnome_evolution_profiler_la_CPPFLAGS =		\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)					\
	$(SHELL_CFLAGS)

liborg_gnome_evolution_profiler_la_SOURCES = profiler.c

liborg_gnome_evolution_profiler_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

EXTRA_DIST = org-gnome-evolution-profiler.eplug.xml

BUILT_SOURCES = $(plugin_DATA)
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

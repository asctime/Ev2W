# Makefile.in generated by automake 1.16.3 from Makefile.am.
# Makefile.  Generated from Makefile.in by configure.

# Copyright (C) 1994-2020 Free Software Foundation, Inc.

# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.


VPATH = ..
am__is_gnu_make = { \
  if test -z '$(MAKELEVEL)'; then \
    false; \
  elif test -n '$(MAKE_HOST)'; then \
    true; \
  elif test -n '$(MAKE_VERSION)' && test -n '$(CURDIR)'; then \
    true; \
  else \
    false; \
  fi; \
}
am__make_running_with_option = \
  case $${target_option-} in \
      ?) ;; \
      *) echo "am__make_running_with_option: internal error: invalid" \
              "target option '$${target_option-}' specified" >&2; \
         exit 1;; \
  esac; \
  has_opt=no; \
  sane_makeflags=$$MAKEFLAGS; \
  if $(am__is_gnu_make); then \
    sane_makeflags=$$MFLAGS; \
  else \
    case $$MAKEFLAGS in \
      *\\[\ \	]*) \
        bs=\\; \
        sane_makeflags=`printf '%s\n' "$$MAKEFLAGS" \
          | sed "s/$$bs$$bs[$$bs $$bs	]*//g"`;; \
    esac; \
  fi; \
  skip_next=no; \
  strip_trailopt () \
  { \
    flg=`printf '%s\n' "$$flg" | sed "s/$$1.*$$//"`; \
  }; \
  for flg in $$sane_makeflags; do \
    test $$skip_next = yes && { skip_next=no; continue; }; \
    case $$flg in \
      *=*|--*) continue;; \
        -*I) strip_trailopt 'I'; skip_next=yes;; \
      -*I?*) strip_trailopt 'I';; \
        -*O) strip_trailopt 'O'; skip_next=yes;; \
      -*O?*) strip_trailopt 'O';; \
        -*l) strip_trailopt 'l'; skip_next=yes;; \
      -*l?*) strip_trailopt 'l';; \
      -[dEDm]) skip_next=yes;; \
      -[JT]) skip_next=yes;; \
    esac; \
    case $$flg in \
      *$$target_option*) has_opt=yes; break;; \
    esac; \
  done; \
  test $$has_opt = yes
am__make_dryrun = (target_option=n; $(am__make_running_with_option))
am__make_keepgoing = (target_option=k; $(am__make_running_with_option))
pkgdatadir = $(datadir)/evolution-rss
pkgincludedir = $(includedir)/evolution-rss
pkglibdir = $(libdir)/evolution-rss
pkglibexecdir = $(libexecdir)/evolution-rss
am__cd = CDPATH="$${ZSH_VERSION+.}$(PATH_SEPARATOR)" && cd
install_sh_DATA = $(install_sh) -c -m 644
install_sh_PROGRAM = $(install_sh) -c
install_sh_SCRIPT = $(install_sh) -c
INSTALL_HEADER = $(INSTALL_DATA)
transform = $(program_transform_name)
NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
build_triplet = x86_64-w64-mingw32
host_triplet = x86_64-w64-mingw32
subdir = .
ACLOCAL_M4 = $(top_srcdir)/aclocal.m4
am__aclocal_m4_deps = $(top_srcdir)/m4/as-compiler-flag.m4 \
	$(top_srcdir)/m4/evolution.m4 $(top_srcdir)/m4/gecko.m4 \
	$(top_srcdir)/m4/intltool.m4 $(top_srcdir)/m4/libtool.m4 \
	$(top_srcdir)/m4/ltoptions.m4 $(top_srcdir)/m4/ltsugar.m4 \
	$(top_srcdir)/m4/ltversion.m4 $(top_srcdir)/m4/lt~obsolete.m4 \
	$(top_srcdir)/configure.ac
am__configure_deps = $(am__aclocal_m4_deps) $(CONFIGURE_DEPENDENCIES) \
	$(ACLOCAL_M4)
DIST_COMMON = $(srcdir)/Makefile.am $(top_srcdir)/configure \
	$(am__configure_deps) $(am__DIST_COMMON)
am__CONFIG_DISTCLEAN_FILES = config.status config.cache config.log \
 configure.lineno config.status.lineno
mkinstalldirs = $(install_sh) -d
CONFIG_HEADER = config.h
CONFIG_CLEAN_FILES = evolution-rss.spec
CONFIG_CLEAN_VPATH_FILES =
AM_V_P = $(am__v_P_$(V))
am__v_P_ = $(am__v_P_$(AM_DEFAULT_VERBOSITY))
am__v_P_0 = false
am__v_P_1 = :
AM_V_GEN = $(am__v_GEN_$(V))
am__v_GEN_ = $(am__v_GEN_$(AM_DEFAULT_VERBOSITY))
am__v_GEN_0 = @echo "  GEN     " $@;
am__v_GEN_1 = 
AM_V_at = $(am__v_at_$(V))
am__v_at_ = $(am__v_at_$(AM_DEFAULT_VERBOSITY))
am__v_at_0 = @
am__v_at_1 = 
SOURCES =
DIST_SOURCES =
RECURSIVE_TARGETS = all-recursive check-recursive cscopelist-recursive \
	ctags-recursive dvi-recursive html-recursive info-recursive \
	install-data-recursive install-dvi-recursive \
	install-exec-recursive install-html-recursive \
	install-info-recursive install-pdf-recursive \
	install-ps-recursive install-recursive installcheck-recursive \
	installdirs-recursive pdf-recursive ps-recursive \
	tags-recursive uninstall-recursive
am__can_run_installinfo = \
  case $$AM_UPDATE_INFO_DIR in \
    n|no|NO) false;; \
    *) (install-info --version) >/dev/null 2>&1;; \
  esac
RECURSIVE_CLEAN_TARGETS = mostlyclean-recursive clean-recursive	\
  distclean-recursive maintainer-clean-recursive
am__recursive_targets = \
  $(RECURSIVE_TARGETS) \
  $(RECURSIVE_CLEAN_TARGETS) \
  $(am__extra_recursive_targets)
AM_RECURSIVE_TARGETS = $(am__recursive_targets:-recursive=) TAGS CTAGS \
	cscope distdir distdir-am dist dist-all distcheck
am__tagged_files = $(HEADERS) $(SOURCES) $(TAGS_FILES) $(LISP) \
	config.h.in
# Read a list of newline-separated strings from the standard input,
# and print each of them once, without duplicates.  Input order is
# *not* preserved.
am__uniquify_input = $(AWK) '\
  BEGIN { nonempty = 0; } \
  { items[$$0] = 1; nonempty = 1; } \
  END { if (nonempty) { for (i in items) print i; }; } \
'
# Make sure the list of sources is unique.  This is necessary because,
# e.g., the same source file might be shared among _SOURCES variables
# for different programs/libraries.
am__define_uniq_tagged_files = \
  list='$(am__tagged_files)'; \
  unique=`for i in $$list; do \
    if test -f "$$i"; then echo $$i; else echo $(srcdir)/$$i; fi; \
  done | $(am__uniquify_input)`
ETAGS = etags
CTAGS = ctags
CSCOPE = cscope
DIST_SUBDIRS = $(SUBDIRS)
am__DIST_COMMON = $(srcdir)/Makefile.in $(srcdir)/config.h.in \
	$(srcdir)/evolution-rss.spec.in AUTHORS COPYING ChangeLog \
	INSTALL NEWS README TODO compile config.guess config.sub \
	depcomp install-sh ltmain.sh missing
DISTFILES = $(DIST_COMMON) $(DIST_SOURCES) $(TEXINFOS) $(EXTRA_DIST)
distdir = $(PACKAGE)-$(VERSION)
top_distdir = $(distdir)
am__remove_distdir = \
  if test -d "$(distdir)"; then \
    find "$(distdir)" -type d ! -perm -200 -exec chmod u+w {} ';' \
      && rm -rf "$(distdir)" \
      || { sleep 5 && rm -rf "$(distdir)"; }; \
  else :; fi
am__post_remove_distdir = $(am__remove_distdir)
am__relativize = \
  dir0=`pwd`; \
  sed_first='s,^\([^/]*\)/.*$$,\1,'; \
  sed_rest='s,^[^/]*/*,,'; \
  sed_last='s,^.*/\([^/]*\)$$,\1,'; \
  sed_butlast='s,/*[^/]*$$,,'; \
  while test -n "$$dir1"; do \
    first=`echo "$$dir1" | sed -e "$$sed_first"`; \
    if test "$$first" != "."; then \
      if test "$$first" = ".."; then \
        dir2=`echo "$$dir0" | sed -e "$$sed_last"`/"$$dir2"; \
        dir0=`echo "$$dir0" | sed -e "$$sed_butlast"`; \
      else \
        first2=`echo "$$dir2" | sed -e "$$sed_first"`; \
        if test "$$first2" = "$$first"; then \
          dir2=`echo "$$dir2" | sed -e "$$sed_rest"`; \
        else \
          dir2="../$$dir2"; \
        fi; \
        dir0="$$dir0"/"$$first"; \
      fi; \
    fi; \
    dir1=`echo "$$dir1" | sed -e "$$sed_rest"`; \
  done; \
  reldir="$$dir2"
DIST_ARCHIVES = $(distdir).tar.gz
GZIP_ENV = --best
DIST_TARGETS = dist-gzip
# Exists only to be overridden by the user if desired.
AM_DISTCHECK_DVI_TARGET = dvi
distuninstallcheck_listfiles = find . -type f -print
am__distuninstallcheck_listfiles = $(distuninstallcheck_listfiles) \
  | sed 's|^\./|$(prefix)/|' | grep -v '$(infodir)/dir$$'
distcleancheck_listfiles = find . -type f -print
ACLOCAL = ${SHELL} '/d/Download/MinGW-64-souorce/evolution-rss-0-2-5/missing' aclocal-1.16
ACLOCAL_AMFLAGS = -I m4
ALL_LINGUAS = 
AMTAR = $${TAR-tar}
AM_DEFAULT_VERBOSITY = 0
AR = ar
AUTOCONF = ${SHELL} '/d/Download/MinGW-64-souorce/evolution-rss-0-2-5/missing' autoconf
AUTOHEADER = ${SHELL} '/d/Download/MinGW-64-souorce/evolution-rss-0-2-5/missing' autoheader
AUTOMAKE = ${SHELL} '/d/Download/MinGW-64-souorce/evolution-rss-0-2-5/missing' automake-1.16
AWK = gawk
BONBONO_LIBDIR = 
CATALOGS = 
CATOBJEXT = .gmo
CC = gcc
CCDEPMODE = depmode=gcc3
CFLAGS = -D__USE_MINGW_ANSI_STDIO=1 -D_GNU_SOURCE -D_POSIX_C_SOURCE -Wno-deprecated -Wno-deprecated-declarations -DGLIB_DISABLE_DEPRECATION_WARNINGS -g -O2 -std=iso9899:1990 -mms-bitfields -pipe -mtune=sandybridge -fomit-frame-pointer -DWIN32 -DWIN64 -D_WIN32_IE=0x0700 -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -MD -MT -fasynchronous-unwind-tables -fexceptions   -DG_DISABLE_DEPRECATED -DPANGO_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE -Wall -Wextra -Wno-missing-field-initializers -Wno-sign-compare -Wno-unused-parameter -Wdeclaration-after-statement -Werror-implicit-function-declaration -Wformat-security -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Wmissing-noreturn -Wnested-externs -Wpointer-arith -Wredundant-decls -Wundef -Wwrite-strings
CPP = gcc -E
CPPFLAGS = -D__USE_MINGW_ANSI_STDIO=1 -D_GNU_SOURCE -D_POSIX_C_SOURCE -Wno-deprecated -Wno-deprecated-declarations -DGLIB_DISABLE_DEPRECATION_WARNINGS -g -O2 -mms-bitfields -pipe -fomit-frame-pointer -mtune=sandybridge -DWIN32 -DWIN64 -D_WIN32_IE=0x0700 -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -D_FORTIFY_SOURCE=2 -MD -MT -fstack-protector-strong -fasynchronous-unwind-tables -fexceptions
CXX = g++
CXXCPP = g++ -E
CXXDEPMODE = depmode=gcc3
CXXFLAGS = -D__USE_MINGW_ANSI_STDIO=1 -D_GNU_SOURCE -D_POSIX_C_SOURCE -D_FORTIFY_SOURCE=2 -Wno-deprecated -Wno-deprecated-declarations -DGLIB_DISABLE_DEPRECATION_WARNINGS -g -O2 -mms-bitfields -pipe -fomit-frame-pointer -mtune=sandybridge -DWIN32 -DWIN64 -D_WIN32_IE=0x0700 -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -MD -MT -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fasynchronous-unwind-tables -fexceptions
CYGPATH_W = cygpath -w
DATADIRNAME = share
DATASERVER_VERSION = 2032003
DBUS_CFLAGS = -ID:/MSYS2/mingw64/bin/../include/dbus-1.0 -ID:/MSYS2/mingw64/bin/../lib/dbus-1.0/include -ID:/MSYS2/mingw64/bin/../include/glib-2.0 -ID:/MSYS2/mingw64/bin/../lib/glib-2.0/include -ID:/MSYS2/mingw64/bin/../include -mms-bitfields -ID:/MSYS2/mingw64/bin/../lib/libffi-3.2.1/include 
DBUS_LIBS = -LD:/MSYS2/mingw64/bin/../lib -ldbus-glib-1 -ldbus-1 -lgobject-2.0 -lglib-2.0 -lintl 
DEFS = -DHAVE_CONFIG_H
DEPDIR = .deps
DLLTOOL = dlltool
DSYMUTIL = 
DUMPBIN = 
ECHO_C = 
ECHO_N = -n
ECHO_T = 
EGREP = /usr/bin/grep -E
ERROR_DIR = D:/MSYS2/mingw64/bin/../share/evolution/2.32/errors
EVOLUTION_2_12 = 
EVOLUTION_DATASERVER_CFLAGS = -ID:/MSYS2/mingw64/bin/../include -ID:/MSYS2/mingw64/bin/../include/glib-2.0 -ID:/MSYS2/mingw64/bin/../lib/glib-2.0/include -ID:/MSYS2/mingw64/bin/../lib/libffi-3.2.1/include -mms-bitfields -ID:/MSYS2/mingw64/bin/../include/nss3 -ID:/MSYS2/mingw64/bin/../include/nspr 
EVOLUTION_DATASERVER_LIBS = -LD:/MSYS2/mingw64/bin/../lib -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lintl -lssl3 -lsmime3 -lnss3 -lnssutil3 -lplds4 -lplc4 -lnspr4 
EVOLUTION_EXEC_VERSION = 2.32
EVOLUTION_RSS_EPLUGIN_CFLAGS = -ID:/MSYS2/mingw64/bin/../include/glib-2.0 -ID:/MSYS2/mingw64/bin/../lib/glib-2.0/include -ID:/MSYS2/mingw64/bin/../include -ID:/MSYS2/mingw64/bin/../include/gtk-2.0 -ID:/MSYS2/mingw64/bin/../lib/gtk-2.0/include -ID:/MSYS2/mingw64/bin/../include/pango-1.0 -ID:/MSYS2/mingw64/bin/../lib/libffi-3.2.1/include -ID:/MSYS2/mingw64/bin/../include/harfbuzz -ID:/MSYS2/mingw64/bin/../include/fribidi -ID:/MSYS2/mingw64/bin/../include/freetype2 -ID:/MSYS2/mingw64/bin/../include/libpng16 -ID:/MSYS2/mingw64/bin/../include/cairo -ID:/MSYS2/mingw64/bin/../include/lzo -ID:/MSYS2/mingw64/bin/../include/pixman-1 -ID:/MSYS2/mingw64/bin/../include/gdk-pixbuf-2.0 -ID:/MSYS2/mingw64/bin/../include/atk-1.0 -ID:/MSYS2/mingw64/bin/../include/libsoup-2.4 -ID:/MSYS2/mingw64/bin/../include/libxml2 -ID:/MSYS2/mingw64/bin/../include/evolution-2.32 -ID:/MSYS2/mingw64/bin/../include/evolution-data-server-2.32 -mms-bitfields -ID:/MSYS2/mingw64/bin/../include/gconf/2 -ID:/MSYS2/mingw64/bin/../include/dbus-1.0 -ID:/MSYS2/mingw64/bin/../lib/dbus-1.0/include -ID:/MSYS2/mingw64/bin/../include/nss3 -ID:/MSYS2/mingw64/bin/../include/nspr -ID:/MSYS2/mingw64/bin/../include/unique-1.0 -ID:/MSYS2/mingw64/bin/../include/libgtkhtml-3.14 -ID:/MSYS2/mingw64/bin/../include/enchant -ID:/MSYS2/mingw64/bin/../include/libgtkhtml-3.14/editor -mms-bitfields -DCAMEL_HAVE_NSS -DCAMEL_HAVE_SSL -mms-bitfields -mms-bitfields 
EVOLUTION_RSS_EPLUGIN_LIBS = -LD:/MSYS2/mingw64/bin/../lib -leutil -lcamel-provider-1.2 -lgtkhtml-editor-3.14 -leshell -Wl,-RD:/MSYS2/mingw64/bin/../lib -lgconf-2 -ledataserverui-1.2 -lunique-1.0 -lebook-1.2 -lcamel-1.2 -lsqlite3 -lssl3 -lsmime3 -lnss3 -lnssutil3 -lplds4 -lplc4 -lnspr4 -ledataserver-1.2 -lxml2 -lgconf-2 -lsoup-2.4 -lgtkhtml-3.14 -lgtk-win32-2.0 -lgdk-win32-2.0 -limm32 -lshell32 -lole32 -Wl,-luuid -lpangowin32-1.0 -lpangocairo-1.0 -lpango-1.0 -lharfbuzz -lgdi32 -lmsimg32 -latk-1.0 -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lenchant -lgmodule-2.0 -lglib-2.0 -lintl 
EVOLUTION_VERSION = 23230
EXEEXT = .exe
FGREP = /usr/bin/grep -F
GCONFTOOL = /MSYS2/mingw64/bin/gconftool-2
GCONF_SCHEMA_CONFIG_SOURCE = xml:merged:D:/MSYS2/mingw64/etc/gconf/gconf.xml.defaults
GCONF_SCHEMA_FILE_DIR = $(sysconfdir)/gconf/schemas
GECKO = 
GECKO_CFLAGS = 
GECKO_EXTRA_LIBS = 
GECKO_FLAVOUR = 
GECKO_GLUE_LIBS = 
GECKO_HOME = 
GECKO_INCLUDE_ROOT = 
GECKO_LIBS = 
GECKO_PREFIX = 
GECKO_RPATH = 
GECKO_XPCOM = 
GETTEXT = /MSYS2/mingw64/bin/gettext
GETTEXT_PACKAGE = evolution-rss
GMOFILES = 
GMSGFMT = /MSYS2/mingw64/bin/msgfmt
GPP = /MSYS2/mingw64/bin/g++
GREP = /usr/bin/grep
GTKHTML_CFLAGS = -ID:/MSYS2/mingw64/bin/../include/libgtkhtml-3.14 -ID:/MSYS2/mingw64/bin/../include/gtk-2.0 -ID:/MSYS2/mingw64/bin/../lib/gtk-2.0/include -ID:/MSYS2/mingw64/bin/../include/pango-1.0 -ID:/MSYS2/mingw64/bin/../include -ID:/MSYS2/mingw64/bin/../include/glib-2.0 -ID:/MSYS2/mingw64/bin/../lib/glib-2.0/include -ID:/MSYS2/mingw64/bin/../lib/libffi-3.2.1/include -ID:/MSYS2/mingw64/bin/../include/harfbuzz -ID:/MSYS2/mingw64/bin/../include/fribidi -ID:/MSYS2/mingw64/bin/../include/freetype2 -ID:/MSYS2/mingw64/bin/../include/libpng16 -ID:/MSYS2/mingw64/bin/../include/cairo -ID:/MSYS2/mingw64/bin/../include/lzo -ID:/MSYS2/mingw64/bin/../include/pixman-1 -ID:/MSYS2/mingw64/bin/../include/gdk-pixbuf-2.0 -ID:/MSYS2/mingw64/bin/../include/atk-1.0 -ID:/MSYS2/mingw64/bin/../include/enchant -mms-bitfields 
GTKHTML_EDITOR_CFLAGS = -ID:/MSYS2/mingw64/bin/../include/libgtkhtml-3.14/editor -ID:/MSYS2/mingw64/bin/../include/libgtkhtml-3.14 -ID:/MSYS2/mingw64/bin/../include/gtk-2.0 -ID:/MSYS2/mingw64/bin/../lib/gtk-2.0/include -ID:/MSYS2/mingw64/bin/../include/pango-1.0 -ID:/MSYS2/mingw64/bin/../include -ID:/MSYS2/mingw64/bin/../include/glib-2.0 -ID:/MSYS2/mingw64/bin/../lib/glib-2.0/include -ID:/MSYS2/mingw64/bin/../lib/libffi-3.2.1/include -ID:/MSYS2/mingw64/bin/../include/harfbuzz -ID:/MSYS2/mingw64/bin/../include/fribidi -ID:/MSYS2/mingw64/bin/../include/freetype2 -ID:/MSYS2/mingw64/bin/../include/libpng16 -ID:/MSYS2/mingw64/bin/../include/cairo -ID:/MSYS2/mingw64/bin/../include/lzo -ID:/MSYS2/mingw64/bin/../include/pixman-1 -ID:/MSYS2/mingw64/bin/../include/gdk-pixbuf-2.0 -ID:/MSYS2/mingw64/bin/../include/atk-1.0 -ID:/MSYS2/mingw64/bin/../include/enchant -mms-bitfields 
GTKHTML_EDITOR_LIBS = -LD:/MSYS2/mingw64/bin/../lib -lgtkhtml-editor-3.14 -lgtkhtml-3.14 -lgtk-win32-2.0 -lgdk-win32-2.0 -limm32 -lshell32 -lole32 -Wl,-luuid -lpangowin32-1.0 -lpangocairo-1.0 -lpango-1.0 -lharfbuzz -lgdi32 -lmsimg32 -latk-1.0 -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lenchant -lgmodule-2.0 -lglib-2.0 -lintl 
GTKHTML_LIBS = -LD:/MSYS2/mingw64/bin/../lib -lgtkhtml-3.14 -lgtk-win32-2.0 -lgdk-win32-2.0 -limm32 -lshell32 -lole32 -Wl,-luuid -lpangowin32-1.0 -lpangocairo-1.0 -lpango-1.0 -lharfbuzz -lgdi32 -lmsimg32 -latk-1.0 -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lenchant -lgmodule-2.0 -lglib-2.0 -lintl 
GTK_VERSION = 2024033
HAVE_BUGGY_GECKO = 
HAVE_GECKO = 
HAVE_GECKO_1_9 = 
HAVE_GTKHTMLEDITOR = 
HAVE_LIBSOUP_GNOME = 
HAVE_RENDERKIT = 
HAVE_WEBKIT = 
HAVE_XULRUNNER = 
ICON_DIR = D:/MSYS2/mingw64/bin/../share/evolution/2.32/images
IMAGESDIR_IN_SERVER_FILE = ../../../share/evolution/2.32/images
INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}
INSTALL_SCRIPT = ${INSTALL}
INSTALL_STRIP_PROGRAM = $(install_sh) -c -s
INSTOBJEXT = .mo
INTLLIBS = -lintl  
INTLTOOL_EXTRACT = /usr/bin/intltool-extract
INTLTOOL_MERGE = /usr/bin/intltool-merge
INTLTOOL_PERL = /usr/bin/perl
INTLTOOL_UPDATE = /usr/bin/intltool-update
INTLTOOL_V_MERGE = $(INTLTOOL__v_MERGE_$(V))
INTLTOOL_V_MERGE_OPTIONS = $(intltool__v_merge_options_$(V))
INTLTOOL__v_MERGE_ = $(INTLTOOL__v_MERGE_$(AM_DEFAULT_VERBOSITY))
INTLTOOL__v_MERGE_0 = @echo "  ITMRG " $@;
INTL_MACOSX_LIBS = 
LD = D:/MSYS2/mingw64/x86_64-w64-mingw32/bin/ld.exe
LDFLAGS = 
LIBOBJS = 
LIBS = -lz -lws2_32 -lregex -lversion -lssp
LIBSOUP_22_CFLAGS = 
LIBSOUP_22_LIBS = 
LIBSOUP_24_CFLAGS = -ID:/MSYS2/mingw64/bin/../include/libsoup-2.4 -ID:/MSYS2/mingw64/bin/../include/glib-2.0 -ID:/MSYS2/mingw64/bin/../lib/glib-2.0/include -ID:/MSYS2/mingw64/bin/../include -ID:/MSYS2/mingw64/bin/../lib/libffi-3.2.1/include -mms-bitfields -ID:/MSYS2/mingw64/bin/../include/libxml2 
LIBSOUP_24_LIBS = -LD:/MSYS2/mingw64/bin/../lib -lsoup-2.4 -lgmodule-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lintl 
LIBSOUP_CFLAGS = 
LIBSOUP_GNOME_CFLAGS = -ID:/MSYS2/mingw64/bin/../include/libsoup-gnome-2.4 -ID:/MSYS2/mingw64/bin/../include/libsoup-2.4 -ID:/MSYS2/mingw64/bin/../include/glib-2.0 -ID:/MSYS2/mingw64/bin/../lib/glib-2.0/include -ID:/MSYS2/mingw64/bin/../include -ID:/MSYS2/mingw64/bin/../lib/libffi-3.2.1/include -ID:/MSYS2/mingw64/bin/../include/libxml2 -mms-bitfields 
LIBSOUP_GNOME_LIBS = -LD:/MSYS2/mingw64/bin/../lib -lsoup-gnome-2.4 -lsoup-2.4 -lgmodule-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lintl 
LIBSOUP_LIBS = -LD:/MSYS2/mingw64/bin/../lib -lsoup-gnome-2.4 -lsoup-2.4 -lgmodule-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lintl 
LIBTOOL = $(SHELL) $(top_builddir)/libtool
LIPO = 
LN_S = cp -pR
LOCALEDIR = /MSYS2/mingw64/share/locale
LTLIBOBJS = 
LT_SYS_LIBRARY_PATH = 
MAINT = 
MAKEINFO = ${SHELL} '/d/Download/MinGW-64-souorce/evolution-rss-0-2-5/missing' makeinfo
MANIFEST_TOOL = :
MINOR_VERSION = 32000
MKDIR_P = /usr/bin/mkdir -p
MKINSTALLDIRS = ../mkinstalldirs
MSGFMT = /MSYS2/mingw64/bin/msgfmt
MSGFMT_OPTS = -c
MSGMERGE = /MSYS2/mingw64/bin/msgmerge
NM = /MSYS2/mingw64/bin/nm -B
NMEDIT = 
NO_UNDEFINED = -no-undefined
NO_UNDEFINED_LIBS = $(EVOLUTION_RSS_EPLUGIN_LIBS) -levolution-mail -lemiscwidgets -lemformat
OBJDUMP = objdump
OBJEXT = o
OTOOL = 
OTOOL64 = 
PACKAGE = evolution-rss
PACKAGE_BUGREPORT = http://bugzilla.gnome.org/enter_bug.cgi?product=evolution-rss
PACKAGE_NAME = evolution-rss
PACKAGE_STRING = evolution-rss 0.2.6
PACKAGE_TARNAME = evolution-rss
PACKAGE_URL = 
PACKAGE_VERSION = 0.2.6
PATH_SEPARATOR = :
PKG_CONFIG = /MSYS2/mingw64/bin/pkg-config.exe
PKG_CONFIG_LIBDIR = 
PKG_CONFIG_PATH = 
PLUGINDIR_IN_SERVER_FILE = ../../../lib/evolution/2.32/plugins
PLUGIN_INSTALL_DIR = D:/MSYS2/mingw64/bin/../lib/evolution/2.32/plugins
POFILES = 
POSUB = po
PO_IN_DATADIR_FALSE = 
PO_IN_DATADIR_TRUE = 
RANLIB = ranlib
RENDER = gtkhtml
RENDER_CFLAGS = 
RENDER_N = 0
SED = /usr/bin/sed
SET_MAKE = 
SHELL = /bin/sh
SOEXT = .dll
SOUP_VERSION = 2074003
STRIP = strip
USE_NLS = yes
VERSION = 0.2.6
WARNING_FLAGS =   -DG_DISABLE_DEPRECATED -DPANGO_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE -Wall -Wextra -Wno-missing-field-initializers -Wno-sign-compare -Wno-unused-parameter -Wdeclaration-after-statement -Werror-implicit-function-declaration -Wformat-security -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Wmissing-noreturn -Wnested-externs -Wpointer-arith -Wredundant-decls -Wundef -Wwrite-strings
WARN_CFLAGS =  -Wall -Wstrict-prototypes -Wnested-externs -Werror=missing-prototypes -Werror=implicit-function-declaration -Werror=pointer-arith -Werror=init-self -Werror=format-security -Werror=format=2 -Werror=missing-include-dirs -Werror=return-type  -ansi -pedantic
WEBKIT_CFLAGS = 
WEBKIT_LIBS = 
WEBKIT_UNSTD = 
WEBKIT_VERSION = 
WebKitGtk_CFLAGS = 
WebKitGtk_LIBS = 
XGETTEXT = /MSYS2/mingw64/bin/xgettext
abs_builddir = /d/Download/MinGW-64-souorce/evolution-rss-0-2-5/mingw64
abs_srcdir = /d/Download/MinGW-64-souorce/evolution-rss-0-2-5/mingw64/..
abs_top_builddir = /d/Download/MinGW-64-souorce/evolution-rss-0-2-5/mingw64
abs_top_srcdir = /d/Download/MinGW-64-souorce/evolution-rss-0-2-5/mingw64/..
ac_ct_AR = ar
ac_ct_CC = 
ac_ct_CXX = 
ac_ct_DUMPBIN = 
am__include = include
am__leading_dot = .
am__quote = 
am__tar = $${TAR-tar} chof - "$$tardir"
am__untar = $${TAR-tar} xf -
bindir = /MSYS2/mingw64/bin
build = x86_64-w64-mingw32
build_alias = x86_64-w64-mingw32
build_cpu = x86_64
build_os = mingw32
build_vendor = w64
builddir = .
datadir = /MSYS2/mingw64/share
datarootdir = ${prefix}/share
docdir = ${datarootdir}/doc/${PACKAGE_TARNAME}
dvidir = ${docdir}
evolution_version_int = 23203
exec_prefix = ${prefix}
host = x86_64-w64-mingw32
host_alias = x86_64-w64-mingw32
host_cpu = x86_64
host_os = mingw32
host_vendor = w64
htmldir = ${docdir}
includedir = ${prefix}/include
infodir = ${datarootdir}/info
install_sh = ${SHELL} /d/Download/MinGW-64-souorce/evolution-rss-0-2-5/install-sh
intltool__v_merge_options_ = $(intltool__v_merge_options_$(AM_DEFAULT_VERBOSITY))
intltool__v_merge_options_0 = -q
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
localedir = /MSYS2/mingw64/share/locale
localstatedir = ${prefix}/var
mandir = ${datarootdir}/man
mkdir_p = $(MKDIR_P)
oldincludedir = /usr/include
pdfdir = ${docdir}
prefix = /MSYS2/mingw64
privdatadir = ${datadir}/evolution/2.32
privlibdir = ${libdir}/evolution/2.32
program_transform_name = s,x,x,
psdir = ${docdir}
sbindir = /MSYS2/mingw64/bin
serverdir = 
sharedstatedir = ${prefix}/com
srcdir = ..
sysconfdir = /MSYS2/mingw64/etc
target_alias = x86_64-w64-mingw32
top_build_prefix = 
top_builddir = .
top_srcdir = ..
uidir = ${datadir}/evolution/2.32/ui
SUBDIRS = pixmaps po src
INTLTOOL = \
	intltool-extract.in     \
	intltool-merge.in       \
	intltool-update.in

EXTRA_DIST = \
	$(INTLTOOL)		\
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	server.mk		\
	FAQ

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

GITIGNOREFILES = aclocal.m4				\
			compile					\
			config.guess				\
			config.h.in				\
			config.sub				\
			ltmain.sh				\
			m4/libtool.m4				\
			m4/lt*.m4				\
			src/GNOME_Evolution_RSS_2.26.server

all: config.h
	$(MAKE) $(AM_MAKEFLAGS) all-recursive

.SUFFIXES:
am--refresh: Makefile
	@:
$(srcdir)/Makefile.in:  $(srcdir)/Makefile.am  $(am__configure_deps)
	@for dep in $?; do \
	  case '$(am__configure_deps)' in \
	    *$$dep*) \
	      echo ' cd $(srcdir) && $(AUTOMAKE) --gnu'; \
	      $(am__cd) $(srcdir) && $(AUTOMAKE) --gnu \
		&& exit 0; \
	      exit 1;; \
	  esac; \
	done; \
	echo ' cd $(top_srcdir) && $(AUTOMAKE) --gnu Makefile'; \
	$(am__cd) $(top_srcdir) && \
	  $(AUTOMAKE) --gnu Makefile
Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status
	@case '$?' in \
	  *config.status*) \
	    echo ' $(SHELL) ./config.status'; \
	    $(SHELL) ./config.status;; \
	  *) \
	    echo ' cd $(top_builddir) && $(SHELL) ./config.status $@ $(am__maybe_remake_depfiles)'; \
	    cd $(top_builddir) && $(SHELL) ./config.status $@ $(am__maybe_remake_depfiles);; \
	esac;

$(top_builddir)/config.status: $(top_srcdir)/configure $(CONFIG_STATUS_DEPENDENCIES)
	$(SHELL) ./config.status --recheck

$(top_srcdir)/configure:  $(am__configure_deps)
	$(am__cd) $(srcdir) && $(AUTOCONF)
$(ACLOCAL_M4):  $(am__aclocal_m4_deps)
	$(am__cd) $(srcdir) && $(ACLOCAL) $(ACLOCAL_AMFLAGS)
$(am__aclocal_m4_deps):

config.h: stamp-h1
	@test -f $@ || rm -f stamp-h1
	@test -f $@ || $(MAKE) $(AM_MAKEFLAGS) stamp-h1

stamp-h1: $(srcdir)/config.h.in $(top_builddir)/config.status
	@rm -f stamp-h1
	cd $(top_builddir) && $(SHELL) ./config.status config.h
$(srcdir)/config.h.in:  $(am__configure_deps) 
	($(am__cd) $(top_srcdir) && $(AUTOHEADER))
	rm -f stamp-h1
	touch $@

distclean-hdr:
	-rm -f config.h stamp-h1
evolution-rss.spec: $(top_builddir)/config.status $(srcdir)/evolution-rss.spec.in
	cd $(top_builddir) && $(SHELL) ./config.status $@

mostlyclean-libtool:
	-rm -f *.lo

clean-libtool:
	-rm -rf .libs _libs

distclean-libtool:
	-rm -f libtool config.lt

# This directory's subdirectories are mostly independent; you can cd
# into them and run 'make' without going through this Makefile.
# To change the values of 'make' variables: instead of editing Makefiles,
# (1) if the variable is set in 'config.status', edit 'config.status'
#     (which will cause the Makefiles to be regenerated when you run 'make');
# (2) otherwise, pass the desired values on the 'make' command line.
$(am__recursive_targets):
	@fail=; \
	if $(am__make_keepgoing); then \
	  failcom='fail=yes'; \
	else \
	  failcom='exit 1'; \
	fi; \
	dot_seen=no; \
	target=`echo $@ | sed s/-recursive//`; \
	case "$@" in \
	  distclean-* | maintainer-clean-*) list='$(DIST_SUBDIRS)' ;; \
	  *) list='$(SUBDIRS)' ;; \
	esac; \
	for subdir in $$list; do \
	  echo "Making $$target in $$subdir"; \
	  if test "$$subdir" = "."; then \
	    dot_seen=yes; \
	    local_target="$$target-am"; \
	  else \
	    local_target="$$target"; \
	  fi; \
	  ($(am__cd) $$subdir && $(MAKE) $(AM_MAKEFLAGS) $$local_target) \
	  || eval $$failcom; \
	done; \
	if test "$$dot_seen" = "no"; then \
	  $(MAKE) $(AM_MAKEFLAGS) "$$target-am" || exit 1; \
	fi; test -z "$$fail"

ID: $(am__tagged_files)
	$(am__define_uniq_tagged_files); mkid -fID $$unique
tags: tags-recursive
TAGS: tags

tags-am: $(TAGS_DEPENDENCIES) $(am__tagged_files)
	set x; \
	here=`pwd`; \
	if ($(ETAGS) --etags-include --version) >/dev/null 2>&1; then \
	  include_option=--etags-include; \
	  empty_fix=.; \
	else \
	  include_option=--include; \
	  empty_fix=; \
	fi; \
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  if test "$$subdir" = .; then :; else \
	    test ! -f $$subdir/TAGS || \
	      set "$$@" "$$include_option=$$here/$$subdir/TAGS"; \
	  fi; \
	done; \
	$(am__define_uniq_tagged_files); \
	shift; \
	if test -z "$(ETAGS_ARGS)$$*$$unique"; then :; else \
	  test -n "$$unique" || unique=$$empty_fix; \
	  if test $$# -gt 0; then \
	    $(ETAGS) $(ETAGSFLAGS) $(AM_ETAGSFLAGS) $(ETAGS_ARGS) \
	      "$$@" $$unique; \
	  else \
	    $(ETAGS) $(ETAGSFLAGS) $(AM_ETAGSFLAGS) $(ETAGS_ARGS) \
	      $$unique; \
	  fi; \
	fi
ctags: ctags-recursive

CTAGS: ctags
ctags-am: $(TAGS_DEPENDENCIES) $(am__tagged_files)
	$(am__define_uniq_tagged_files); \
	test -z "$(CTAGS_ARGS)$$unique" \
	  || $(CTAGS) $(CTAGSFLAGS) $(AM_CTAGSFLAGS) $(CTAGS_ARGS) \
	     $$unique

GTAGS:
	here=`$(am__cd) $(top_builddir) && pwd` \
	  && $(am__cd) $(top_srcdir) \
	  && gtags -i $(GTAGS_ARGS) "$$here"
cscope: cscope.files
	test ! -s cscope.files \
	  || $(CSCOPE) -b -q $(AM_CSCOPEFLAGS) $(CSCOPEFLAGS) -i cscope.files $(CSCOPE_ARGS)
clean-cscope:
	-rm -f cscope.files
cscope.files: clean-cscope cscopelist
cscopelist: cscopelist-recursive

cscopelist-am: $(am__tagged_files)
	list='$(am__tagged_files)'; \
	case "$(srcdir)" in \
	  [\\/]* | ?:[\\/]*) sdir="$(srcdir)" ;; \
	  *) sdir=$(subdir)/$(srcdir) ;; \
	esac; \
	for i in $$list; do \
	  if test -f "$$i"; then \
	    echo "$(subdir)/$$i"; \
	  else \
	    echo "$$sdir/$$i"; \
	  fi; \
	done >> $(top_builddir)/cscope.files

distclean-tags:
	-rm -f TAGS ID GTAGS GRTAGS GSYMS GPATH tags
	-rm -f cscope.out cscope.in.out cscope.po.out cscope.files

distdir: $(BUILT_SOURCES)
	$(MAKE) $(AM_MAKEFLAGS) distdir-am

distdir-am: $(DISTFILES)
	$(am__remove_distdir)
	test -d "$(distdir)" || mkdir "$(distdir)"
	@srcdirstrip=`echo "$(srcdir)" | sed 's/[].[^$$\\*]/\\\\&/g'`; \
	topsrcdirstrip=`echo "$(top_srcdir)" | sed 's/[].[^$$\\*]/\\\\&/g'`; \
	list='$(DISTFILES)'; \
	  dist_files=`for file in $$list; do echo $$file; done | \
	  sed -e "s|^$$srcdirstrip/||;t" \
	      -e "s|^$$topsrcdirstrip/|$(top_builddir)/|;t"`; \
	case $$dist_files in \
	  */*) $(MKDIR_P) `echo "$$dist_files" | \
			   sed '/\//!d;s|^|$(distdir)/|;s,/[^/]*$$,,' | \
			   sort -u` ;; \
	esac; \
	for file in $$dist_files; do \
	  if test -f $$file || test -d $$file; then d=.; else d=$(srcdir); fi; \
	  if test -d $$d/$$file; then \
	    dir=`echo "/$$file" | sed -e 's,/[^/]*$$,,'`; \
	    if test -d "$(distdir)/$$file"; then \
	      find "$(distdir)/$$file" -type d ! -perm -700 -exec chmod u+rwx {} \;; \
	    fi; \
	    if test -d $(srcdir)/$$file && test $$d != $(srcdir); then \
	      cp -fpR $(srcdir)/$$file "$(distdir)$$dir" || exit 1; \
	      find "$(distdir)/$$file" -type d ! -perm -700 -exec chmod u+rwx {} \;; \
	    fi; \
	    cp -fpR $$d/$$file "$(distdir)$$dir" || exit 1; \
	  else \
	    test -f "$(distdir)/$$file" \
	    || cp -p $$d/$$file "$(distdir)/$$file" \
	    || exit 1; \
	  fi; \
	done
	@list='$(DIST_SUBDIRS)'; for subdir in $$list; do \
	  if test "$$subdir" = .; then :; else \
	    $(am__make_dryrun) \
	      || test -d "$(distdir)/$$subdir" \
	      || $(MKDIR_P) "$(distdir)/$$subdir" \
	      || exit 1; \
	    dir1=$$subdir; dir2="$(distdir)/$$subdir"; \
	    $(am__relativize); \
	    new_distdir=$$reldir; \
	    dir1=$$subdir; dir2="$(top_distdir)"; \
	    $(am__relativize); \
	    new_top_distdir=$$reldir; \
	    echo " (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) top_distdir="$$new_top_distdir" distdir="$$new_distdir" \\"; \
	    echo "     am__remove_distdir=: am__skip_length_check=: am__skip_mode_fix=: distdir)"; \
	    ($(am__cd) $$subdir && \
	      $(MAKE) $(AM_MAKEFLAGS) \
	        top_distdir="$$new_top_distdir" \
	        distdir="$$new_distdir" \
		am__remove_distdir=: \
		am__skip_length_check=: \
		am__skip_mode_fix=: \
	        distdir) \
	      || exit 1; \
	  fi; \
	done
	$(MAKE) $(AM_MAKEFLAGS) \
	  top_distdir="$(top_distdir)" distdir="$(distdir)" \
	  dist-hook
	-test -n "$(am__skip_mode_fix)" \
	|| find "$(distdir)" -type d ! -perm -755 \
		-exec chmod u+rwx,go+rx {} \; -o \
	  ! -type d ! -perm -444 -links 1 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -400 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -444 -exec $(install_sh) -c -m a+r {} {} \; \
	|| chmod -R a+r "$(distdir)"
dist-gzip: distdir
	tardir=$(distdir) && $(am__tar) | eval GZIP= gzip $(GZIP_ENV) -c >$(distdir).tar.gz
	$(am__post_remove_distdir)

dist-bzip2: distdir
	tardir=$(distdir) && $(am__tar) | BZIP2=$${BZIP2--9} bzip2 -c >$(distdir).tar.bz2
	$(am__post_remove_distdir)

dist-lzip: distdir
	tardir=$(distdir) && $(am__tar) | lzip -c $${LZIP_OPT--9} >$(distdir).tar.lz
	$(am__post_remove_distdir)

dist-xz: distdir
	tardir=$(distdir) && $(am__tar) | XZ_OPT=$${XZ_OPT--e} xz -c >$(distdir).tar.xz
	$(am__post_remove_distdir)

dist-zstd: distdir
	tardir=$(distdir) && $(am__tar) | zstd -c $${ZSTD_CLEVEL-$${ZSTD_OPT--19}} >$(distdir).tar.zst
	$(am__post_remove_distdir)

dist-tarZ: distdir
	@echo WARNING: "Support for distribution archives compressed with" \
		       "legacy program 'compress' is deprecated." >&2
	@echo WARNING: "It will be removed altogether in Automake 2.0" >&2
	tardir=$(distdir) && $(am__tar) | compress -c >$(distdir).tar.Z
	$(am__post_remove_distdir)

dist-shar: distdir
	@echo WARNING: "Support for shar distribution archives is" \
	               "deprecated." >&2
	@echo WARNING: "It will be removed altogether in Automake 2.0" >&2
	shar $(distdir) | eval GZIP= gzip $(GZIP_ENV) -c >$(distdir).shar.gz
	$(am__post_remove_distdir)

dist-zip: distdir
	-rm -f $(distdir).zip
	zip -rq $(distdir).zip $(distdir)
	$(am__post_remove_distdir)

dist dist-all:
	$(MAKE) $(AM_MAKEFLAGS) $(DIST_TARGETS) am__post_remove_distdir='@:'
	$(am__post_remove_distdir)

# This target untars the dist file and tries a VPATH configuration.  Then
# it guarantees that the distribution is self-contained by making another
# tarfile.
distcheck: dist
	case '$(DIST_ARCHIVES)' in \
	*.tar.gz*) \
	  eval GZIP= gzip $(GZIP_ENV) -dc $(distdir).tar.gz | $(am__untar) ;;\
	*.tar.bz2*) \
	  bzip2 -dc $(distdir).tar.bz2 | $(am__untar) ;;\
	*.tar.lz*) \
	  lzip -dc $(distdir).tar.lz | $(am__untar) ;;\
	*.tar.xz*) \
	  xz -dc $(distdir).tar.xz | $(am__untar) ;;\
	*.tar.Z*) \
	  uncompress -c $(distdir).tar.Z | $(am__untar) ;;\
	*.shar.gz*) \
	  eval GZIP= gzip $(GZIP_ENV) -dc $(distdir).shar.gz | unshar ;;\
	*.zip*) \
	  unzip $(distdir).zip ;;\
	*.tar.zst*) \
	  zstd -dc $(distdir).tar.zst | $(am__untar) ;;\
	esac
	chmod -R a-w $(distdir)
	chmod u+w $(distdir)
	mkdir $(distdir)/_build $(distdir)/_build/sub $(distdir)/_inst
	chmod a-w $(distdir)
	test -d $(distdir)/_build || exit 0; \
	dc_install_base=`$(am__cd) $(distdir)/_inst && pwd | sed -e 's,^[^:\\/]:[\\/],/,'` \
	  && dc_destdir="$${TMPDIR-/tmp}/am-dc-$$$$/" \
	  && am__cwd=`pwd` \
	  && $(am__cd) $(distdir)/_build/sub \
	  && ../../configure \
	    $(AM_DISTCHECK_CONFIGURE_FLAGS) \
	    $(DISTCHECK_CONFIGURE_FLAGS) \
	    --srcdir=../.. --prefix="$$dc_install_base" \
	  && $(MAKE) $(AM_MAKEFLAGS) \
	  && $(MAKE) $(AM_MAKEFLAGS) $(AM_DISTCHECK_DVI_TARGET) \
	  && $(MAKE) $(AM_MAKEFLAGS) check \
	  && $(MAKE) $(AM_MAKEFLAGS) install \
	  && $(MAKE) $(AM_MAKEFLAGS) installcheck \
	  && $(MAKE) $(AM_MAKEFLAGS) uninstall \
	  && $(MAKE) $(AM_MAKEFLAGS) distuninstallcheck_dir="$$dc_install_base" \
	        distuninstallcheck \
	  && chmod -R a-w "$$dc_install_base" \
	  && ({ \
	       (cd ../.. && umask 077 && mkdir "$$dc_destdir") \
	       && $(MAKE) $(AM_MAKEFLAGS) DESTDIR="$$dc_destdir" install \
	       && $(MAKE) $(AM_MAKEFLAGS) DESTDIR="$$dc_destdir" uninstall \
	       && $(MAKE) $(AM_MAKEFLAGS) DESTDIR="$$dc_destdir" \
	            distuninstallcheck_dir="$$dc_destdir" distuninstallcheck; \
	      } || { rm -rf "$$dc_destdir"; exit 1; }) \
	  && rm -rf "$$dc_destdir" \
	  && $(MAKE) $(AM_MAKEFLAGS) dist \
	  && rm -rf $(DIST_ARCHIVES) \
	  && $(MAKE) $(AM_MAKEFLAGS) distcleancheck \
	  && cd "$$am__cwd" \
	  || exit 1
	$(am__post_remove_distdir)
	@(echo "$(distdir) archives ready for distribution: "; \
	  list='$(DIST_ARCHIVES)'; for i in $$list; do echo $$i; done) | \
	  sed -e 1h -e 1s/./=/g -e 1p -e 1x -e '$$p' -e '$$x'
distuninstallcheck:
	@test -n '$(distuninstallcheck_dir)' || { \
	  echo 'ERROR: trying to run $@ with an empty' \
	       '$$(distuninstallcheck_dir)' >&2; \
	  exit 1; \
	}; \
	$(am__cd) '$(distuninstallcheck_dir)' || { \
	  echo 'ERROR: cannot chdir into $(distuninstallcheck_dir)' >&2; \
	  exit 1; \
	}; \
	test `$(am__distuninstallcheck_listfiles) | wc -l` -eq 0 \
	   || { echo "ERROR: files left after uninstall:" ; \
	        if test -n "$(DESTDIR)"; then \
	          echo "  (check DESTDIR support)"; \
	        fi ; \
	        $(distuninstallcheck_listfiles) ; \
	        exit 1; } >&2
distcleancheck: distclean
	@if test '$(srcdir)' = . ; then \
	  echo "ERROR: distcleancheck can only run from a VPATH build" ; \
	  exit 1 ; \
	fi
	@test `$(distcleancheck_listfiles) | wc -l` -eq 0 \
	  || { echo "ERROR: files left in build directory after distclean:" ; \
	       $(distcleancheck_listfiles) ; \
	       exit 1; } >&2
check-am: all-am
check: check-recursive
all-am: Makefile config.h
installdirs: installdirs-recursive
installdirs-am:
install: install-recursive
install-exec: install-exec-recursive
install-data: install-data-recursive
uninstall: uninstall-recursive

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

installcheck: installcheck-recursive
install-strip:
	if test -z '$(STRIP)'; then \
	  $(MAKE) $(AM_MAKEFLAGS) INSTALL_PROGRAM="$(INSTALL_STRIP_PROGRAM)" \
	    install_sh_PROGRAM="$(INSTALL_STRIP_PROGRAM)" INSTALL_STRIP_FLAG=-s \
	      install; \
	else \
	  $(MAKE) $(AM_MAKEFLAGS) INSTALL_PROGRAM="$(INSTALL_STRIP_PROGRAM)" \
	    install_sh_PROGRAM="$(INSTALL_STRIP_PROGRAM)" INSTALL_STRIP_FLAG=-s \
	    "INSTALL_PROGRAM_ENV=STRIPPROG='$(STRIP)'" install; \
	fi
mostlyclean-generic:

clean-generic:

distclean-generic:
	-test -z "$(CONFIG_CLEAN_FILES)" || rm -f $(CONFIG_CLEAN_FILES)
	-test . = "$(srcdir)" || test -z "$(CONFIG_CLEAN_VPATH_FILES)" || rm -f $(CONFIG_CLEAN_VPATH_FILES)
	-test -z "$(DISTCLEANFILES)" || rm -f $(DISTCLEANFILES)

maintainer-clean-generic:
	@echo "This command is intended for maintainers to use"
	@echo "it deletes files that may require special tools to rebuild."
clean: clean-recursive

clean-am: clean-generic clean-libtool mostlyclean-am

distclean: distclean-recursive
	-rm -f $(am__CONFIG_DISTCLEAN_FILES)
	-rm -f Makefile
distclean-am: clean-am distclean-generic distclean-hdr \
	distclean-libtool distclean-tags

dvi: dvi-recursive

dvi-am:

html: html-recursive

html-am:

info: info-recursive

info-am:

install-data-am:

install-dvi: install-dvi-recursive

install-dvi-am:

install-exec-am:

install-html: install-html-recursive

install-html-am:

install-info: install-info-recursive

install-info-am:

install-man:

install-pdf: install-pdf-recursive

install-pdf-am:

install-ps: install-ps-recursive

install-ps-am:

installcheck-am:

maintainer-clean: maintainer-clean-recursive
	-rm -f $(am__CONFIG_DISTCLEAN_FILES)
	-rm -rf $(top_srcdir)/autom4te.cache
	-rm -f Makefile
maintainer-clean-am: distclean-am maintainer-clean-generic

mostlyclean: mostlyclean-recursive

mostlyclean-am: mostlyclean-generic mostlyclean-libtool

pdf: pdf-recursive

pdf-am:

ps: ps-recursive

ps-am:

uninstall-am:

.MAKE: $(am__recursive_targets) all install-am install-strip

.PHONY: $(am__recursive_targets) CTAGS GTAGS TAGS all all-am \
	am--refresh check check-am clean clean-cscope clean-generic \
	clean-libtool cscope cscopelist-am ctags ctags-am dist \
	dist-all dist-bzip2 dist-gzip dist-hook dist-lzip dist-shar \
	dist-tarZ dist-xz dist-zip dist-zstd distcheck distclean \
	distclean-generic distclean-hdr distclean-libtool \
	distclean-tags distcleancheck distdir distuninstallcheck dvi \
	dvi-am html html-am info info-am install install-am \
	install-data install-data-am install-dvi install-dvi-am \
	install-exec install-exec-am install-html install-html-am \
	install-info install-info-am install-man install-pdf \
	install-pdf-am install-ps install-ps-am install-strip \
	installcheck installcheck-am installdirs installdirs-am \
	maintainer-clean maintainer-clean-generic mostlyclean \
	mostlyclean-generic mostlyclean-libtool pdf pdf-am ps ps-am \
	tags tags-am uninstall uninstall-am

.PRECIOUS: Makefile


# Generate a ChangeLog file from 'git log'
dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		( cd "$(top_srcdir)" && \
		  echo '# Generated by Makefile. Do not edit.'; echo; \
		  $(top_srcdir)/missing --run git log --stat \
		  EVOLUTION-RSS_0_1_4..) > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		     echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi

-include $(top_srcdir)/git.mk

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:

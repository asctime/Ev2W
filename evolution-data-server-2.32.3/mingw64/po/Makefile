# Makefile for program source directory in GNU NLS utilities package.
# Copyright (C) 1995, 1996, 1997 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
# Copyright (C) 2004-2008 Rodney Dawes <dobey.pwns@gmail.com>
#
# This file may be copied and used freely without restrictions.  It may
# be used in projects which are not available under a GNU Public License,
# but which still want to provide support for the GNU gettext functionality.
#
# - Modified by Owen Taylor <otaylor@redhat.com> to use GETTEXT_PACKAGE
#   instead of PACKAGE and to look for po2tbl in ./ not in intl/
#
# - Modified by jacob berkman <jacob@ximian.com> to install
#   Makefile.in.in and po2tbl.sed.in for use with glib-gettextize
#
# - Modified by Rodney Dawes <dobey.pwns@gmail.com> for use with intltool
#
# We have the following line for use by intltoolize:
# INTLTOOL_MAKEFILE

GETTEXT_PACKAGE = evolution-data-server-2.32
PACKAGE = evolution-data-server
VERSION = 2.32.3

SHELL = /bin/sh

srcdir = ../../po
top_srcdir = ../..
top_builddir = ..
VPATH = ../../po

prefix = /MSYS2/mingw64
exec_prefix = ${prefix}
datadir = /MSYS2/mingw64/share
datarootdir = ${prefix}/share
libdir = ${exec_prefix}/lib
DATADIRNAME = share
itlocaledir = $(prefix)/$(DATADIRNAME)/locale
subdir = po
install_sh = ${SHELL} /d/Download/MinGW-64-souorce/evolution-data-server-2.32.3/install-sh
# Automake >= 1.8 provides /usr/bin/mkdir -p.
# Until it can be supposed, use the safe fallback:
mkdir_p = $(install_sh) -d

INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644

GMSGFMT = /MSYS2/mingw64/bin/msgfmt
MSGFMT = /MSYS2/mingw64/bin/msgfmt
XGETTEXT = /MSYS2/mingw64/bin/xgettext
INTLTOOL_UPDATE = /usr/bin/intltool-update
INTLTOOL_EXTRACT = /usr/bin/intltool-extract
MSGMERGE = INTLTOOL_EXTRACT=$(INTLTOOL_EXTRACT) srcdir=$(srcdir) $(INTLTOOL_UPDATE) --gettext-package $(GETTEXT_PACKAGE) --dist
GENPOT   = INTLTOOL_EXTRACT=$(INTLTOOL_EXTRACT) srcdir=$(srcdir) $(INTLTOOL_UPDATE) --gettext-package $(GETTEXT_PACKAGE) --pot

ALL_LINGUAS = 

PO_LINGUAS=$(shell if test -r $(srcdir)/LINGUAS; then grep -v "^\#" $(srcdir)/LINGUAS; else echo "$(ALL_LINGUAS)"; fi)

USER_LINGUAS=$(shell if test -n "$(LINGUAS)"; then LLINGUAS="$(LINGUAS)"; ALINGUAS="$(ALL_LINGUAS)"; for lang in $$LLINGUAS; do if test -n "`grep \^$$lang$$ $(srcdir)/LINGUAS 2>/dev/null`" -o -n "`echo $$ALINGUAS|tr ' ' '\n'|grep \^$$lang$$`"; then printf "$$lang "; fi; done; fi)

USE_LINGUAS=$(shell if test -n "$(USER_LINGUAS)" -o -n "$(LINGUAS)"; then LLINGUAS="$(USER_LINGUAS)"; else if test -n "$(PO_LINGUAS)"; then LLINGUAS="$(PO_LINGUAS)"; else LLINGUAS="$(ALL_LINGUAS)"; fi; fi; for lang in $$LLINGUAS; do printf "$$lang "; done)

POFILES=$(shell LINGUAS="$(PO_LINGUAS)"; for lang in $$LINGUAS; do printf "$$lang.po "; done)

DISTFILES = Makefile.in.in POTFILES.in $(POFILES)
EXTRA_DISTFILES = ChangeLog POTFILES.skip Makevars LINGUAS

POTFILES = \
	../../addressbook/backends/file/e-book-backend-file.c \
	../../addressbook/backends/google/e-book-backend-google.c \
	../../addressbook/backends/google/util.c \
	../../addressbook/backends/groupwise/e-book-backend-groupwise.c \
	../../addressbook/backends/ldap/e-book-backend-ldap.c \
	../../addressbook/backends/vcf/e-book-backend-vcf.c \
	../../addressbook/backends/webdav/e-book-backend-webdav.c \
	../../addressbook/libebook/e-book.c \
	../../addressbook/libebook/e-contact.c \
	../../addressbook/libebook/e-destination.c \
	../../addressbook/libedata-book/e-data-book-factory.c \
	../../addressbook/libedata-book/e-data-book.c \
	../../calendar/backends/caldav/e-cal-backend-caldav.c \
	../../calendar/backends/contacts/e-cal-backend-contacts.c \
	../../calendar/backends/file/e-cal-backend-file.c \
	../../calendar/backends/groupwise/e-cal-backend-groupwise-utils.c \
	../../calendar/backends/groupwise/e-cal-backend-groupwise.c \
	../../calendar/backends/http/e-cal-backend-http.c \
	../../calendar/backends/weather/e-cal-backend-weather.c \
	../../calendar/libecal/e-cal-component.c \
	../../calendar/libecal/e-cal-recur.c \
	../../calendar/libecal/e-cal-util.c \
	../../calendar/libecal/e-cal.c \
	../../calendar/libedata-cal/e-cal-backend-sexp.c \
	../../calendar/libedata-cal/e-cal-backend-util.c \
	../../calendar/libedata-cal/e-data-cal.c \
	../../calendar/libedata-cal/e-data-cal-factory.c \
	../../camel/camel-address.c \
	../../camel/camel-block-file.c \
	../../camel/camel-certdb.c \
	../../camel/camel-charset-map.c \
	../../camel/camel-cipher-context.c \
	../../camel/camel-data-cache.c \
	../../camel/camel-data-wrapper.c \
	../../camel/camel-db.c \
	../../camel/camel-debug.c \
	../../camel/camel-disco-diary.c \
	../../camel/camel-disco-folder.c \
	../../camel/camel-disco-store.c \
	../../camel/camel-file-utils.c \
	../../camel/camel-filter-driver.c \
	../../camel/camel-filter-search.c \
	../../camel/camel-folder-search.c \
	../../camel/camel-folder-summary.c \
	../../camel/camel-folder-thread.c \
	../../camel/camel-folder.c \
	../../camel/camel-gpg-context.c \
	../../camel/camel-html-parser.c \
	../../camel/camel-http-stream.c \
	../../camel/camel-index-control.c \
	../../camel/camel-index.c \
	../../camel/camel-internet-address.c \
	../../camel/camel-junk-plugin.c \
	../../camel/camel-list-utils.c \
	../../camel/camel-lock-client.c \
	../../camel/camel-lock-helper.c \
	../../camel/camel-lock.c \
	../../camel/camel-medium.c \
	../../camel/camel-mime-filter-basic.c \
	../../camel/camel-mime-filter-bestenc.c \
	../../camel/camel-mime-filter-canon.c \
	../../camel/camel-mime-filter-charset.c \
	../../camel/camel-mime-filter-crlf.c \
	../../camel/camel-mime-filter-enriched.c \
	../../camel/camel-mime-filter-from.c \
	../../camel/camel-mime-filter-gzip.c \
	../../camel/camel-mime-filter-html.c \
	../../camel/camel-mime-filter-index.c \
	../../camel/camel-mime-filter-linewrap.c \
	../../camel/camel-mime-filter-save.c \
	../../camel/camel-mime-filter-tohtml.c \
	../../camel/camel-mime-filter-windows.c \
	../../camel/camel-mime-filter-yenc.c \
	../../camel/camel-mime-filter.c \
	../../camel/camel-mime-message.c \
	../../camel/camel-mime-parser.c \
	../../camel/camel-mime-part-utils.c \
	../../camel/camel-mime-part.c \
	../../camel/camel-mime-utils.c \
	../../camel/camel-movemail.c \
	../../camel/camel-multipart-encrypted.c \
	../../camel/camel-multipart-signed.c \
	../../camel/camel-multipart.c \
	../../camel/camel-net-utils.c \
	../../camel/camel-object.c \
	../../camel/camel-offline-folder.c \
	../../camel/camel-offline-journal.c \
	../../camel/camel-offline-store.c \
	../../camel/camel-operation.c \
	../../camel/camel-partition-table.c \
	../../camel/camel-provider.c \
	../../camel/camel-sasl-anonymous.c \
	../../camel/camel-sasl-cram-md5.c \
	../../camel/camel-sasl-digest-md5.c \
	../../camel/camel-sasl-gssapi.c \
	../../camel/camel-sasl-login.c \
	../../camel/camel-sasl-ntlm.c \
	../../camel/camel-sasl-plain.c \
	../../camel/camel-sasl-popb4smtp.c \
	../../camel/camel-sasl.c \
	../../camel/camel-search-private.c \
	../../camel/camel-seekable-stream.c \
	../../camel/camel-seekable-substream.c \
	../../camel/camel-service.c \
	../../camel/camel-session.c \
	../../camel/camel-smime-context.c \
	../../camel/camel-store-summary.c \
	../../camel/camel-store.c \
	../../camel/camel-stream-buffer.c \
	../../camel/camel-stream-filter.c \
	../../camel/camel-stream-fs.c \
	../../camel/camel-stream-mem.c \
	../../camel/camel-stream-null.c \
	../../camel/camel-stream-process.c \
	../../camel/camel-stream.c \
	../../camel/camel-string-utils.c \
	../../camel/camel-tcp-stream-raw.c \
	../../camel/camel-tcp-stream-ssl.c \
	../../camel/camel-tcp-stream.c \
	../../camel/camel-text-index.c \
	../../camel/camel-transport.c \
	../../camel/camel-uid-cache.c \
	../../camel/camel-url-scanner.c \
	../../camel/camel-url.c \
	../../camel/camel-utf8.c \
	../../camel/camel-vee-folder.c \
	../../camel/camel-vee-store.c \
	../../camel/camel-vee-summary.c \
	../../camel/camel-vtrash-folder.c \
	../../camel/camel.c \
	../../camel/providers/groupwise/camel-groupwise-folder.c \
	../../camel/providers/groupwise/camel-groupwise-journal.c \
	../../camel/providers/groupwise/camel-groupwise-provider.c \
	../../camel/providers/groupwise/camel-groupwise-store-summary.c \
	../../camel/providers/groupwise/camel-groupwise-store.c \
	../../camel/providers/groupwise/camel-groupwise-summary.c \
	../../camel/providers/groupwise/camel-groupwise-transport.c \
	../../camel/providers/groupwise/camel-groupwise-utils.c \
	../../camel/providers/imap/camel-imap-command.c \
	../../camel/providers/imap/camel-imap-folder.c \
	../../camel/providers/imap/camel-imap-journal.c \
	../../camel/providers/imap/camel-imap-message-cache.c \
	../../camel/providers/imap/camel-imap-provider.c \
	../../camel/providers/imap/camel-imap-search.c \
	../../camel/providers/imap/camel-imap-store-summary.c \
	../../camel/providers/imap/camel-imap-store.c \
	../../camel/providers/imap/camel-imap-summary.c \
	../../camel/providers/imap/camel-imap-utils.c \
	../../camel/providers/imap/camel-imap-wrapper.c \
	../../camel/providers/imapx/camel-imapx-folder.c \
	../../camel/providers/imapx/camel-imapx-provider.c \
	../../camel/providers/imapx/camel-imapx-server.c \
	../../camel/providers/imapx/camel-imapx-store.c \
	../../camel/providers/imapx/camel-imapx-stream.c \
	../../camel/providers/local/camel-local-folder.c \
	../../camel/providers/local/camel-local-provider.c \
	../../camel/providers/local/camel-local-store.c \
	../../camel/providers/local/camel-local-summary.c \
	../../camel/providers/local/camel-maildir-folder.c \
	../../camel/providers/local/camel-maildir-store.c \
	../../camel/providers/local/camel-maildir-summary.c \
	../../camel/providers/local/camel-mbox-folder.c \
	../../camel/providers/local/camel-mbox-store.c \
	../../camel/providers/local/camel-mbox-summary.c \
	../../camel/providers/local/camel-mh-folder.c \
	../../camel/providers/local/camel-mh-store.c \
	../../camel/providers/local/camel-mh-summary.c \
	../../camel/providers/local/camel-spool-folder.c \
	../../camel/providers/local/camel-spool-store.c \
	../../camel/providers/local/camel-spool-summary.c \
	../../camel/providers/nntp/camel-nntp-folder.c \
	../../camel/providers/nntp/camel-nntp-provider.c \
	../../camel/providers/nntp/camel-nntp-store-summary.c \
	../../camel/providers/nntp/camel-nntp-store.c \
	../../camel/providers/nntp/camel-nntp-stream.c \
	../../camel/providers/nntp/camel-nntp-summary.c \
	../../camel/providers/pop3/camel-pop3-engine.c \
	../../camel/providers/pop3/camel-pop3-folder.c \
	../../camel/providers/pop3/camel-pop3-provider.c \
	../../camel/providers/pop3/camel-pop3-store.c \
	../../camel/providers/pop3/camel-pop3-stream.c \
	../../camel/providers/sendmail/camel-sendmail-provider.c \
	../../camel/providers/sendmail/camel-sendmail-transport.c \
	../../camel/providers/smtp/camel-smtp-provider.c \
	../../camel/providers/smtp/camel-smtp-transport.c \
	../../libedataserver/e-categories.c \
	../../libedataserver/e-time-utils.c \
	../../libedataserverui/e-book-auth-util.c \
	../../libedataserverui/e-categories-dialog.c \
	../../libedataserverui/e-categories-dialog.ui \
	../../libedataserverui/e-category-completion.c \
	../../libedataserverui/e-cell-renderer-color.c \
	../../libedataserverui/e-name-selector-dialog.c \
	../../libedataserverui/e-name-selector-dialog.ui \
	../../libedataserverui/e-name-selector-entry.c \
	../../libedataserverui/e-name-selector-list.c \
	../../libedataserverui/e-passwords.c \
	../../libedataserverui/e-source-selector-dialog.c \
	../../servers/groupwise/e-gw-connection.c

CATALOGS=$(shell LINGUAS="$(USE_LINGUAS)"; for lang in $$LINGUAS; do printf "$$lang.gmo "; done)

.SUFFIXES:
.SUFFIXES: .po .pox .gmo .mo .msg .cat

.po.pox:
	$(MAKE) $(GETTEXT_PACKAGE).pot
	$(MSGMERGE) $< $(GETTEXT_PACKAGE).pot -o $*.pox

.po.mo:
	$(MSGFMT) -o $@ $<

.po.gmo:
	file=`echo $* | sed 's,.*/,,'`.gmo \
	  && rm -f $$file && $(GMSGFMT) -o $$file $<

.po.cat:
	sed -f ../intl/po2msg.sed < $< > $*.msg \
	  && rm -f $@ && gencat $@ $*.msg


all: all-yes

all-yes: $(CATALOGS)
all-no:

$(GETTEXT_PACKAGE).pot: $(POTFILES)
	$(GENPOT)

install: install-data
install-data: install-data-yes
install-data-no: all
install-data-yes: all
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  dir=$(DESTDIR)$(itlocaledir)/$$lang/LC_MESSAGES; \
	  $(mkdir_p) $$dir; \
	  if test -r $$lang.gmo; then \
	    $(INSTALL_DATA) $$lang.gmo $$dir/$(GETTEXT_PACKAGE).mo; \
	    echo "installing $$lang.gmo as $$dir/$(GETTEXT_PACKAGE).mo"; \
	  else \
	    $(INSTALL_DATA) $(srcdir)/$$lang.gmo $$dir/$(GETTEXT_PACKAGE).mo; \
	    echo "installing $(srcdir)/$$lang.gmo as" \
		 "$$dir/$(GETTEXT_PACKAGE).mo"; \
	  fi; \
	  if test -r $$lang.gmo.m; then \
	    $(INSTALL_DATA) $$lang.gmo.m $$dir/$(GETTEXT_PACKAGE).mo.m; \
	    echo "installing $$lang.gmo.m as $$dir/$(GETTEXT_PACKAGE).mo.m"; \
	  else \
	    if test -r $(srcdir)/$$lang.gmo.m ; then \
	      $(INSTALL_DATA) $(srcdir)/$$lang.gmo.m \
		$$dir/$(GETTEXT_PACKAGE).mo.m; \
	      echo "installing $(srcdir)/$$lang.gmo.m as" \
		   "$$dir/$(GETTEXT_PACKAGE).mo.m"; \
	    else \
	      true; \
	    fi; \
	  fi; \
	done

# Empty stubs to satisfy archaic automake needs
dvi info ctags tags CTAGS TAGS ID:

# Define this as empty until I found a useful application.
install-exec installcheck:

uninstall:
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  rm -f $(DESTDIR)$(itlocaledir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE).mo; \
	  rm -f $(DESTDIR)$(itlocaledir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE).mo.m; \
	done

check: all $(GETTEXT_PACKAGE).pot
	rm -f missing notexist
	srcdir=$(srcdir) $(INTLTOOL_UPDATE) -m
	if [ -r missing -o -r notexist ]; then \
	  exit 1; \
	fi

mostlyclean:
	rm -f *.pox $(GETTEXT_PACKAGE).pot *.old.po cat-id-tbl.tmp
	rm -f .intltool-merge-cache

clean: mostlyclean

distclean: clean
	rm -f Makefile Makefile.in POTFILES stamp-it
	rm -f *.mo *.msg *.cat *.cat.m *.gmo

maintainer-clean: distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."
	rm -f Makefile.in.in

distdir = ../$(PACKAGE)-$(VERSION)/$(subdir)
dist distdir: $(DISTFILES)
	dists="$(DISTFILES)"; \
	extra_dists="$(EXTRA_DISTFILES)"; \
	for file in $$extra_dists; do \
	  test -f $(srcdir)/$$file && dists="$$dists $(srcdir)/$$file"; \
	done; \
	for file in $$dists; do \
	  test -f $$file || file="$(srcdir)/$$file"; \
	  ln $$file $(distdir) 2> /dev/null \
	    || cp -p $$file $(distdir); \
	done

update-po: Makefile
	$(MAKE) $(GETTEXT_PACKAGE).pot
	tmpdir=`pwd`; \
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  echo "$$lang:"; \
	  result="`$(MSGMERGE) -o $$tmpdir/$$lang.new.po $$lang`"; \
	  if $$result; then \
	    if cmp $(srcdir)/$$lang.po $$tmpdir/$$lang.new.po >/dev/null 2>&1; then \
	      rm -f $$tmpdir/$$lang.new.po; \
            else \
	      if mv -f $$tmpdir/$$lang.new.po $$lang.po; then \
	        :; \
	      else \
	        echo "msgmerge for $$lang.po failed: cannot move $$tmpdir/$$lang.new.po to $$lang.po" 1>&2; \
	        rm -f $$tmpdir/$$lang.new.po; \
	        exit 1; \
	      fi; \
	    fi; \
	  else \
	    echo "msgmerge for $$lang.gmo failed!"; \
	    rm -f $$tmpdir/$$lang.new.po; \
	  fi; \
	done

Makefile POTFILES: stamp-it
	@if test ! -f $@; then \
	  rm -f stamp-it; \
	  $(MAKE) stamp-it; \
	fi

stamp-it: Makefile.in.in $(top_builddir)/config.status POTFILES.in
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/Makefile.in CONFIG_HEADERS= CONFIG_LINKS= \
	       $(SHELL) ./config.status

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:

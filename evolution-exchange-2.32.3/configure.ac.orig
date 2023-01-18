dnl Sets the evolution exchange version.
m4_define([eex_version], [2.32.3])

dnl Initializes automake/autoconf
AC_PREREQ(2.58)
AC_INIT([evolution-exchange], [eex_version], [http://bugzilla.gnome.org/enter_bug.cgi?product=Evolution%20Exchange])
AM_INIT_AUTOMAKE([gnu 1.9])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR(camel)
AC_CONFIG_HEADERS(config.h)

dnl Automake 1.11 - Silent Build Rules
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Required Package Versions
m4_define([glib_minimum_version], [2.16.0])
m4_define([eds_minimum_version], [2.32.2])
m4_define([evo_minimum_version], [2.32.2])
m4_define([gconf_minimum_version], [2.0.0])		dnl XXX Just a Guess
m4_define([libxml_minimum_version], [2.0.0])		dnl XXX Just a Guess
m4_define([libsoup_minimum_version], [2.3.0])


dnl **********************************
dnl Gtk-3 compatibility
dnl **********************************
AC_ARG_ENABLE([gtk3],
	AS_HELP_STRING([--enable-gtk3],
	[Attempt to use gtk+-3]),
	[enable_gtk3=$enableval],[enable_gtk3=no])

if test "x${enable_gtk3}" = "xyes"; then
	GTK="gtk+-3.0"
	gtk_minimum_version="2.90.4"

	EVOLUTION_PLUGIN_PK="evolution-plugin-3.0"
	EVOLUTION_MAIL_PK="evolution-mail-3.0"
	EVOLUTION_SHELL_PK="evolution-shell-3.0"
	LIBEDATASERVERUI_PK="libedataserverui-3.0"
else	
	GTK="gtk+-2.0"
	gtk_minimum_version="2.20.0"

	EVOLUTION_PLUGIN_PK="evolution-plugin"
	EVOLUTION_MAIL_PK="evolution-mail"
	EVOLUTION_SHELL_PK="evolution-shell"
	LIBEDATASERVERUI_PK="libedataserverui-1.2"
fi

dnl *********************************************************************
dnl Update these for every new development release of Evolution-Exchange.
dnl These numbers actually correspond to the next stable release number.
dnl Note, this is set the way it is so that GETTEXT_PACKAGE will be
dnl parsed correctly.
dnl ******************************************************************
BASE_VERSION=2.32
m4_define([base_version], [2.32])

dnl **********************
dnl Compiler Warning Flags
dnl **********************
AS_COMPILER_FLAGS(WARNING_FLAGS,
	"-DG_DISABLE_DEPRECATED
	-DPANGO_DISABLE_DEPRECATED
	-DGDK_DISABLE_DEPRECATED
	-DGDK_PIXBUF_DISABLE_DEPRECATED
	-DGTK_DISABLE_DEPRECATED
	-DE_BOOK_DISABLE_DEPRECATED
	-DE_CAL_DISABLE_DEPRECATED
	-DG_DISABLE_SINGLE_INCLUDES
	-DGTK_DISABLE_SINGLE_INCLUDES
	-DGSEAL_ENABLE
	-Wall -Wextra
	-Wno-missing-field-initializers
	-Wno-sign-compare
	-Wno-unused-parameter
	-Wdeclaration-after-statement
	-Werror-implicit-function-declaration
	-Wformat-security -Winit-self
	-Wmissing-declarations -Wmissing-include-dirs
	-Wmissing-noreturn -Wnested-externs -Wpointer-arith
	-Wredundant-decls -Wundef -Wwrite-strings")
AC_SUBST(WARNING_FLAGS)

dnl Other useful compiler warnings for test builds only.
dnl These may produce warnings we have no control over,
dnl or false positives we don't always want to see.
dnl
dnl	-Wformat-nonliteral
dnl	-Wmissing-format-attribute
dnl	-Wshadow
dnl	-Wstrict-aliasing=2

AM_CPPFLAGS="$WARNING_FLAGS -fno-strict-aliasing"
AC_SUBST(AM_CPPFLAGS)

dnl **************************
dnl Initialize maintainer mode
dnl **************************
AM_MAINTAINER_MODE

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl GCC 4.4 got more aggressive in its aliasing optimizations, changing
dnl behavior that -- according to the C99 standard -- is supposed to be
dnl undefined.  We may still have aliasing abuses lying around that rely
dnl on GCC's previous "undefined" behavior, so disable strict-aliasing
dnl optimization until we can find and fix all the abuses.
dnl (AC_PROG_CC must run first to set the GCC variable.)
dnl XXX This really belongs in AM_CFLAGS.
if test "x${GCC}" = "xyes"; then
	CFLAGS="$CFLAGS -fno-strict-aliasing"
fi

dnl ******************************
dnl Check for Win32
dnl ******************************

AC_MSG_CHECKING([for Win32])
case "$host" in
*-mingw*)
	os_win32=yes
	NO_UNDEFINED='-no-undefined'
	SOCKET_LIBS='-lws2_32'
	DNS_LIBS='-ldnsapi'
	;;
*)
	os_win32=no
	NO_UNDEFINED='-no-undefined'
	SOCKET_LIBS=''
	DNS_LIBS=''
	;;
esac

AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(OS_WIN32, [test "x$os_win32" = "xyes"])
AC_SUBST(NO_UNDEFINED)
AC_SUBST(SOCKET_LIBS)
AC_SUBST(DNS_LIBS)

dnl ******************
dnl Initialize libtool
dnl ******************
LT_PREREQ(2.2)
LT_INIT(disable-static win32-dll)

PKG_PROG_PKG_CONFIG

dnl ***************
dnl Initialize i18n
dnl ***************
IT_PROG_INTLTOOL([0.35.5])
AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=evolution-exchange-$BASE_VERSION
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Package name for gettext])

localedir='$(prefix)/$(DATADIRNAME)/locale'
AC_SUBST(localedir)

dnl **********************************
dnl Check for base dependencies early.
dnl **********************************

PKG_CHECK_MODULES(GNOME_PLATFORM,
	[gio-2.0 >= glib_minimum_version
	$GTK >= $gtk_minimum_version
	gconf-2.0 >= gconf_minimum_version
	libxml-2.0 >= libxml_minimum_version
	libsoup-2.4 >= libsoup_minimum_version])

PKG_CHECK_MODULES(EVOLUTION_DATA_SERVER,
	[libebook-1.2 >= eds_minimum_version
	libecal-1.2 >= eds_minimum_version
	libedataserver-1.2 >= eds_minimum_version
	$LIBEDATASERVERUI_PK >= eds_minimum_version
	libedata-book-1.2 >= eds_minimum_version
	libedata-cal-1.2 >= eds_minimum_version
	libegroupwise-1.2 >= eds_minimum_version
	libebackend-1.2 >= eds_minimum_version])

dnl ************************
dnl Check for socklen_t type
dnl ************************
if test x$os_win32 != xyes; then
   AC_EGREP_HEADER(socklen_t, sys/socket.h, :, AC_DEFINE(socklen_t, int, [Define to "int" if socklen_t is not defined]))
fi

dnl *******************
dnl GObject marshalling
dnl *******************
AM_PATH_GLIB_2_0

dnl ***********
dnl GConf stuff
dnl ***********
AC_PATH_PROG([GCONFTOOL],[gconftool-2],[no])
AM_GCONF_SOURCE_2

dnl *************
dnl Gtk-Doc stuff
dnl *************
GTK_DOC_CHECK(1.9)

dnl **************
dnl Exchange debug
dnl **************
AC_ARG_ENABLE([e2k-debug],
	AS_HELP_STRING([--enable-e2k-debug],
	[Allow debugging]),
	[enable_e2kdebug=$enableval],[enable_e2kdebug="no"])

if test "x$enable_e2kdebug" = "xyes"; then
	AC_DEFINE([E2K_DEBUG],[1],[Define if you want E2K_DEBUG to be available])
fi

dnl *************************
dnl Set the connector datadir
dnl *************************
CONNECTOR_DATADIR='$(datadir)/evolution-exchange/$(BASE_VERSION)'
AC_SUBST(CONNECTOR_DATADIR)

dnl *************************************
dnl Check for evo/eds and version setting
dnl *************************************
AC_MSG_CHECKING(Evolution version)
EVOLUTION_VERSION="`$PKG_CONFIG --modversion $EVOLUTION_SHELL_PK 2>/dev/null`"
if test -z "$EVOLUTION_VERSION"; then
	AC_MSG_ERROR(Evolution development libraries not installed)
fi
AC_SUBST(EVOLUTION_VERSION)

AC_SUBST([BASE_VERSION], [base_version])
AC_DEFINE_UNQUOTED(BASE_VERSION, "$BASE_VERSION", Connector base version)

plugindir="`$PKG_CONFIG --variable=plugindir $EVOLUTION_PLUGIN_PK`"
AC_SUBST(plugindir)

EVOLUTION_PLUGIN_errordir="`$PKG_CONFIG --variable=errordir $EVOLUTION_PLUGIN_PK`"
AC_SUBST(EVOLUTION_PLUGIN_errordir)

extensiondir="`$PKG_CONFIG --variable=extensiondir evolution-data-server-1.2`"
AC_SUBST(extensiondir)

EVOLUTION_imagesdir="`$PKG_CONFIG --variable=imagesdir $EVOLUTION_SHELL_PK`"
AC_SUBST(EVOLUTION_imagesdir)

CAMEL_providerdir="`$PKG_CONFIG --variable=camel_providerdir camel-provider-1.2`"
AC_SUBST(CAMEL_providerdir)

dnl ***************************
dnl Check for evolution plugins
dnl ***************************
PKG_CHECK_MODULES(EVOLUTION_PLUGIN, $EVOLUTION_PLUGIN_PK >= evo_minimum_version)
AC_SUBST(EVOLUTION_PLUGIN_CFLAGS)
AC_SUBST(EVOLUTION_PLUGIN_LIBS)

dnl ***************************
dnl Check for evolution mail
dnl ***************************
PKG_CHECK_MODULES(EVOLUTION_MAIL, $EVOLUTION_MAIL_PK >= evo_minimum_version)
AC_SUBST(EVOLUTION_MAIL_CFLAGS)
AC_SUBST(EVOLUTION_MAIL_LIBS)

dnl ***************************
dnl Check for evolution shell
dnl ***************************
PKG_CHECK_MODULES(EVOLUTION_SHELL, $EVOLUTION_SHELL_PK >= evo_minimum_version)
AC_SUBST(EVOLUTION_SHELL_CFLAGS)
AC_SUBST(EVOLUTION_SHELL_LIBS)

dnl *********************
dnl Pilot license support
dnl *********************
dnl This does not take leap years into account, but that's not
dnl important: having the build time be slightly too early is good
dnl to support slight clock skew anyway.
dnl 946684800 is 2000-01-01T00:00:00Z.
dnl Don't change this without testing under /bin/sh on Solaris.
abt=`eval expr \`date '+\( \( %y \* 365 \) + %j - 1 \) \* 24 \* 60 \* 60 + 946684800'\``
case $abt in
"")
	echo "Warning: build time check failed. (Are you on OS X?)."
	echo "Pilot licenses won't work."
	abt="0"
	;;
esac
AC_DEFINE_UNQUOTED([E2K_APPROX_BUILD_TIME],[$abt],[Used to prevent clock-setting attacks against pilot licenses])

AC_DEFINE([HANDLE_LIBICAL_MEMORY],[1],[Define it once memory returned by libical is free'ed properly])

dnl **************
dnl libdb checking
dnl **************
AC_ARG_WITH([libdb],
	AC_HELP_STRING([--with-libdb=PREFIX],
	[Prefix where libdb is installed]),
	[libdb_prefix="$withval"], [libdb_prefix='${prefix}'])

DB_CFLAGS="-I$libdb_prefix/include"
DB_LIBS="-L$libdb_prefix/lib -ldb"

AC_MSG_CHECKING([Berkeley DB])
save_cflags=$CFLAGS
save_libs=$LIBS
CFLAGS=$DB_CFLAGS
LIBS="$DB_LIBS"
AC_LINK_IFELSE([AC_LANG_PROGRAM(
	[[#include <db.h>]],
	[db_create(NULL, NULL, 0)])],
	[check_db=yes],
	[check_db=no])

if test "x$check_db" = "xyes"; then
	AC_MSG_RESULT([$check_db])
else
	AC_DEFINE(ENABLE_CACHE, 0, [Disabling GAL Caching])
	DB_CFLAGS=""
	DB_LIBS=""
fi
AM_CONDITIONAL(HAVE_LIBDB, [test "x$check_db" = "xyes"])

CFLAGS=$save_cflags
LIBS=$save_libs
AC_SUBST(DB_CFLAGS)
AC_SUBST(DB_LIBS)

dnl *********************
dnl OpenLDAP NTLM support
dnl *********************
if test "x$os_win32" != "xyes"; then
	EVO_SUNLDAP_CHECK(no)
	if test "x$with_sunldap" = "xno"; then
		EVO_LDAP_CHECK(yes)
		if test "x$with_openldap" = "xno"; then
			AC_MSG_ERROR([LDAP support is required for Connector])
		fi
	fi

	SAVE_CFLAGS="$CFLAGS"
	SAVE_LIBS="$LIBS"
	LDAP_CFLAGS="$LDAP_CFLAGS -DLDAP_DEPRECATED"
	CFLAGS="$CFLAGS $LDAP_CFLAGS"
	LIBS="$LIBS $LDAP_LIBS"
	AC_CHECK_FUNCS(ldap_ntlm_bind)
	CFLAGS="$SAVE_CFLAGS"
	LIBS="$SAVE_LIBS"
dnl Win32 support
else
	LDAP_CFLAGS="$LDAP_CFLAGS -DLDAP_DEPRECATED"
	LDAP_LIBS="-lwldap32"
	AC_SUBST(LDAP_CFLAGS)
	AC_SUBST(LDAP_LIBS)
	AC_DEFINE(HAVE_LDAP,1,[Define if you have LDAP support])
	AM_CONDITIONAL(ENABLE_LDAP, true)
fi

AM_CONDITIONAL(SUNLDAP, test "x$with_sunldap" != "xno")

AC_CACHE_CHECK([for LDAP Paged Control], [ac_cv_ldappaged],
	AC_RUN_IFELSE([AC_LANG_PROGRAM(
	[[	#include <ldap.h>
		#include <stdlib.h>
	]],
	[[	int main(int argc, char **argv) {
		#ifdef LDAP_CONTROL_PAGEDRESULTS
			exit(0);
		#else
			exit(1);
		#endif
		}
	]],
	)], [ac_cv_ldappaged="yes"], [ac_cv_ladppaged="no"], [ac_cv_ldappaged="yes"]))

# hardcode this for win32 because winldap.h supports the paged results
# although with a tad different API
if test x$os_win32 = xyes; then
   ac_cv_ldappaged=yes
fi

if test "x$ac_cv_ldappaged" = "xyes"; then
	AC_DEFINE([HAVE_LDAP_PAGED], 1, [Supports Paged results])
	if test "x$check_db" = "xyes"; then
		AC_DEFINE([ENABLE_CACHE],[1],[Enabling GAL Caching])
	fi
fi

dnl ****************
dnl Kerberos check
dnl ****************

EVO_KRB5_SUPPORT

dnl ******************************
dnl Makefiles
dnl ******************************

AC_CONFIG_FILES([
Makefile
evolution-exchange-zip
eplugin/Makefile
camel/Makefile
addressbook/Makefile
calendar/Makefile
tools/Makefile
docs/Makefile
docs/reference/Makefile
po/Makefile.in
server/Makefile
server/lib/Makefile
server/storage/Makefile
server/xntlm/Makefile
])
AC_OUTPUT

AC_MSG_NOTICE([
	GTK package	: $GTK
	Debugging	: $enable_e2kdebug
	GTK-Doc		: $enable_gtk_doc
	Kerberos 5	: $msg_krb5
])

if test "x$ac_cv_func_ldap_ntlm_bind" = "xno"; then
	AC_MSG_WARN([
No NTLM support in OpenLDAP; Plaintext password authentication will be
used when connecting to the Global Catalog server. Consider installing
the evo-openldap package, or building OpenLDAP with the patch in
docs/openldap-ntlm.diff
])
fi

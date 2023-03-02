/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Connector base version */
#define BASE_VERSION "2.32"

/* Used to prevent clock-setting attacks against pilot licenses */
<<<<<<< HEAD
<<<<<<< HEAD
#define E2K_APPROX_BUILD_TIME 1677283200
=======
#define E2K_APPROX_BUILD_TIME 1676937600
>>>>>>> 2aa22ab... Further gnome-desktop references removed, refactor to latest Ev2W.
=======
#define E2K_APPROX_BUILD_TIME 1677283200
>>>>>>> d34b05d... Switch to Windows native threads (-MT) and mms-bitfields. More cleanup.

/* Define if you want E2K_DEBUG to be available */
/* #undef E2K_DEBUG */

/* Enabling GAL Caching */
#define ENABLE_CACHE 1

/* always defined to indicate that i18n is enabled */
#define ENABLE_NLS 1

/* Package name for gettext */
#define GETTEXT_PACKAGE "evolution-exchange-2.32"

/* Define it once memory returned by libical is free'ed properly */
#define HANDLE_LIBICAL_MEMORY 1

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#define HAVE_BIND_TEXTDOMAIN_CODESET 1

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Have <comm_err.h> */
/* #undef HAVE_COM_ERR_H */

/* Define to 1 if you have the `dcgettext' function. */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Have <et/comm_err.h> */
/* #undef HAVE_ET_COM_ERR_H */

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* Define if you have Heimdal */
/* #undef HAVE_HEIMDAL_KRB5 */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have Krb5 */
/* #undef HAVE_KRB5 */

/* Define if your <locale.h> file defines LC_MESSAGES. */
/* #undef HAVE_LC_MESSAGES */

/* Define if you have LDAP support */
#define HAVE_LDAP 1

/* Define to 1 if you have the `ldap_ntlm_bind' function. */
/* #undef HAVE_LDAP_NTLM_BIND */

/* Supports Paged results */
#define HAVE_LDAP_PAGED 1

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have MIT Krb5 */
/* #undef HAVE_MIT_KRB5 */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define if you have Sun Kerberosv5 */
/* #undef HAVE_SUN_KRB5 */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if running the test suite so that test #27 works on MinGW. */
/* #undef LT_MINGW_STATIC_TESTSUITE_HACK */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Name of package */
#define PACKAGE "evolution-exchange"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "http://bugzilla.gnome.org/enter_bug.cgi?product=Evolution%20Exchange"

/* Define to the full name of this package. */
#define PACKAGE_NAME "evolution-exchange"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "evolution-exchange 2.32.3"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "evolution-exchange"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.32.3"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you use SunLDAP */
/* #undef SUNLDAP */

/* Version number of package */
#define VERSION "2.32.3"

/* Define to "int" if socklen_t is not defined */
/* #undef socklen_t */

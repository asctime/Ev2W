/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* always defined to indicate that i18n is enabled */
#define ENABLE_NLS 1

/* evolution mail 2.12 present */
#define EVOLUTION_2_12 1

/* definition of GETTEXT_PACKAGE */
#define GETTEXT_PACKAGE "evolution-rss"

/* Define the location where the catalogs will be installed */
#define GNOMELOCALEDIR "/MSYS2/mingw64/share/locale"

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#define HAVE_BIND_TEXTDOMAIN_CODESET 1

/* workarund for a bug in shutdown gecko */
/* #undef HAVE_BUGGY_GECKO */

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Define to 1 if you have the `dcgettext' function. */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* gecko render engine present */
/* #undef HAVE_GECKO */

/* Define if we have gecko 1.7 */
/* #undef HAVE_GECKO_1_7 */

/* Define if we have gecko 1.8 */
/* #undef HAVE_GECKO_1_8 */

/* Define if we have gecko 1.8.1 */
/* #undef HAVE_GECKO_1_8_1 */

/* at least gecko 1.9 */
/* #undef HAVE_GECKO_1_9 */

/* Define if we have gecko 1.9.1 */
/* #undef HAVE_GECKO_1_9_1 */

/* Define if gecko is a debug build */
/* #undef HAVE_GECKO_DEBUG */

/* Define if xpcom glue is used */
/* #undef HAVE_GECKO_XPCOM_GLUE */

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* gthtml editor component present */
#define HAVE_GTKHTMLEDITOR 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if your <locale.h> file defines LC_MESSAGES. */
/* #undef HAVE_LC_MESSAGES */

/* libsoup-gnome library present */
#define HAVE_LIBSOUP_GNOME 1

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if mozilla is of the toolkit flavour */
/* #undef HAVE_MOZILLA_TOOLKIT */

/* either webkit or gecko render engines are present */
/* #undef HAVE_RENDERKIT */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* webkit render engine present */
/* #undef HAVE_WEBKIT */

/* gecko flavour is xulrunner */
/* #undef HAVE_XULRUNNER */

/* Define if running the test suite so that test #27 works on MinGW. */
/* #undef LT_MINGW_STATIC_TESTSUITE_HACK */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "evolution-rss"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "http://bugzilla.gnome.org/enter_bug.cgi?product=evolution-rss"

/* Define to the full name of this package. */
#define PACKAGE_NAME "evolution-rss"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "evolution-rss 0.2.6"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "evolution-rss"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.2.6"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.2.6"

/* unstandard webkit installation */
/* #undef WEBKIT_UNSTD */

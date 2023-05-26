/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Base version (Major.Minor) */
#define BASE_VERSION "2.32"

/* Solaris-style ctime_r */
/* #undef CTIME_R_THREE_ARGS */

/* Define if Mono embedding should be enabled */
/* #undef ENABLE_MONO */

/* always defined to indicate that i18n is enabled */
#define ENABLE_NLS 1

/* Profiling Hooks Enabled */
/* #undef ENABLE_PROFILING */

/* Define if SMIME should be enabled */
#define ENABLE_SMIME 1

/* Solaris-style gethostbyaddr_r */
/* #undef GETHOSTBYADDR_R_SEVEN_ARGS */

/* Solaris-style gethostbyname_r */
/* #undef GETHOSTBYNAME_R_FIVE_ARGS */

/* Package name for gettext */
#define GETTEXT_PACKAGE "evolution-2.32"

/* Define it once memory returned by libical is free'ed properly */
#define HANDLE_LIBICAL_MEMORY 1

/* Define if libc defines an altzone variable */
/* #undef HAVE_ALTZONE */

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#define HAVE_BIND_TEXTDOMAIN_CODESET 1

/* Define if using Canberra and Canberra-GTK for sound */
/* #undef HAVE_CANBERRA */

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Clutter not available */
#define HAVE_CLUTTER 0

/* Have <comm_err.h> */
/* #undef HAVE_COM_ERR_H */

/* Define to 1 if you have the `dcgettext' function. */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Have <et/comm_err.h> */
/* #undef HAVE_ET_COM_ERR_H */

/* Have <eventsys.h> */
#define HAVE_EVENTSYS_H 1

/* Define to 1 if you have the `gethostbyaddr_r' function. */
/* #undef HAVE_GETHOSTBYADDR_R */

/* Define to 1 if you have the `gethostbyname_r' function. */
/* #undef HAVE_GETHOSTBYNAME_R */

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* Define to 1 if you have the `gnu_get_libc_version' function. */
/* #undef HAVE_GNU_GET_LIBC_VERSION */

/* Define if you have Heimdal */
/* #undef HAVE_HEIMDAL_KRB5 */

/* libical provides ical_set_unknown_token_handling_setting function */
#define HAVE_ICAL_UNKNOWN_TOKEN_HANDLING 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `isblank' function. */
#define HAVE_ISBLANK 1

/* Define if you have Krb5 */
/* #undef HAVE_KRB5 */

/* Define if your <locale.h> file defines LC_MESSAGES. */
/* #undef HAVE_LC_MESSAGES */

/* Define if you have LDAP support */
#define HAVE_LDAP 1

/* Define to 1 if you have the `ldap_ntlm_bind' function. */
/* #undef HAVE_LDAP_NTLM_BIND */

/* libnotify available */
#define HAVE_LIBNOTIFY 1

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have MIT Krb5 */
/* #undef HAVE_MIT_KRB5 */

/* Define to 1 if you have the `mkdtemp' function. */
/* #undef HAVE_MKDTEMP */

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if you have the <nspr.h> header file. */
/* #undef HAVE_NSPR_H */

/* Define if you have NSS */
#define HAVE_NSS 1

/* Define to 1 if you have the <nss.h> header file. */
/* #undef HAVE_NSS_H */

/* Define to 1 if you have the <prio.h> header file. */
/* #undef HAVE_PRIO_H */

/* Define to 1 if you have the regexec function. */
#define HAVE_REGEXEC 1

/* Have <sensevts.h> */
#define HAVE_SENSEVTS_H 1

/* Define to 1 if you have the <smime.h> header file. */
/* #undef HAVE_SMIME_H */

/* Define if you have a supported SSL library */
#define HAVE_SSL 1

/* Define to 1 if you have the <ssl.h> header file. */
/* #undef HAVE_SSL_H */

/* Define to 1 if you have the `statfs' function. */
/* #undef HAVE_STATFS */

/* Define to 1 if you have the `statvfs' function. */
/* #undef HAVE_STATVFS */

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

/* Have <sys/mount.h> */
/* #undef HAVE_SYS_MOUNT_H */

/* Have <sys/param.h> */
#define HAVE_SYS_PARAM_H 1

/* Have <sys/statvfs.h> */
/* #undef HAVE_SYS_STATVFS_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
/* #undef HAVE_SYS_WAIT_H */

/* Define if libc defines a timezone variable */
#define HAVE_TIMEZONE 1

/* Define if struct tm has a tm_gmtoff member */
/* #undef HAVE_TM_GMTOFF */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* defined if you have X11/XF86keysym.h */
/* #undef HAVE_XFREE */

/* Command to kill processes by name */
/* #undef KILL_PROCESS_CMD */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to the full path of mozilla nss library */
#define MOZILLA_NSS_LIB_DIR "D:/MSYS2/mingw64/bin/../lib"

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Name of package */
#define PACKAGE "evolution"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "http://bugzilla.gnome.org/enter_bug.cgi?product=Evolution"

/* Define to the full name of this package. */
#define PACKAGE_NAME "evolution"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "evolution 2.32.3"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "evolution"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.32.3"

/* Define to the latest stable version if this version is unstable */
/* #undef STABLE_VERSION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version substring, for packagers */
#define SUB_VERSION ""

/* Define if you use SunLDAP */
/* #undef SUNLDAP */

/* The number of times we've upgraded since the BASE_VERSION release */
#define UPGRADE_REVISION "0"

/* Version number of package */
#define VERSION "2.32.3"

/* Define if you want a comment appended to the version number */
#define VERSION_COMMENT ""



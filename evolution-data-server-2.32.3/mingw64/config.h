/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* API version (Major.Minor) */
#define API_VERSION "1.2"

/* Base version (Major.Minor) */
#define BASE_VERSION "2.32"

/* Define if you have NSS */
#define CAMEL_HAVE_NSS 1

/* Define if you have a supported SSL library */
#define CAMEL_HAVE_SSL 1

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Solaris-style ctime_r */
/* #undef CTIME_R_THREE_ARGS */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Define if Calendar should be built */
#define ENABLE_CALENDAR 1

/* Enable IPv6 support */
#define ENABLE_IPv6 1

/* always defined to indicate that i18n is enabled */
#define ENABLE_NLS 1

/* Build NNTP backend */
#define ENABLE_NNTP 1

/* Define if SMIME should be enabled */
#define ENABLE_SMIME 1

/* Required */
#define ENABLE_THREADS 1

/* Solaris-style gethostbyaddr_r */
/* #undef GETHOSTBYADDR_R_SEVEN_ARGS */

/* Solaris-style gethostbyname_r */
/* #undef GETHOSTBYNAME_R_FIVE_ARGS */

/* Package name for gettext */
#define GETTEXT_PACKAGE "evolution-data-server-2.32"

/* Define it once memory returned by libical is free'ed properly */
#define HANDLE_LIBICAL_MEMORY 1

/* Define if the system defines the AI_ADDRCONFIG flag for getaddrinfo */
/* #undef HAVE_AI_ADDRCONFIG */

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
/* #undef HAVE_ALLOCA_H */

/* Define if libc defines an altzone variable */
/* #undef HAVE_ALTZONE */

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#define HAVE_BIND_TEXTDOMAIN_CODESET 1

/* Define if mail delivered to the system mail directory is in broken
   Content-Length format */
/* #undef HAVE_BROKEN_SPOOL */

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Have nl_langinfo (CODESET) */
/* #undef HAVE_CODESET */

/* Have <com_err.h> */
/* #undef HAVE_COM_ERR_H */

/* Define to 1 if you have the `dcgettext' function. */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Have <et/com_err.h> */
/* #undef HAVE_ET_COM_ERR_H */

/* Define to 1 if you have the `fsync' function. */
/* #undef HAVE_FSYNC */

/* libgdata is 0.7 or higher */
/* #undef HAVE_GDATA_07 */

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

/* Define if you have Krb5 */
/* #undef HAVE_KRB5 */

/* Define if your <locale.h> file defines LC_MESSAGES. */
/* #undef HAVE_LC_MESSAGES */

/* Define if you have LDAP support */
#define HAVE_LDAP 1

/* strftime supports use of l and k */
/* #undef HAVE_LKSTRFTIME */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have MIT Krb5 */
/* #undef HAVE_MIT_KRB5 */

/* Define to 1 if you have the `nl_langinfo' function. */
/* #undef HAVE_NL_LANGINFO */

/* Define to 1 if you have the <nspr.h> header file. */
/* #undef HAVE_NSPR_H */

/* Define to 1 if you have the <nss.h> header file. */
/* #undef HAVE_NSS_H */

/* Define to 1 if you have the <prio.h> header file. */
/* #undef HAVE_PRIO_H */

/* Define to 1 if you have the regexec function. */
#define HAVE_REGEXEC 1

/* Define to 1 if you have the <smime.h> header file. */
/* #undef HAVE_SMIME_H */

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

/* Define to 1 if you have the `strptime' function. */
/* #undef HAVE_STRPTIME */

/* Define to 1 if you have the `strtok_r' function. */
#define HAVE_STRTOK_R 1

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

/* Have <wspiapi.h> */
#define HAVE_WSPIAPI_H 1

/* IDL interface version (Major.Minor) */
#define INTERFACE_VERSION "2.32"

/* Define if running the test suite so that test #27 works on MinGW. */
/* #undef LT_MINGW_STATIC_TESTSUITE_HACK */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Enable getaddrinfo emulation */
/* #undef NEED_ADDRINFO */

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Define to 0 if your system does not have the O_LARGEFILE flag */
#define O_LARGEFILE 0

/* Name of package */
#define PACKAGE "evolution-data-server"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "http://bugzilla.gnome.org/enter_bug.cgi?product=Evolution-Data-Server"

/* Define to the full name of this package. */
#define PACKAGE_NAME "evolution-data-server"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "evolution-data-server 2.32.3"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "evolution-data-server"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.32.3"

/* Path to a sendmail binary, or equivalent */
#define SENDMAIL_PATH "/usr/sbin/sendmail"

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you use SunLDAP */
/* #undef SUNLDAP */

/* Directory local mail is delivered to */
#define SYSTEM_MAIL_DIR "/var/mail"

/* Define to use dot locking for mbox files */
/* #undef USE_DOT */

/* Define to use fcntl locking for mbox files */
/* #undef USE_FCNTL */

/* Define to use flock locking for mbox files */
/* #undef USE_FLOCK */

/* Version number of package */
#define VERSION "2.32.3"

/* Gnome Keyring available */
/* #undef WITH_GNOME_KEYRING */

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#define YYTEXT_POINTER 1

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* To get getaddrinfo etc declarations */
#define _WIN32_WINNT 0x501

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

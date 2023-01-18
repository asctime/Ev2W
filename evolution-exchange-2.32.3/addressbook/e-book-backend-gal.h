/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2000-2004 Novell, Inc. */

#ifndef __E_BOOK_BACKEND_GAL_H__
#define __E_BOOK_BACKEND_GAL_H__

#include "libedata-book/e-book-backend.h"

#if defined(SUNLDAP) || defined(G_OS_WIN32)
/*   copy from openldap ldap.h   */
#define LDAP_RANGE(n,x,y)      (((x) <= (n)) && ((n) <= (y)))
#ifndef LDAP_NAME_ERROR
#define LDAP_NAME_ERROR(n)     LDAP_RANGE((n), 0x20, 0x24)
#endif
#ifndef LBER_USE_DER
#define LBER_USE_DER			0x01
#endif
#ifndef LDAP_CONTROL_PAGEDRESULTS
#define LDAP_CONTROL_PAGEDRESULTS      "1.2.840.113556.1.4.319"
#endif
#endif

#ifndef LDAP_TYPE_OR_VALUE_EXISTS
#define LDAP_TYPE_OR_VALUE_EXISTS 0x14
#endif
#ifndef LDAP_SCOPE_DEFAULT
#define LDAP_SCOPE_DEFAULT -1
#endif
#ifndef LDAP_OPT_SUCCESS
#define LDAP_OPT_SUCCESS 0x00
#endif
#ifndef LDAP_INSUFFICIENT_ACCESS
#define LDAP_INSUFFICIENT_ACCESS 0x32
#endif
#ifndef LDAP_OPT_SERVER_CONTROLS
#define LDAP_OPT_SERVER_CONTROLS 0x12
#endif

#ifdef G_OS_WIN32
/* map between the WinLDAP API and OpenLDAP API */
#  ifndef ldap_msgtype
#    define ldap_msgtype(m) ((m)->lm_msgtype)
#  endif

#  ifndef ldap_first_message
#    define ldap_first_message ldap_first_entry
#  endif

#  ifndef ldap_next_message
#    define ldap_next_message ldap_next_entry
#  endif

#  ifndef LDAP_RES_MODDN
#    define LDAP_RES_MODDN LDAP_RES_MODRDN
#  endif

#  ifdef ldap_compare_ext
#    undef ldap_compare_ext
#  endif
#  ifdef ldap_search_ext
#    undef ldap_search_ext
#  endif
#  ifdef ldap_start_tls_s
#    undef ldap_start_tls_s
#  endif

#  ifdef UNICODE
#    define ldap_compare_ext(ld,dn,a,v,sc,cc,msg) \
        ldap_compare_extW(ld,dn,a,0,v,sc,cc,msg)
#    define ldap_search_ext(ld,base,scope,f,a,o,sc,cc,(t),s,msg) \
        ldap_search_extW(ld,base,scope,f,a,o,sc,cc,((PLDAP_TIMEVAL)t)?((PLDAP_TIMEVAL)t)->tv_sec:0,s,msg)
#    define ldap_start_tls_s(ld,sc,cc) \
        ldap_start_tls_sW(ld,0,0,sc,cc)
#  else /* !UNICODE */
#    define ldap_compare_ext(ld,dn,a,v,sc,cc,msg) \
        ldap_compare_extA(ld,dn,a,0,v,sc,cc,msg)
#    define ldap_search_ext(ld,base,scope,f,a,o,sc,cc,t,s,msg) \
        ldap_search_extA(ld,base,scope,f,a,o,sc,cc,((PLDAP_TIMEVAL)t)?((PLDAP_TIMEVAL)t)->tv_sec:0,s,msg)
#    define ldap_start_tls_s(ld,sc,cc) \
        ldap_start_tls_sA(ld,0,0,sc,cc)
#  endif /* UNICODE */
#endif /* G_OS_WIN32 */

typedef struct _EBookBackendGALPrivate EBookBackendGALPrivate;

typedef struct {
	EBookBackend             parent_object;
	EBookBackendGALPrivate *priv;
} EBookBackendGAL;

typedef struct {
	EBookBackendClass parent_class;
} EBookBackendGALClass;

EBookBackend *e_book_backend_gal_new      (void);
GType       e_book_backend_gal_get_type (void);

#define E_TYPE_BOOK_BACKEND_GAL        (e_book_backend_gal_get_type ())
#define E_BOOK_BACKEND_GAL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_BOOK_BACKEND_GAL, EBookBackendGAL))
#define E_BOOK_BACKEND_GAL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_BOOK_BACKEND_GAL, EBookBackendGALClass))
#define E_IS_BOOK_BACKEND_GAL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_BOOK_BACKEND_GAL))
#define E_IS_BOOK_BACKEND_GAL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_BOOK_BACKEND_GAL))

#endif /* __E_BOOK_BACKEND_GAL_H__ */


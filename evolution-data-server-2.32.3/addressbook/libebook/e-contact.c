/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Chris Toshok (toshok@ximian.com)
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include "e-contact.h"
#include "e-book.h"
#include "e-name-western.h"

#ifdef G_OS_WIN32
#include "libedataserver/e-data-server-util.h"
#undef LOCALEDIR
#define LOCALEDIR e_util_get_localedir ()
#endif

#define d(x)

G_DEFINE_TYPE (EContact, e_contact, E_TYPE_VCARD)

struct _EContactPrivate {
	gchar *cached_strings[E_CONTACT_FIELD_LAST];
};

#define E_CONTACT_FIELD_TYPE_STRING       0x00000001   /* used for simple single valued attributes */
/*E_CONTACT_FIELD_TYPE_FLOAT*/
#define E_CONTACT_FIELD_TYPE_LIST         0x00000002   /* used for multivalued single attributes - the elements are of type gchar * */
#define E_CONTACT_FIELD_TYPE_MULTI        0x00000004   /* used for multivalued attributes - the elements are of type EVCardAttribute */
#define E_CONTACT_FIELD_TYPE_GETSET       0x00000008   /* used for attributes that need custom handling for getting/setting */
#define E_CONTACT_FIELD_TYPE_STRUCT       0x00000010   /* used for structured types (N and ADR properties, in particular) */
#define E_CONTACT_FIELD_TYPE_BOOLEAN      0x00000020   /* used for boolean types (WANTS_HTML) */

#define E_CONTACT_FIELD_TYPE_SYNTHETIC    0x10000000   /* used when there isn't a corresponding vcard field (such as email_1) */
#define E_CONTACT_FIELD_TYPE_LIST_ELEM    0x20000000   /* used when a synthetic attribute is a numbered list element */
#define E_CONTACT_FIELD_TYPE_MULTI_ELEM   0x40000000   /* used when we're looking for the nth attribute where more than 1 can be present in the vcard */
#define E_CONTACT_FIELD_TYPE_ATTR_TYPE    0x80000000   /* used when a synthetic attribute is flagged with a TYPE= that we'll be looking for */

typedef struct {
	guint32 t;

	EContactField field_id;
	const gchar *vcard_field_name;
	const gchar *field_name;      /* non translated */
	const gchar *pretty_name;     /* translated */

	gboolean read_only;

	gint list_elem;
	const gchar *attr_type1;
	const gchar *attr_type2;

	gpointer  (*struct_getter)(EContact *contact, EVCardAttribute *attribute);
	void (*struct_setter)(EContact *contact, EVCardAttribute *attribute, gpointer data);

	GType (*boxed_type_getter) (void);
} EContactFieldInfo;

static gpointer  photo_getter (EContact *contact, EVCardAttribute *attr);
static void photo_setter (EContact *contact, EVCardAttribute *attr, gpointer data);
static gpointer  geo_getter (EContact *contact, EVCardAttribute *attr);
static void geo_setter (EContact *contact, EVCardAttribute *attr, gpointer data);
static gpointer  fn_getter (EContact *contact, EVCardAttribute *attr);
static void fn_setter (EContact *contact, EVCardAttribute *attr, gpointer data);
static gpointer  n_getter (EContact *contact, EVCardAttribute *attr);
static void n_setter (EContact *contact, EVCardAttribute *attr, gpointer data);
static gpointer  adr_getter (EContact *contact, EVCardAttribute *attr);
static void adr_setter (EContact *contact, EVCardAttribute *attr, gpointer data);
static gpointer  date_getter (EContact *contact, EVCardAttribute *attr);
static void date_setter (EContact *contact, EVCardAttribute *attr, gpointer data);
static gpointer  cert_getter (EContact *contact, EVCardAttribute *attr);
static void cert_setter (EContact *contact, EVCardAttribute *attr, gpointer data);

#define STRING_FIELD(id,vc,n,pn,ro)  { E_CONTACT_FIELD_TYPE_STRING, (id), (vc), (n), (pn), (ro) }
#define BOOLEAN_FIELD(id,vc,n,pn,ro)  { E_CONTACT_FIELD_TYPE_BOOLEAN, (id), (vc), (n), (pn), (ro) }
#define LIST_FIELD(id,vc,n,pn,ro)      { E_CONTACT_FIELD_TYPE_LIST, (id), (vc), (n), (pn), (ro) }
#define MULTI_LIST_FIELD(id,vc,n,pn,ro) { E_CONTACT_FIELD_TYPE_MULTI, (id), (vc), (n), (pn), (ro) }
#define GETSET_FIELD(id,vc,n,pn,ro,get,set)    { E_CONTACT_FIELD_TYPE_STRING | E_CONTACT_FIELD_TYPE_GETSET, (id), (vc), (n), (pn), (ro), -1, NULL, NULL, (get), (set) }
#define STRUCT_FIELD(id,vc,n,pn,ro,get,set,ty)    { E_CONTACT_FIELD_TYPE_STRUCT | E_CONTACT_FIELD_TYPE_GETSET, (id), (vc), (n), (pn), (ro), -1, NULL, NULL, (get), (set), (ty) }
#define SYNTH_STR_FIELD(id,n,pn,ro)  { E_CONTACT_FIELD_TYPE_STRING | E_CONTACT_FIELD_TYPE_SYNTHETIC, (id), NULL, (n), (pn), (ro) }
#define LIST_ELEM_STR_FIELD(id,vc,n,pn,ro,nm) { E_CONTACT_FIELD_TYPE_LIST_ELEM | E_CONTACT_FIELD_TYPE_SYNTHETIC | E_CONTACT_FIELD_TYPE_STRING, (id), (vc), (n), (pn), (ro), (nm) }
#define MULTI_ELEM_STR_FIELD(id,vc,n,pn,ro,nm) { E_CONTACT_FIELD_TYPE_MULTI_ELEM | E_CONTACT_FIELD_TYPE_SYNTHETIC | E_CONTACT_FIELD_TYPE_STRING, (id), (vc), (n), (pn), (ro), (nm) }
#define ATTR_TYPE_STR_FIELD(id,vc,n,pn,ro,at1,nth) { E_CONTACT_FIELD_TYPE_ATTR_TYPE | E_CONTACT_FIELD_TYPE_SYNTHETIC | E_CONTACT_FIELD_TYPE_STRING, (id), (vc), (n), (pn), (ro), (nth), (at1), NULL }
#define ATTR_TYPE_GETSET_FIELD(id,vc,n,pn,ro,at1,nth,get,set) { E_CONTACT_FIELD_TYPE_ATTR_TYPE | E_CONTACT_FIELD_TYPE_GETSET, (id), (vc), (n), (pn), (ro), (nth), (at1), NULL, (get), (set) }
#define ATTR2_TYPE_STR_FIELD(id,vc,n,pn,ro,at1,at2,nth) { E_CONTACT_FIELD_TYPE_ATTR_TYPE | E_CONTACT_FIELD_TYPE_SYNTHETIC | E_CONTACT_FIELD_TYPE_STRING, (id), (vc), (n), (pn), (ro), (nth), (at1), (at2) }
#define ATTR_TYPE_STRUCT_FIELD(id,vc,n,pn,ro,at,get,set,ty) { E_CONTACT_FIELD_TYPE_ATTR_TYPE | E_CONTACT_FIELD_TYPE_SYNTHETIC | E_CONTACT_FIELD_TYPE_GETSET | E_CONTACT_FIELD_TYPE_STRUCT, (id), (vc), (n), (pn), (ro), 0, (at), NULL, (get), (set), (ty) }

/* This *must* be kept in the same order as the EContactField enum */
static const EContactFieldInfo field_info[] = {
	{0,}, /* Dummy row as EContactField starts from 1 */
	STRING_FIELD (E_CONTACT_UID,        EVC_UID,       "id",         N_("Unique ID"),  FALSE),
	STRING_FIELD (E_CONTACT_FILE_AS,    EVC_X_FILE_AS, "file_as",    N_("File Under"),    FALSE),
	/* URI of the book to which the contact belongs to */
	STRING_FIELD (E_CONTACT_BOOK_URI, EVC_X_BOOK_URI, "book_uri", N_("Book URI"), FALSE),

	/* Name fields */
	/* FN isn't really a structured field - we use a getter/setter
	   so we can set the N property (since evo 1.4 works fine with
	   vcards that don't even have a N attribute.  *sigh*) */
	GETSET_FIELD        (E_CONTACT_FULL_NAME,   EVC_FN,       "full_name",   N_("Full Name"),   FALSE, fn_getter, fn_setter),
	LIST_ELEM_STR_FIELD (E_CONTACT_GIVEN_NAME,  EVC_N,        "given_name",  N_("Given Name"),  FALSE, 1),
	LIST_ELEM_STR_FIELD (E_CONTACT_FAMILY_NAME, EVC_N,        "family_name", N_("Family Name"), FALSE, 0),
	STRING_FIELD        (E_CONTACT_NICKNAME,    EVC_NICKNAME, "nickname",    N_("Nickname"),    FALSE),

	/* Email fields */
	MULTI_ELEM_STR_FIELD (E_CONTACT_EMAIL_1,    EVC_EMAIL,        "email_1",    N_("Email 1"),         FALSE, 0),
	MULTI_ELEM_STR_FIELD (E_CONTACT_EMAIL_2,    EVC_EMAIL,        "email_2",    N_("Email 2"),         FALSE, 1),
	MULTI_ELEM_STR_FIELD (E_CONTACT_EMAIL_3,    EVC_EMAIL,        "email_3",    N_("Email 3"),         FALSE, 2),
	MULTI_ELEM_STR_FIELD (E_CONTACT_EMAIL_4,    EVC_EMAIL,        "email_4",    N_("Email 4"),         FALSE, 3),

	STRING_FIELD         (E_CONTACT_MAILER,     EVC_MAILER,       "mailer",     N_("Mailer"),          FALSE),

	/* Address Labels */
	ATTR_TYPE_STR_FIELD (E_CONTACT_ADDRESS_LABEL_HOME,  EVC_LABEL, "address_label_home",  N_("Home Address Label"),  FALSE, "HOME", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_ADDRESS_LABEL_WORK,  EVC_LABEL, "address_label_work",  N_("Work Address Label"),  FALSE, "WORK", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_ADDRESS_LABEL_OTHER, EVC_LABEL, "address_label_other", N_("Other Address Label"), FALSE, "OTHER", 0),

	/* Phone fields */
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_ASSISTANT,    EVC_TEL, "assistant_phone",   N_("Assistant Phone"),  FALSE, EVC_X_ASSISTANT, 0),
	ATTR2_TYPE_STR_FIELD (E_CONTACT_PHONE_BUSINESS,     EVC_TEL, "business_phone",    N_("Business Phone"),   FALSE, "WORK", "VOICE",         0),
	ATTR2_TYPE_STR_FIELD (E_CONTACT_PHONE_BUSINESS_2,   EVC_TEL, "business_phone_2",  N_("Business Phone 2"), FALSE, "WORK", "VOICE",         1),
	ATTR2_TYPE_STR_FIELD (E_CONTACT_PHONE_BUSINESS_FAX, EVC_TEL, "business_fax",      N_("Business Fax"),     FALSE, "WORK", "FAX",           0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_CALLBACK,     EVC_TEL, "callback_phone",    N_("Callback Phone"),   FALSE, EVC_X_CALLBACK,  0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_CAR,          EVC_TEL, "car_phone",         N_("Car Phone"),        FALSE, "CAR",                   0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_COMPANY,      EVC_TEL, "company_phone",     N_("Company Phone"),    FALSE, EVC_X_COMPANY,   0),
	ATTR2_TYPE_STR_FIELD (E_CONTACT_PHONE_HOME,         EVC_TEL, "home_phone",        N_("Home Phone"),       FALSE, "HOME", "VOICE",         0),
	ATTR2_TYPE_STR_FIELD (E_CONTACT_PHONE_HOME_2,       EVC_TEL, "home_phone_2",      N_("Home Phone 2"),     FALSE, "HOME", "VOICE",         1),
	ATTR2_TYPE_STR_FIELD (E_CONTACT_PHONE_HOME_FAX,     EVC_TEL, "home_fax",          N_("Home Fax"),         FALSE, "HOME", "FAX",           0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_ISDN,         EVC_TEL, "isdn_phone",        N_("ISDN"),             FALSE, "ISDN",                  0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_MOBILE,       EVC_TEL, "mobile_phone",      N_("Mobile Phone"),     FALSE, "CELL",                  0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_OTHER,        EVC_TEL, "other_phone",       N_("Other Phone"),      FALSE, "VOICE",                 0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_OTHER_FAX,    EVC_TEL, "other_fax",         N_("Other Fax"),        FALSE, "FAX",                   0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_PAGER,        EVC_TEL, "pager",             N_("Pager"),            FALSE, "PAGER",                 0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_PRIMARY,      EVC_TEL, "primary_phone",     N_("Primary Phone"),    FALSE, "PREF",                  0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_RADIO,        EVC_TEL, "radio",             N_("Radio"),            FALSE, EVC_X_RADIO,     0),
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_TELEX,        EVC_TEL, "telex",             N_("Telex"),            FALSE, EVC_X_TELEX,     0),
	/* To translators: TTY is Teletypewriter */
	ATTR_TYPE_STR_FIELD  (E_CONTACT_PHONE_TTYTDD,       EVC_TEL, "tty",               N_("TTY"),              FALSE, EVC_X_TTYTDD,    0),

	/* Organizational fields */
	LIST_ELEM_STR_FIELD (E_CONTACT_ORG,      EVC_ORG, "org",      N_("Organization"),        FALSE, 0),
	LIST_ELEM_STR_FIELD (E_CONTACT_ORG_UNIT, EVC_ORG, "org_unit", N_("Organizational Unit"), FALSE, 1),
	LIST_ELEM_STR_FIELD (E_CONTACT_OFFICE,   EVC_ORG, "office",   N_("Office"),              FALSE, 2),
	STRING_FIELD    (E_CONTACT_TITLE,     EVC_TITLE,       "title",     N_("Title"),           FALSE),
	STRING_FIELD    (E_CONTACT_ROLE,      EVC_ROLE,        "role",      N_("Role"),            FALSE),
	STRING_FIELD    (E_CONTACT_MANAGER,   EVC_X_MANAGER,   "manager",   N_("Manager"),         FALSE),
	STRING_FIELD    (E_CONTACT_ASSISTANT, EVC_X_ASSISTANT, "assistant", N_("Assistant"),       FALSE),

	/* Web fields */
	STRING_FIELD (E_CONTACT_HOMEPAGE_URL, EVC_URL,         "homepage_url", N_("Homepage URL"), FALSE),
	STRING_FIELD (E_CONTACT_BLOG_URL,     EVC_X_BLOG_URL,  "blog_url",     N_("Weblog URL"),   FALSE),

	/* Contact categories */
	SYNTH_STR_FIELD (E_CONTACT_CATEGORIES,                    "categories",    N_("Categories"),    FALSE),

	/* Collaboration fields */
	STRING_FIELD (E_CONTACT_CALENDAR_URI, EVC_CALURI,      "caluri",     N_("Calendar URI"),  FALSE),
	STRING_FIELD (E_CONTACT_FREEBUSY_URL, EVC_FBURL,       "fburl",       N_("Free/Busy URL"), FALSE),
	STRING_FIELD (E_CONTACT_ICS_CALENDAR, EVC_ICSCALENDAR, "icscalendar", N_("ICS Calendar"),  FALSE),
	STRING_FIELD (E_CONTACT_VIDEO_URL,    EVC_X_VIDEO_URL, "video_url",    N_("Video Conferencing URL"),   FALSE),

	/* Misc fields */
	STRING_FIELD (E_CONTACT_SPOUSE, EVC_X_SPOUSE,    "spouse", N_("Spouse's Name"), FALSE),
	STRING_FIELD (E_CONTACT_NOTE,   EVC_NOTE,        "note",   N_("Note"),          FALSE),

	/* Instant messaging fields */
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_AIM_HOME_1,    EVC_X_AIM,    "im_aim_home_1",    N_("AIM Home Screen Name 1"),    FALSE, "HOME", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_AIM_HOME_2,    EVC_X_AIM,    "im_aim_home_2",    N_("AIM Home Screen Name 2"),    FALSE, "HOME", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_AIM_HOME_3,    EVC_X_AIM,    "im_aim_home_3",    N_("AIM Home Screen Name 3"),    FALSE, "HOME", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_AIM_WORK_1,    EVC_X_AIM,    "im_aim_work_1",    N_("AIM Work Screen Name 1"),    FALSE, "WORK", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_AIM_WORK_2,    EVC_X_AIM,    "im_aim_work_2",    N_("AIM Work Screen Name 2"),    FALSE, "WORK", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_AIM_WORK_3,    EVC_X_AIM,    "im_aim_work_3",    N_("AIM Work Screen Name 3"),    FALSE, "WORK", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GROUPWISE_HOME_1, EVC_X_GROUPWISE, "im_groupwise_home_1", N_("GroupWise Home Screen Name 1"),    FALSE, "HOME", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GROUPWISE_HOME_2, EVC_X_GROUPWISE, "im_groupwise_home_2", N_("GroupWise Home Screen Name 2"),    FALSE, "HOME", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GROUPWISE_HOME_3, EVC_X_GROUPWISE, "im_groupwise_home_3", N_("GroupWise Home Screen Name 3"),    FALSE, "HOME", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GROUPWISE_WORK_1, EVC_X_GROUPWISE, "im_groupwise_work_1", N_("GroupWise Work Screen Name 1"),    FALSE, "WORK", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GROUPWISE_WORK_2, EVC_X_GROUPWISE, "im_groupwise_work_2", N_("GroupWise Work Screen Name 2"),    FALSE, "WORK", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GROUPWISE_WORK_3, EVC_X_GROUPWISE, "im_groupwise_work_3", N_("GroupWise Work Screen Name 3"),    FALSE, "WORK", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_JABBER_HOME_1, EVC_X_JABBER, "im_jabber_home_1", N_("Jabber Home ID 1"),          FALSE, "HOME", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_JABBER_HOME_2, EVC_X_JABBER, "im_jabber_home_2", N_("Jabber Home ID 2"),          FALSE, "HOME", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_JABBER_HOME_3, EVC_X_JABBER, "im_jabber_home_3", N_("Jabber Home ID 3"),          FALSE, "HOME", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_JABBER_WORK_1, EVC_X_JABBER, "im_jabber_work_1", N_("Jabber Work ID 1"),          FALSE, "WORK", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_JABBER_WORK_2, EVC_X_JABBER, "im_jabber_work_3", N_("Jabber Work ID 2"),          FALSE, "WORK", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_JABBER_WORK_3, EVC_X_JABBER, "im_jabber_work_2", N_("Jabber Work ID 3"),          FALSE, "WORK", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_YAHOO_HOME_1,  EVC_X_YAHOO,  "im_yahoo_home_1",  N_("Yahoo! Home Screen Name 1"), FALSE, "HOME", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_YAHOO_HOME_2,  EVC_X_YAHOO,  "im_yahoo_home_2",  N_("Yahoo! Home Screen Name 2"), FALSE, "HOME", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_YAHOO_HOME_3,  EVC_X_YAHOO,  "im_yahoo_home_3",  N_("Yahoo! Home Screen Name 3"), FALSE, "HOME", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_YAHOO_WORK_1,  EVC_X_YAHOO,  "im_yahoo_work_1",  N_("Yahoo! Work Screen Name 1"), FALSE, "WORK", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_YAHOO_WORK_2,  EVC_X_YAHOO,  "im_yahoo_work_2",  N_("Yahoo! Work Screen Name 2"), FALSE, "WORK", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_YAHOO_WORK_3,  EVC_X_YAHOO,  "im_yahoo_work_3",  N_("Yahoo! Work Screen Name 3"), FALSE, "WORK", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_MSN_HOME_1,    EVC_X_MSN,    "im_msn_home_1",    N_("MSN Home Screen Name 1"),    FALSE, "HOME", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_MSN_HOME_2,    EVC_X_MSN,    "im_msn_home_2",    N_("MSN Home Screen Name 2"),    FALSE, "HOME", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_MSN_HOME_3,    EVC_X_MSN,    "im_msn_home_3",    N_("MSN Home Screen Name 3"),    FALSE, "HOME", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_MSN_WORK_1,    EVC_X_MSN,    "im_msn_work_1",    N_("MSN Work Screen Name 1"),    FALSE, "WORK", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_MSN_WORK_2,    EVC_X_MSN,    "im_msn_work_2",    N_("MSN Work Screen Name 2"),    FALSE, "WORK", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_MSN_WORK_3,    EVC_X_MSN,    "im_msn_work_3",    N_("MSN Work Screen Name 3"),    FALSE, "WORK", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_ICQ_HOME_1,    EVC_X_ICQ,    "im_icq_home_1",    N_("ICQ Home ID 1"),             FALSE, "HOME", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_ICQ_HOME_2,    EVC_X_ICQ,    "im_icq_home_2",    N_("ICQ Home ID 2"),             FALSE, "HOME", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_ICQ_HOME_3,    EVC_X_ICQ,    "im_icq_home_3",    N_("ICQ Home ID 3"),             FALSE, "HOME", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_ICQ_WORK_1,    EVC_X_ICQ,    "im_icq_work_1",    N_("ICQ Work ID 1"),             FALSE, "WORK", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_ICQ_WORK_2,    EVC_X_ICQ,    "im_icq_work_2",    N_("ICQ Work ID 2"),             FALSE, "WORK", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_ICQ_WORK_3,    EVC_X_ICQ,    "im_icq_work_3",    N_("ICQ Work ID 3"),             FALSE, "WORK", 2),

	/* Last modified time */
	STRING_FIELD (E_CONTACT_REV, EVC_REV, "Rev", N_("Last Revision"), FALSE),
	SYNTH_STR_FIELD     (E_CONTACT_NAME_OR_ORG,               "name_or_org", N_("Name or Org"), TRUE),

	/* Address fields */
	MULTI_LIST_FIELD       (E_CONTACT_ADDRESS,       EVC_ADR, "address",       N_("Address List"),  FALSE),
	ATTR_TYPE_STRUCT_FIELD (E_CONTACT_ADDRESS_HOME,  EVC_ADR, "address_home",  N_("Home Address"),  FALSE, "HOME",  adr_getter, adr_setter, e_contact_address_get_type),
	ATTR_TYPE_STRUCT_FIELD (E_CONTACT_ADDRESS_WORK,  EVC_ADR, "address_work",  N_("Work Address"),  FALSE, "WORK",  adr_getter, adr_setter, e_contact_address_get_type),
	ATTR_TYPE_STRUCT_FIELD (E_CONTACT_ADDRESS_OTHER, EVC_ADR, "address_other", N_("Other Address"), FALSE, "OTHER", adr_getter, adr_setter, e_contact_address_get_type),

	/* Contact categories */
	LIST_FIELD      (E_CONTACT_CATEGORY_LIST, EVC_CATEGORIES, "category_list", N_("Category List"), FALSE),

	/* Photo/Logo */
	STRUCT_FIELD    (E_CONTACT_PHOTO, EVC_PHOTO, "photo", N_("Photo"), FALSE, photo_getter, photo_setter, e_contact_photo_get_type),
	STRUCT_FIELD    (E_CONTACT_LOGO,  EVC_LOGO,  "logo",  N_("Logo"),  FALSE, photo_getter, photo_setter, e_contact_photo_get_type),

	STRUCT_FIELD        (E_CONTACT_NAME,        EVC_N,        "name",        N_("Name"),        FALSE, n_getter, n_setter, e_contact_name_get_type),
	MULTI_LIST_FIELD     (E_CONTACT_EMAIL,      EVC_EMAIL,        "email",      N_("Email List"),      FALSE),

	/* Instant messaging fields */
	MULTI_LIST_FIELD (E_CONTACT_IM_AIM,       EVC_X_AIM,       "im_aim",       N_("AIM Screen Name List"),    FALSE),
	MULTI_LIST_FIELD (E_CONTACT_IM_GROUPWISE, EVC_X_GROUPWISE, "im_groupwise", N_("GroupWise ID List"),       FALSE),
	MULTI_LIST_FIELD (E_CONTACT_IM_JABBER,	  EVC_X_JABBER,    "im_jabber",    N_("Jabber ID List"),          FALSE),
	MULTI_LIST_FIELD (E_CONTACT_IM_YAHOO,	  EVC_X_YAHOO,     "im_yahoo",     N_("Yahoo! Screen Name List"), FALSE),
	MULTI_LIST_FIELD (E_CONTACT_IM_MSN,	  EVC_X_MSN,       "im_msn",       N_("MSN Screen Name List"),    FALSE),
	MULTI_LIST_FIELD (E_CONTACT_IM_ICQ,	  EVC_X_ICQ,       "im_icq",       N_("ICQ ID List"),             FALSE),

	BOOLEAN_FIELD        (E_CONTACT_WANTS_HTML, EVC_X_WANTS_HTML, "wants_html", N_("Wants HTML Mail"), FALSE),

	BOOLEAN_FIELD (E_CONTACT_IS_LIST,             EVC_X_LIST, "list", N_("List"), FALSE),
	BOOLEAN_FIELD (E_CONTACT_LIST_SHOW_ADDRESSES, EVC_X_LIST_SHOW_ADDRESSES, "list_show_addresses", N_("List Show Addresses"), FALSE),

	STRUCT_FIELD (E_CONTACT_BIRTH_DATE,  EVC_BDAY,          "birth_date",  N_("Birth Date"), FALSE, date_getter, date_setter, e_contact_date_get_type),
	STRUCT_FIELD (E_CONTACT_ANNIVERSARY, EVC_X_ANNIVERSARY, "anniversary", N_("Anniversary"), FALSE, date_getter, date_setter, e_contact_date_get_type),

	/* Security fields */
	ATTR_TYPE_STRUCT_FIELD (E_CONTACT_X509_CERT,  EVC_KEY, "x509Cert",  N_("X.509 Certificate"), FALSE, "X509", cert_getter, cert_setter, e_contact_cert_get_type),

	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GADUGADU_HOME_1,  EVC_X_GADUGADU,  "im_gadugadu_home_1",  N_("Gadu-Gadu Home ID 1"), FALSE, "HOME", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GADUGADU_HOME_2,  EVC_X_GADUGADU,  "im_gadugadu_home_2",  N_("Gadu-Gadu Home ID 2"), FALSE, "HOME", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GADUGADU_HOME_3,  EVC_X_GADUGADU,  "im_gadugadu_home_3",  N_("Gadu-Gadu Home ID 3"), FALSE, "HOME", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GADUGADU_WORK_1,  EVC_X_GADUGADU,  "im_gadugadu_work_1",  N_("Gadu-Gadu Work ID 1"), FALSE, "WORK", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GADUGADU_WORK_2,  EVC_X_GADUGADU,  "im_gadugadu_work_2",  N_("Gadu-Gadu Work ID 2"), FALSE, "WORK", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_GADUGADU_WORK_3,  EVC_X_GADUGADU,  "im_gadugadu_work_3",  N_("Gadu-Gadu Work ID 3"), FALSE, "WORK", 2),
	MULTI_LIST_FIELD (E_CONTACT_IM_GADUGADU,  EVC_X_GADUGADU,  "im_gadugadu", N_("Gadu-Gadu ID List"), FALSE),

	/* Geo information */
	STRUCT_FIELD	(E_CONTACT_GEO,  EVC_GEO, "geo",  N_("Geographic Information"),  FALSE, geo_getter, geo_setter, e_contact_geo_get_type),

	MULTI_LIST_FIELD     (E_CONTACT_TEL,      EVC_TEL,        "phone",      N_("Telephone"),      FALSE),

	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_SKYPE_HOME_1,  EVC_X_SKYPE,  "im_skype_home_1",  N_("Skype Home Name 1"),         FALSE, "HOME", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_SKYPE_HOME_2,  EVC_X_SKYPE,  "im_skype_home_2",  N_("Skype Home Name 2"),         FALSE, "HOME", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_SKYPE_HOME_3,  EVC_X_SKYPE,  "im_skype_home_3",  N_("Skype Home Name 3"),         FALSE, "HOME", 2),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_SKYPE_WORK_1,  EVC_X_SKYPE,  "im_skype_work_1",  N_("Skype Work Name 1"),         FALSE, "WORK", 0),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_SKYPE_WORK_2,  EVC_X_SKYPE,  "im_skype_work_2",  N_("Skype Work Name 2"),         FALSE, "WORK", 1),
	ATTR_TYPE_STR_FIELD (E_CONTACT_IM_SKYPE_WORK_3,  EVC_X_SKYPE,  "im_skype_work_3",  N_("Skype Work Name 3"),         FALSE, "WORK", 2),
	MULTI_LIST_FIELD (E_CONTACT_IM_SKYPE,	  EVC_X_SKYPE,     "im_skype",     N_("Skype Name List"),         FALSE),

	MULTI_LIST_FIELD (E_CONTACT_SIP,	  EVC_X_SIP,    "sip",    N_("SIP address"),          FALSE),
};

#undef LIST_ELEM_STR_FIELD
#undef STRING_FIELD
#undef SYNTH_STR_FIELD
#undef LIST_FIELD
#undef GETSET_FIELD

static GObjectClass *parent_class;

static void e_contact_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_contact_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void
e_contact_finalize (GObject *object)
{
	EContact *ec = E_CONTACT (object);
	gint i;

	for (i = E_CONTACT_FIELD_FIRST; i < E_CONTACT_FIELD_LAST; i++) {
		g_free (ec->priv->cached_strings[i]);
	}

	if (ec->priv) {
		g_free (ec->priv);
		ec->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
e_contact_class_init (EContactClass *klass)
{
	GObjectClass *object_class;
	gint i;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (E_TYPE_VCARD);

	object_class->finalize = e_contact_finalize;
	object_class->set_property = e_contact_set_property;
	object_class->get_property = e_contact_get_property;

	for (i = E_CONTACT_FIELD_FIRST; i < E_CONTACT_FIELD_LAST; i++) {
		GParamSpec *pspec = NULL;

		/* Verify the table is correctly ordered */
		g_assert (i == field_info[i].field_id);

		if (field_info[i].t & E_CONTACT_FIELD_TYPE_STRING)
			pspec = g_param_spec_string (field_info[i].field_name,
						     _(field_info[i].pretty_name),
						    field_info[i].pretty_name,
						     NULL,
						     (field_info[i].read_only ? G_PARAM_READABLE : G_PARAM_READWRITE)
						     | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
		else if (field_info[i].t & E_CONTACT_FIELD_TYPE_BOOLEAN)
			pspec = g_param_spec_boolean (field_info[i].field_name,
						      _(field_info[i].pretty_name),
						    field_info[i].pretty_name,
						      FALSE,
						     (field_info[i].read_only ? G_PARAM_READABLE : G_PARAM_READWRITE)
						     | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
		else if (field_info[i].t & E_CONTACT_FIELD_TYPE_STRUCT)
			pspec = g_param_spec_boxed (field_info[i].field_name,
						    _(field_info[i].pretty_name),
						    field_info[i].pretty_name,
						    field_info[i].boxed_type_getter(),
						     (field_info[i].read_only ? G_PARAM_READABLE : G_PARAM_READWRITE)
						     | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
		else
			pspec = g_param_spec_pointer (field_info[i].field_name,
						      _(field_info[i].pretty_name),
						    field_info[i].pretty_name,
						     (field_info[i].read_only ? G_PARAM_READABLE : G_PARAM_READWRITE)
						     | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

		g_object_class_install_property (object_class, field_info[i].field_id,
						 pspec);
	}
}

static void
e_contact_init (EContact *ec)
{
	ec->priv = g_new0 (EContactPrivate, 1);
}

static EVCardAttribute*
e_contact_get_first_attr (EContact *contact, const gchar *attr_name)
{
	GList *attrs, *l;

	attrs = e_vcard_get_attributes (E_VCARD (contact));

	for (l = attrs; l; l = l->next) {
		EVCardAttribute *attr = l->data;
		const gchar *name;

		name = e_vcard_attribute_get_name (attr);

		if (!g_ascii_strcasecmp (name, attr_name))
			return attr;
	}

	return NULL;
}



static gpointer
geo_getter (EContact *contact, EVCardAttribute *attr)
{
	if (attr) {
		GList *p = e_vcard_attribute_get_values (attr);
		EContactGeo *geo = g_new0 (EContactGeo, 1);

		geo->latitude  = (p && p->data ? g_ascii_strtod (p->data, NULL) : 0); if (p) p = p->next;
		geo->longitude = (p && p->data ? g_ascii_strtod (p->data, NULL) : 0);

		return geo;
	}
	else
		return NULL;
}

static void
geo_setter (EContact *contact, EVCardAttribute *attr, gpointer data)
{
	EContactGeo *geo = data;
	gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

	e_vcard_attribute_add_value
		(attr, g_ascii_dtostr (buf, sizeof (buf), geo->latitude));

	e_vcard_attribute_add_value
		(attr, g_ascii_dtostr (buf, sizeof (buf), geo->longitude));
}

static gpointer
photo_getter (EContact *contact, EVCardAttribute *attr)
{
	GList *values;

	if (!attr)
		return NULL;

	values = e_vcard_attribute_get_param (attr, EVC_ENCODING);
	if (values && (g_ascii_strcasecmp (values->data, "b") == 0 ||
		       /* second for photo vCard 2.1 support */
		       g_ascii_strcasecmp (values->data, "base64") == 0)) {
		values = e_vcard_attribute_get_values_decoded (attr);
		if (values && values->data) {
			GString *s = values->data;
			EContactPhoto *photo;

			if (!s->len)
				return NULL;

			photo = g_new0 (EContactPhoto, 1);
			photo->type = E_CONTACT_PHOTO_TYPE_INLINED;
			photo->data.inlined.length = s->len;
			photo->data.inlined.data = g_malloc (photo->data.inlined.length);
			memcpy (photo->data.inlined.data, s->str, photo->data.inlined.length);

			values = e_vcard_attribute_get_param (attr, EVC_TYPE);
			if (values && values->data)
				photo->data.inlined.mime_type = g_strdup_printf("image/%s", (gchar *)values->data);
			return photo;
		}
	}

	values = e_vcard_attribute_get_param (attr, EVC_VALUE);
	if (values && g_ascii_strcasecmp (values->data, "uri") == 0) {
		EContactPhoto *photo;
		photo = g_new0 (EContactPhoto, 1);
		photo->type = E_CONTACT_PHOTO_TYPE_URI;
		photo->data.uri = e_vcard_attribute_get_value (attr);
		return photo;
	}
	return NULL;
}

static void
photo_setter (EContact *contact, EVCardAttribute *attr, gpointer data)
{
	EContactPhoto *photo = data;
	const gchar *image_type, *p;

	switch (photo->type) {
	case E_CONTACT_PHOTO_TYPE_INLINED:
		g_return_if_fail (photo->data.inlined.length > 0);

		e_vcard_attribute_add_param_with_value (attr,
							e_vcard_attribute_param_new (EVC_ENCODING),
							"b");
		if (photo->data.inlined.mime_type && (p = strchr (photo->data.inlined.mime_type, '/'))) {
			image_type = p+1;
		} else {
			image_type = "X-EVOLUTION-UNKNOWN";
		}
		e_vcard_attribute_add_param_with_value (attr,
							e_vcard_attribute_param_new (EVC_TYPE),
							image_type);

		e_vcard_attribute_add_value_decoded (attr, (gchar *)photo->data.inlined.data, photo->data.inlined.length);
		break;
	case E_CONTACT_PHOTO_TYPE_URI:
		e_vcard_attribute_add_param_with_value (attr,
							e_vcard_attribute_param_new (EVC_VALUE),
							"uri");
		e_vcard_attribute_add_value (attr, photo->data.uri);
		break;
	default:
		g_warning ("Unknown EContactPhotoType %d", photo->type);
		break;
	}
}


static gpointer
fn_getter (EContact *contact, EVCardAttribute *attr)
{
	if (attr) {
		GList *p = e_vcard_attribute_get_values (attr);

		return p && p->data ? p->data : (gpointer) "";
	}
	else
		return NULL;
}

static void
fn_setter (EContact *contact, EVCardAttribute *attr, gpointer data)
{
	gchar *name_str = data;

	e_vcard_attribute_add_value (attr, name_str);

	attr = e_contact_get_first_attr (contact, EVC_N);
	if (!attr) {
		EContactName *name = e_contact_name_from_string ((gchar *)data);

		attr = e_vcard_attribute_new (NULL, EVC_N);
		e_vcard_append_attribute (E_VCARD (contact), attr);

		/* call the setter directly */
		n_setter (contact, attr, name);

		e_contact_name_free (name);
	}
}



static gpointer
n_getter (EContact *contact, EVCardAttribute *attr)
{
	EContactName *name = g_new0 (EContactName, 1);
	EVCardAttribute *new_attr;
	gchar *name_str;

	if (attr) {
		GList *p = e_vcard_attribute_get_values (attr);

		name->family     = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		name->given      = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		name->additional = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		name->prefixes   = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		name->suffixes   = g_strdup (p && p->data ? p->data : "");
	}

	new_attr = e_contact_get_first_attr (contact, EVC_FN);
	if (!new_attr) {
		new_attr = e_vcard_attribute_new (NULL, EVC_FN);
		e_vcard_append_attribute (E_VCARD (contact), new_attr);
		name_str = e_contact_name_to_string (name);
		e_vcard_attribute_add_value (new_attr, name_str);
		g_free (name_str);
	}

	return name;
}

static void
n_setter (EContact *contact, EVCardAttribute *attr, gpointer data)
{
	EContactName *name = data;

	e_vcard_attribute_add_value (attr, name->family ? name->family : "");
	e_vcard_attribute_add_value (attr, name->given ? name->given : "");
	e_vcard_attribute_add_value (attr, name->additional ? name->additional : "");
	e_vcard_attribute_add_value (attr, name->prefixes ? name->prefixes : "");
	e_vcard_attribute_add_value (attr, name->suffixes ? name->suffixes : "");

	/* now find the attribute for FileAs.  if it's not present, fill it in */
	attr = e_contact_get_first_attr (contact, EVC_X_FILE_AS);
	if (!attr) {
		gchar *strings[3], **stringptr;
		gchar *string;
		attr = e_vcard_attribute_new (NULL, EVC_X_FILE_AS);
		e_vcard_append_attribute (E_VCARD (contact), attr);

		stringptr = strings;
		if (name->family && *name->family)
			*(stringptr++) = name->family;
		if (name->given && *name->given)
			*(stringptr++) = name->given;
		*stringptr = NULL;
		string = g_strjoinv(", ", strings);

		e_vcard_attribute_add_value (attr, string);
		g_free (string);
	}

}



static gpointer
adr_getter (EContact *contact, EVCardAttribute *attr)
{
	if (attr) {
		GList *p = e_vcard_attribute_get_values (attr);
		EContactAddress *addr = g_new0 (EContactAddress, 1);

		addr->address_format = g_strdup ("");
		addr->po       = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		addr->ext      = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		addr->street   = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		addr->locality = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		addr->region   = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		addr->code     = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;
		addr->country  = g_strdup (p && p->data ? p->data : ""); if (p) p = p->next;

		return addr;
	}

	return NULL;
}

static void
adr_setter (EContact *contact, EVCardAttribute *attr, gpointer data)
{
	EContactAddress *addr = data;

	e_vcard_attribute_add_value (attr, addr->po);
	e_vcard_attribute_add_value (attr, addr->ext);
	e_vcard_attribute_add_value (attr, addr->street);
	e_vcard_attribute_add_value (attr, addr->locality);
	e_vcard_attribute_add_value (attr, addr->region);
	e_vcard_attribute_add_value (attr, addr->code);
	e_vcard_attribute_add_value (attr, addr->country);
}



static gpointer
date_getter (EContact *contact, EVCardAttribute *attr)
{
	if (attr) {
		GList *p = e_vcard_attribute_get_values (attr);
		EContactDate *date;

		if (p && p->data && ((gchar *) p->data)[0])
			date = e_contact_date_from_string ((gchar *) p->data);
		else
			date = NULL;

		return date;
	}

	return NULL;
}

static void
date_setter (EContact *contact, EVCardAttribute *attr, gpointer data)
{
	EContactDate *date = data;
	gchar *str;

	str = e_contact_date_to_string (date);

	e_vcard_attribute_add_value (attr, str);
	g_free (str);
}



static gpointer
cert_getter (EContact *contact, EVCardAttribute *attr)
{
	if (attr) {
		/* the certificate is stored in this vcard.  just
		   return the data */
		GList *values = e_vcard_attribute_get_values_decoded (attr);

		if (values && values->data) {
			GString *s = values->data;
			EContactCert *cert = g_new0 (EContactCert, 1);

			cert->length = s->len;
			cert->data = g_malloc (cert->length);
			memcpy (cert->data, s->str, cert->length);

			return cert;
		}
	}

	/* XXX if we stored a fingerprint in the cert we could look it
	   up via NSS, but that would require the additional NSS dep
	   here, and we'd have more than one process opening the
	   certdb, which is bad.  *sigh* */

	return NULL;
}

static void
cert_setter (EContact *contact, EVCardAttribute *attr, gpointer data)
{
	EContactCert *cert = data;

	e_vcard_attribute_add_param_with_value (attr,
						e_vcard_attribute_param_new (EVC_ENCODING),
						"b");

	e_vcard_attribute_add_value_decoded (attr, cert->data, cert->length);
}



/* Set_arg handler for the contact */
static void
e_contact_set_property (GObject *object,
			guint prop_id,
			const GValue *value,
			GParamSpec *pspec)
{
	EContact *contact = E_CONTACT (object);
	const EContactFieldInfo *info = NULL;

	if (prop_id < 1 || prop_id >= E_CONTACT_FIELD_LAST) {
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		return;
	}

	info = &field_info[prop_id];

	if (info->t & E_CONTACT_FIELD_TYPE_MULTI) {
		GList *new_values = g_value_get_pointer (value);
		GList *l;

		/* first we remove all attributes of the type we're
		   adding, then add new ones based on the values that
		   are passed in */
		e_vcard_remove_attributes (E_VCARD (contact), NULL, info->vcard_field_name);

		for (l = new_values; l; l = l->next)
			e_vcard_append_attribute_with_value (E_VCARD (contact),
							  e_vcard_attribute_new (NULL, info->vcard_field_name),
							  (gchar *)l->data);
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_SYNTHETIC) {
		if (info->t & E_CONTACT_FIELD_TYPE_MULTI_ELEM) {
			/* XXX this is kinda broken - we don't insert
			   insert padding elements if, e.g. the user
			   sets email 3 when email 1 and 2 don't
			   exist.  But, if we *did* pad the lists we'd
			   end up with empty items in the vcard.  I
			   dunno which is worse. */
			EVCardAttribute *attr = NULL;
			gboolean found = FALSE;
			gint num_left = info->list_elem;
			GList *attrs = e_vcard_get_attributes (E_VCARD (contact));
			GList *l;
			const gchar *sval;

			for (l = attrs; l; l = l->next) {
				const gchar *name;

				attr = l->data;
				name = e_vcard_attribute_get_name (attr);

				if (!g_ascii_strcasecmp (name, info->vcard_field_name)) {
					if (num_left-- == 0) {
						found = TRUE;
						break;
					}
				}
			}

			sval = g_value_get_string (value);
			if (sval && *sval) {
				if (found) {
					/* we found it, overwrite it */
					e_vcard_attribute_remove_values (attr);
				}
				else {
					/* we didn't find it - add a new attribute */
					attr = e_vcard_attribute_new (NULL, info->vcard_field_name);
					if (!g_ascii_strcasecmp (info->vcard_field_name, "EMAIL") &&
					    !info->attr_type1 &&
					    !info->attr_type2) {
						/* Add default type */
						e_vcard_attribute_add_param_with_value ( attr,
								e_vcard_attribute_param_new (EVC_TYPE),
								"OTHER");
					}
					e_vcard_append_attribute (E_VCARD (contact), attr);
				}

				e_vcard_attribute_add_value (attr, sval);
			}
			else {
				if (found)
					e_vcard_remove_attribute (E_VCARD (contact), attr);
			}
		}
		else if (info->t & E_CONTACT_FIELD_TYPE_ATTR_TYPE) {
			/* XXX this is kinda broken - we don't insert
			   insert padding elements if, e.g. the user
			   sets email 3 when email 1 and 2 don't
			   exist.  But, if we *did* pad the lists we'd
			   end up with empty items in the vcard.  I
			   dunno which is worse. */
			EVCardAttribute *attr = NULL;
			gboolean found = FALSE;
			gint num_left = info->list_elem;
			GList *attrs = e_vcard_get_attributes (E_VCARD (contact));
			GList *l;

			for (l = attrs; l && !found; l = l->next) {
				const gchar *name;
				gboolean found_needed1, found_needed2;

				found_needed1 = (info->attr_type1 == NULL);
				found_needed2 = (info->attr_type2 == NULL);

				attr = l->data;
				name = e_vcard_attribute_get_name (attr);

				if (!g_ascii_strcasecmp (name, info->vcard_field_name)) {
					GList *params;

					for (params = e_vcard_attribute_get_params (attr); params; params = params->next) {
						EVCardAttributeParam *param = params->data;
						const gchar *param_name = e_vcard_attribute_param_get_name (param);

						if (!g_ascii_strcasecmp (param_name, EVC_TYPE)) {
							gboolean matches = FALSE;
							GList *values = e_vcard_attribute_param_get_values (param);

							while (values && values->data) {
								if (!found_needed1 && !g_ascii_strcasecmp ((gchar *)values->data, info->attr_type1)) {
									found_needed1 = TRUE;
									matches = TRUE;
								}
								else if (!found_needed2 && !g_ascii_strcasecmp ((gchar *)values->data, info->attr_type2)) {
									found_needed2 = TRUE;
									matches = TRUE;
								} else {
									matches = FALSE;
									break;
								}

								values = values->next;
							}

							if (!matches) {
								/* this is to enforce that we find an attribute
								   with *only* the TYPE='s we need.  This may seem like
								   an odd restriction but it's the only way at present to
								   implement the Other Fax and Other Phone attributes. */
								found_needed1 = FALSE;
								break;
							}
						}

						if (found_needed1 && found_needed2) {
							if (num_left-- == 0) {
								found = TRUE;
								break;
							}
						}
					}
				}
			}

			if (found) {
				/* we found it, overwrite it */
				e_vcard_attribute_remove_values (attr);
			}
			else {
				/* we didn't find it - add a new attribute */
				attr = e_vcard_attribute_new (NULL, info->vcard_field_name);
				e_vcard_append_attribute (E_VCARD (contact), attr);
				if (info->attr_type1)
					e_vcard_attribute_add_param_with_value (attr, e_vcard_attribute_param_new (EVC_TYPE),
										info->attr_type1);
				if (info->attr_type2)
					e_vcard_attribute_add_param_with_value (attr, e_vcard_attribute_param_new (EVC_TYPE),
										info->attr_type2);
			}

			if (info->t & E_CONTACT_FIELD_TYPE_STRUCT || info->t & E_CONTACT_FIELD_TYPE_GETSET) {
				gpointer data = info->t & E_CONTACT_FIELD_TYPE_STRUCT ? g_value_get_boxed (value) : (gchar *)g_value_get_string (value);

				if ((info->t & E_CONTACT_FIELD_TYPE_STRUCT && data)
				    || (data && *(gchar *)data))
					info->struct_setter (contact, attr, data);
				else
					e_vcard_remove_attribute (E_VCARD (contact), attr);
			}
			else {
				const gchar *sval = g_value_get_string (value);

				if (sval && *sval)
					e_vcard_attribute_add_value (attr, sval);
				else
					e_vcard_remove_attribute (E_VCARD (contact), attr);
			}
		}
		else if (info->t & E_CONTACT_FIELD_TYPE_LIST_ELEM) {
			EVCardAttribute *attr = e_contact_get_first_attr (contact, info->vcard_field_name);
			GList *values;
			GList *p;
			const gchar *sval = g_value_get_string (value);

			if (!attr) {
				if (!sval || !*sval)
					return;

				d(printf ("adding new %s\n", info->vcard_field_name));

				attr = e_vcard_attribute_new (NULL, info->vcard_field_name);
				e_vcard_append_attribute (E_VCARD (contact), attr);
			}

			values = e_vcard_attribute_get_values (attr);
			p = g_list_nth (values, info->list_elem);

			if (p) {
				g_free (p->data);
				p->data = g_strdup (g_value_get_string (value));
			}
			else {
				/* there weren't enough elements in the list, pad it */
				gint count = info->list_elem - g_list_length (values);

				while (count--)
					e_vcard_attribute_add_value (attr, "");

				e_vcard_attribute_add_value (attr, g_value_get_string (value));
			}
		}
		else {
			switch (info->field_id) {
			case E_CONTACT_CATEGORIES: {
				EVCardAttribute *attr = e_contact_get_first_attr (contact, EVC_CATEGORIES);
				gchar **split, **s;
				const gchar *str;

				if (attr)
					e_vcard_attribute_remove_values (attr);
				else {
					/* we didn't find it - add a new attribute */
					attr = e_vcard_attribute_new (NULL, EVC_CATEGORIES);
					e_vcard_append_attribute (E_VCARD (contact), attr);
				}

				str = g_value_get_string (value);
				if (str && *str) {
					split = g_strsplit (str, ",", 0);
					if (split) {
						for (s = split; *s; s++) {
							e_vcard_attribute_add_value (attr, g_strstrip (*s));
						}
						g_strfreev (split);
					} else
						e_vcard_attribute_add_value (attr, str);
				}
				else {
					d(printf ("removing %s\n", info->vcard_field_name));

					e_vcard_remove_attribute (E_VCARD (contact), attr);
				}
				break;
			}
			default:
				g_warning ("unhandled synthetic field 0x%02x", info->field_id);
				break;
			}
		}
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_STRUCT || info->t & E_CONTACT_FIELD_TYPE_GETSET) {
		EVCardAttribute *attr = e_contact_get_first_attr (contact, info->vcard_field_name);
		gpointer data = info->t & E_CONTACT_FIELD_TYPE_STRUCT ? g_value_get_boxed (value) : (gchar *)g_value_get_string (value);

		if (attr) {
			if ((info->t & E_CONTACT_FIELD_TYPE_STRUCT && data)
			    || (data && *(gchar *)data)) {
				d(printf ("overwriting existing %s\n", info->vcard_field_name));
				/* remove all existing values and parameters.
				   the setter will add the correct ones */
				e_vcard_attribute_remove_values (attr);
				e_vcard_attribute_remove_params (attr);

				info->struct_setter (contact, attr, data);
			}
			else {
				d(printf ("removing %s\n", info->vcard_field_name));

				e_vcard_remove_attribute (E_VCARD (contact), attr);
			}
		}
		else if ((info->t & E_CONTACT_FIELD_TYPE_STRUCT && data)
			 || (data && *(gchar *)data)) {
			d(printf ("adding new %s\n", info->vcard_field_name));
			attr = e_vcard_attribute_new (NULL, info->vcard_field_name);

			e_vcard_append_attribute (E_VCARD (contact), attr);

			info->struct_setter (contact, attr, data);
		}
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_BOOLEAN) {
		EVCardAttribute *attr;

		/* first we search for an attribute we can overwrite */
		attr = e_contact_get_first_attr (contact, info->vcard_field_name);
		if (attr) {
			d(printf ("setting %s to `%s'\n", info->vcard_field_name, g_value_get_string (value)));
			e_vcard_attribute_remove_values (attr);
			e_vcard_attribute_add_value (attr, g_value_get_boolean (value) ? "TRUE" : "FALSE");
		}
		else {
			/* and if we don't find one we create a new attribute */
			e_vcard_append_attribute_with_value (E_VCARD (contact),
							  e_vcard_attribute_new (NULL, info->vcard_field_name),
							  g_value_get_boolean (value) ? "TRUE" : "FALSE");
		}
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_STRING) {
		EVCardAttribute *attr;
		const gchar *sval = g_value_get_string (value);

		/* first we search for an attribute we can overwrite */
		attr = e_contact_get_first_attr (contact, info->vcard_field_name);
		if (attr) {
			d(printf ("setting %s to `%s'\n", info->vcard_field_name, sval));
			e_vcard_attribute_remove_values (attr);
			if (sval) {
				e_vcard_attribute_add_value (attr, sval);
			}
			else {
				d(printf ("removing %s\n", info->vcard_field_name));

				e_vcard_remove_attribute (E_VCARD (contact), attr);
			}

		}
		else if (sval) {
			/* and if we don't find one we create a new attribute */
			e_vcard_append_attribute_with_value (E_VCARD (contact),
							  e_vcard_attribute_new (NULL, info->vcard_field_name),
							  g_value_get_string (value));
		}
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_LIST) {
		EVCardAttribute *attr;
		GList *values, *l;

		values = g_value_get_pointer (value);

		attr = e_contact_get_first_attr (contact, info->vcard_field_name);

		if (attr) {
			e_vcard_attribute_remove_values (attr);

			if (!values)
				e_vcard_remove_attribute (E_VCARD (contact), attr);
		}
		else if (values) {
			attr = e_vcard_attribute_new (NULL, info->vcard_field_name);
			e_vcard_append_attribute (E_VCARD (contact), attr);
		}

		for (l = values; l != NULL; l = l->next)
			e_vcard_attribute_add_value (attr, l->data);
	}
	else {
		g_warning ("unhandled attribute `%s'", info->vcard_field_name);
	}
}

static EVCardAttribute *
e_contact_find_attribute_with_types (EContact *contact, const gchar *attr_name, const gchar *type_needed1, const gchar *type_needed2, gint nth)
{
	GList *l, *attrs;
	gboolean found_needed1, found_needed2;

	attrs = e_vcard_get_attributes (E_VCARD (contact));

	for (l = attrs; l; l = l->next) {
		EVCardAttribute *attr = l->data;
		const gchar *name;

		found_needed1 = (type_needed1 == NULL);
		found_needed2 = (type_needed2 == NULL);

		name = e_vcard_attribute_get_name (attr);

		if (!g_ascii_strcasecmp (name, attr_name)) {
			GList *params;

			for (params = e_vcard_attribute_get_params (attr); params; params = params->next) {
				EVCardAttributeParam *param = params->data;
				const gchar *param_name = e_vcard_attribute_param_get_name (param);

				if (!g_ascii_strcasecmp (param_name, EVC_TYPE)) {
					gboolean matches = FALSE;
					GList *values = e_vcard_attribute_param_get_values (param);

					while (values && values->data) {
						if (!found_needed1 && !g_ascii_strcasecmp ((gchar *)values->data, type_needed1)) {
							found_needed1 = TRUE;
							matches = TRUE;
						}
						else if (!found_needed2 && !g_ascii_strcasecmp ((gchar *)values->data, type_needed2)) {
							found_needed2 = TRUE;
							matches = TRUE;
						} else {
							matches = FALSE;
							break;
						}
						values = values->next;
					}

					if (!matches) {
						/* this is to enforce that we find an attribute
						   with *only* the TYPE='s we need.  This may seem like
						   an odd restriction but it's the only way at present to
						   implement the Other Fax and Other Phone attributes. */
						found_needed1 = FALSE;
						break;
					}
				}

				if (found_needed1 && found_needed2) {
					if (nth-- == 0)
						return attr;
					else
						break;
				}
			}
		}
	}

	return NULL;
}

static void
e_contact_get_property (GObject *object,
			guint prop_id,
			GValue *value,
			GParamSpec *pspec)
{
	const EContactFieldInfo *info = NULL;
	gpointer data;

	if (prop_id < 1 || prop_id >= E_CONTACT_FIELD_LAST) {
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		g_value_reset (value);
		return;
	}

	info = &field_info[prop_id];
	data = e_contact_get (E_CONTACT (object), prop_id);

	if (info->t & E_CONTACT_FIELD_TYPE_BOOLEAN) {
		g_value_set_boolean (value, data != NULL);
	} else if (info->t & E_CONTACT_FIELD_TYPE_LIST) {
		g_value_set_pointer (value, data);
	} else if (info->t & E_CONTACT_FIELD_TYPE_STRUCT) {
		g_value_take_boxed (value, data);
	} else if (info->t & E_CONTACT_FIELD_TYPE_GETSET) {
		if (info->t & E_CONTACT_FIELD_TYPE_STRUCT) {
			g_value_set_boxed (value, data);
		} else {
			g_value_set_string (value, data);
		}
	} else if (info->t & E_CONTACT_FIELD_TYPE_STRING) {
		g_value_set_string (value, data);
	} else {
		g_value_set_pointer (value, data);
	}
}



/**
 * e_contact_new:
 *
 * Creates a new, blank #EContact.
 *
 * Returns: A new #EContact.
 **/
EContact*
e_contact_new (void)
{
	return e_contact_new_from_vcard ("");
}

/**
 * e_contact_new_from_vcard:
 * @vcard: a string representing a vcard
 *
 * Creates a new #EContact based on a vcard.
 *
 * Returns: A new #EContact.
 **/
EContact*
e_contact_new_from_vcard  (const gchar *vcard)
{
	EContact *contact;
	const gchar *file_as;

	g_return_val_if_fail (vcard != NULL, NULL);

	contact = g_object_new (E_TYPE_CONTACT, NULL);
	e_vcard_construct (E_VCARD (contact), vcard);

	/* Generate a FILE_AS field if needed */

	file_as = e_contact_get_const (contact, E_CONTACT_FILE_AS);
	if (!file_as || !*file_as) {
		EContactName *name;
		const gchar *org;
		gchar *file_as_new = NULL;
		gchar *strings[4];
		gchar **strings_p = strings;

		name = e_contact_get (contact, E_CONTACT_NAME);
		org = e_contact_get_const (contact, E_CONTACT_ORG);

		if (name) {
			if (name->family && *name->family)
				*(strings_p++) = name->family;
			if (name->given && *name->given)
				*(strings_p++) = name->given;

			if (strings_p != strings) {
				*strings_p = NULL;
				file_as_new = g_strjoinv (", ", strings);
			}

			e_contact_name_free (name);
		}

		if (!file_as_new && org && *org)
			file_as_new = g_strdup (org);

		if (file_as_new) {
			e_contact_set (contact, E_CONTACT_FILE_AS, file_as_new);
			g_free (file_as_new);
		}
	}

	return contact;
}

/**
 * e_contact_duplicate:
 * @contact: an #EContact
 *
 * Creates a copy of @contact.
 *
 * Returns: A new #EContact identical to @contact.
 **/
EContact*
e_contact_duplicate (EContact *contact)
{
	gchar *vcard;
	EContact *c;

	g_return_val_if_fail (E_IS_CONTACT (contact), NULL);

	vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);
	c = e_contact_new_from_vcard (vcard);
	g_free (vcard);

	return c;
}

/**
 * e_contact_field_name:
 * @field_id: an #EContactField
 *
 * Gets the string representation of @field_id.
 *
 * Returns: The string representation of @field_id, or %NULL if it doesn't exist.
 **/
const gchar *
e_contact_field_name (EContactField field_id)
{
	g_return_val_if_fail (field_id >= 1 && field_id < E_CONTACT_FIELD_LAST, "");

	return field_info[field_id].field_name;
}

/**
 * e_contact_pretty_name:
 * @field_id: an #EContactField
 *
 * Gets a human-readable, translated string representation
 * of @field_id.
 *
 * Returns: The human-readable representation of @field_id, or %NULL if it doesn't exist.
 **/
const gchar *
e_contact_pretty_name (EContactField field_id)
{
	g_return_val_if_fail (field_id >= 1 && field_id < E_CONTACT_FIELD_LAST, "");

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

	return _(field_info[field_id].pretty_name);
}

/**
 * e_contact_vcard_attribute:
 * @field_id: an #EContactField
 *
 * Gets the vcard attribute corresponding to @field_id, as a string.
 *
 * Returns: The vcard attribute corresponding to @field_id, or %NULL if it doesn't exist.
 **/
const gchar *
e_contact_vcard_attribute  (EContactField field_id)
{
	g_return_val_if_fail (field_id >= 1 && field_id < E_CONTACT_FIELD_LAST, "");

	return field_info[field_id].vcard_field_name;
}

/**
 * e_contact_field_id:
 * @field_name: a string representing a contact field
 *
 * Gets the #EContactField corresponding to the @field_name.
 *
 * Returns: An #EContactField corresponding to @field_name, or %0 if it doesn't exist.
 **/
EContactField
e_contact_field_id (const gchar *field_name)
{
	gint i;
	for (i = E_CONTACT_FIELD_FIRST; i < E_CONTACT_FIELD_LAST; i++) {
		if (!g_ascii_strcasecmp (field_info[i].field_name, field_name))
			return field_info[i].field_id;
	}

	g_warning ("unknown field name `%s'", field_name);
	return 0;
}

/**
 * e_contact_field_id_from_vcard:
 * @vcard_field: a string representing a vCard field
 *
 * Gets the #EContactField corresponding to the @vcard_field.
 *
 * Returns: An #EContactField corresponding to @vcard_field, or %0 if it doesn't exist.
 *
 * Since: 2.26
 **/
EContactField
e_contact_field_id_from_vcard (const gchar *vcard_field)
{
	gint i;

	for (i = E_CONTACT_FIELD_FIRST; i < E_CONTACT_FIELD_LAST; i++) {
		if (field_info[i].vcard_field_name == NULL)
			continue;
		if (field_info[i].t & E_CONTACT_FIELD_TYPE_SYNTHETIC)
			continue;
		if (!strcmp (field_info[i].vcard_field_name, vcard_field))
			return field_info[i].field_id;
	}

	g_warning ("unknown vCard field `%s'", vcard_field);
	return 0;
}

/**
 * e_contact_get:
 * @contact: an #EContact
 * @field_id: an #EContactField
 *
 * Gets the value of @contact's field specified by @field_id.
 *
 * Returns: (transfer full): Depends on the field's type, owned by the caller.
 **/
gpointer
e_contact_get (EContact *contact, EContactField field_id)
{
	const EContactFieldInfo *info = NULL;

	g_return_val_if_fail (contact && E_IS_CONTACT (contact), NULL);
	g_return_val_if_fail (field_id >= 1 && field_id < E_CONTACT_FIELD_LAST, NULL);

	info = &field_info[field_id];

	if (info->t & E_CONTACT_FIELD_TYPE_BOOLEAN) {
		EVCardAttribute *attr = e_contact_get_first_attr (contact, info->vcard_field_name);
		gboolean rv = FALSE;

		if (attr) {
			GList *v = e_vcard_attribute_get_values (attr);
			rv = v && v->data && !g_ascii_strcasecmp ((gchar *)v->data, "true");
			return rv ? (gpointer) "1" : NULL;
		}
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_LIST) {
		EVCardAttribute *attr = e_contact_get_first_attr (contact, info->vcard_field_name);

		if (attr) {
			GList *list = g_list_copy (e_vcard_attribute_get_values (attr));
			GList *l;
			for (l = list; l; l = l->next)
				l->data = l->data ? g_strstrip (g_strdup (l->data)) : NULL;
			return list;
		}
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_LIST_ELEM) {
		if (info->t & E_CONTACT_FIELD_TYPE_STRING) {
			EVCardAttribute *attr = e_contact_get_first_attr (contact, info->vcard_field_name);

			if (attr) {
				GList *v;

				v = e_vcard_attribute_get_values (attr);
				v = g_list_nth (v, info->list_elem);

				return (v && v->data) ? g_strstrip (g_strdup (v->data)) : NULL;
			}
		}
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_MULTI_ELEM) {
		if (info->t & E_CONTACT_FIELD_TYPE_STRING) {
			GList *attrs, *l;
			gint num_left = info->list_elem;

			attrs = e_vcard_get_attributes (E_VCARD (contact));

			for (l = attrs; l; l = l->next) {
				EVCardAttribute *attr = l->data;
				const gchar *name;

				name = e_vcard_attribute_get_name (attr);

				if (!g_ascii_strcasecmp (name, info->vcard_field_name)) {
					if (num_left-- == 0) {
						GList *v = e_vcard_attribute_get_values (attr);

						return (v && v->data) ? g_strstrip (g_strdup (v->data)) : NULL;
					}
				}
			}
		}
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_ATTR_TYPE) {
		EVCardAttribute *attr = e_contact_find_attribute_with_types (contact, info->vcard_field_name, info->attr_type1, info->attr_type2, info->list_elem);

		if (info->t & E_CONTACT_FIELD_TYPE_STRING) {
			if (attr) {
				GList *p = e_vcard_attribute_get_values (attr);
				return (p && p->data) ? g_strstrip (g_strdup (p->data)) : NULL;
			}
			else {
				return NULL;
			}
		}
		else { /* struct */
			return info->struct_getter (contact, attr);
		}

	}
	else if (info->t & E_CONTACT_FIELD_TYPE_STRUCT) {
		EVCardAttribute *attr = e_contact_get_first_attr (contact, info->vcard_field_name);
		if (attr)
			return info->struct_getter (contact, attr);
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_GETSET) {
		EVCardAttribute *attr = e_contact_get_first_attr (contact, info->vcard_field_name);
		gpointer rv = NULL;

		if (attr)
			rv = info->struct_getter (contact, attr);

		if (info->t & E_CONTACT_FIELD_TYPE_STRUCT)
			return (gpointer)info->boxed_type_getter();
		else if (!rv)
			return NULL;
		else
			return g_strstrip (g_strdup (rv));
	}
	else if (info->t & E_CONTACT_FIELD_TYPE_SYNTHETIC) {
		switch (info->field_id) {
		case E_CONTACT_NAME_OR_ORG: {
			const gchar *str;

			str = e_contact_get_const (contact, E_CONTACT_FILE_AS);
			if (!str)
				str = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
			if (!str)
				str = e_contact_get_const (contact, E_CONTACT_ORG);
			if (!str) {
				gboolean is_list = GPOINTER_TO_INT (e_contact_get (contact, E_CONTACT_IS_LIST));

				if (is_list)
					str = _("Unnamed List");
				else
					str = e_contact_get_const (contact, E_CONTACT_EMAIL_1);
			}

			return str ? g_strstrip (g_strdup (str)) : NULL;
		}
		case E_CONTACT_CATEGORIES: {
			EVCardAttribute *attr = e_contact_get_first_attr (contact, EVC_CATEGORIES);
			gchar *rv = NULL;

			if (attr) {
				GString *str = g_string_new ("");
				GList *v = e_vcard_attribute_get_values (attr);
				while (v) {
					g_string_append (str, (gchar *)v->data);
					v = v->next;
					if (v)
						g_string_append (str, ", ");
				}

				rv = g_string_free (str, FALSE);
			}
			return rv;
		}
		default:
			g_warning ("unhandled synthetic field 0x%02x", info->field_id);
			break;
		}
	}
	else {
		GList *attrs, *l;
		GList *rv = NULL; /* used for multi attribute lists */

		attrs = e_vcard_get_attributes (E_VCARD (contact));

		for (l = attrs; l; l = l->next) {
			EVCardAttribute *attr = l->data;
			const gchar *name;

			name = e_vcard_attribute_get_name (attr);

			if (!g_ascii_strcasecmp (name, info->vcard_field_name)) {
				GList *v;
				v = e_vcard_attribute_get_values (attr);

				if (info->t & E_CONTACT_FIELD_TYPE_STRING) {
					return (v && v->data) ? g_strstrip (g_strdup (v->data)) : NULL;
				}
				else {
					rv = g_list_append (rv, (v && v->data) ? g_strstrip (g_strdup (v->data)) : NULL);
				}
			}
		}
		return rv;
	}
	return NULL;
}

/**
 * e_contact_get_const:
 * @contact: an #EContact
 * @field_id: an #EContactField
 *
 * Gets the value of @contact's field specified by @field_id, caching
 * the result so it can be freed later.
 *
 * Returns: Depends on the field's type, owned by the #EContact.
 **/
gconstpointer
e_contact_get_const (EContact *contact, EContactField field_id)
{
	gpointer value = NULL;

	g_return_val_if_fail (E_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (field_id >= 1 && field_id < E_CONTACT_FIELD_LAST, NULL);
	g_return_val_if_fail (field_info[field_id].t & E_CONTACT_FIELD_TYPE_STRING, NULL);

	value = contact->priv->cached_strings[field_id];

	if (!value) {
		value = e_contact_get (contact, field_id);
		if (value)
			contact->priv->cached_strings[field_id] = value;
	}

	return value;
}

/**
 * e_contact_set;
 * @contact: an #EContact
 * @field_id: an #EContactField
 * @value: a value whose type depends on the @field_id
 *
 * Sets the value of @contact's field specified by @field_id to @value.
 **/
void
e_contact_set (EContact *contact, EContactField field_id, gconstpointer value)
{
	d(printf ("e_contact_set (%p, %d, %p)\n", contact, field_id, value));

	g_return_if_fail (contact && E_IS_CONTACT (contact));
	g_return_if_fail (field_id >= 1 && field_id < E_CONTACT_FIELD_LAST);

	/* set the cached slot to NULL so we'll re-get the new string
	   if e_contact_get_const is called again */
	contact->priv->cached_strings[field_id] = NULL;

	g_object_set (contact,
		      e_contact_field_name (field_id), value,
		      NULL);
}

/**
 * e_contact_get_attributes:
 * @contact: an #EContact
 * @field_id: an #EContactField
 *
 * Gets a list of the vcard attributes for @contact's @field_id.
 *
 * Returns: (element-type EVCardAttribute) (transfer: full): A #GList of pointers to #EVCardAttribute, owned by the caller.
 **/
GList*
e_contact_get_attributes (EContact *contact, EContactField field_id)
{
	GList *l = NULL;
	GList *attrs, *a;
	const EContactFieldInfo *info = NULL;

	g_return_val_if_fail (contact && E_IS_CONTACT (contact), NULL);
	g_return_val_if_fail (field_id >= 1 && field_id < E_CONTACT_FIELD_LAST, NULL);

	info = &field_info[field_id];

	attrs = e_vcard_get_attributes (E_VCARD (contact));

	for (a = attrs; a; a = a->next) {
		EVCardAttribute *attr = a->data;
		const gchar *name;

		name = e_vcard_attribute_get_name (attr);

		if (!g_ascii_strcasecmp (name, info->vcard_field_name)) {
			l = g_list_prepend (l, e_vcard_attribute_copy (attr));
		}
	}

	return g_list_reverse(l);
}

/**
 * e_contact_set_attributes:
 * @contact: an #EContact
 * @field_id: an #EContactField
 * @attributes: a #GList of pointers to #EVCardAttribute
 *
 * Sets the vcard attributes for @contact's @field_id.
 * Attributes are added to the contact in the same order as they are in @attributes.
 **/
void
e_contact_set_attributes (EContact *contact, EContactField field_id, GList *attributes)
{
	const EContactFieldInfo *info = NULL;
	GList *l;

	g_return_if_fail (contact && E_IS_CONTACT (contact));
	g_return_if_fail (field_id >= 1 && field_id < E_CONTACT_FIELD_LAST);

	info = &field_info[field_id];

	e_vcard_remove_attributes (E_VCARD (contact), NULL, info->vcard_field_name);

	for (l = attributes; l; l = l->next)
		e_vcard_append_attribute (E_VCARD (contact),
				       e_vcard_attribute_copy ((EVCardAttribute*)l->data));
}

/**
 * e_contact_name_new:
 *
 * Creates a new #EContactName struct.
 *
 * Returns: A new #EContactName struct.
 **/
EContactName*
e_contact_name_new (void)
{
	return g_new0 (EContactName, 1);
}

/**
 * e_contact_name_to_string:
 * @name: an #EContactName
 *
 * Generates a string representation of @name.
 *
 * Returns: The string representation of @name.
 **/
gchar *
e_contact_name_to_string(const EContactName *name)
{
	gchar *strings[6], **stringptr = strings;

	g_return_val_if_fail (name != NULL, NULL);

	if (name->prefixes && *name->prefixes)
		*(stringptr++) = name->prefixes;
	if (name->given && *name->given)
		*(stringptr++) = name->given;
	if (name->additional && *name->additional)
		*(stringptr++) = name->additional;
	if (name->family && *name->family)
		*(stringptr++) = name->family;
	if (name->suffixes && *name->suffixes)
		*(stringptr++) = name->suffixes;
	*stringptr = NULL;
	return g_strjoinv(" ", strings);
}

/**
 * e_contact_name_from_string:
 * @name_str: a string representing a contact's full name
 *
 * Creates a new #EContactName based on the parsed @name_str.
 *
 * Returns: A new #EContactName struct.
 **/
EContactName*
e_contact_name_from_string (const gchar *name_str)
{
	EContactName *name = e_contact_name_new();
	ENameWestern *western;

	g_return_val_if_fail (name_str != NULL, NULL);

	western = e_name_western_parse (name_str);

	name->prefixes   = g_strdup (western->prefix);
	name->given      = g_strdup (western->first );
	name->additional = g_strdup (western->middle);
	name->family     = g_strdup (western->last  );
	name->suffixes   = g_strdup (western->suffix);

	e_name_western_free(western);

	return name;
}

/**
 * e_contact_name_copy:
 * @n: an #EContactName
 *
 * Creates a copy of @n.
 *
 * Returns: A new #EContactName identical to @n.
 **/
EContactName*
e_contact_name_copy (EContactName *n)
{
	EContactName *name;

	g_return_val_if_fail (n != NULL, NULL);

	name = e_contact_name_new();

	name->prefixes   = g_strdup (n->prefixes);
	name->given      = g_strdup (n->given);
	name->additional = g_strdup (n->additional);
	name->family     = g_strdup (n->family);
	name->suffixes   = g_strdup (n->suffixes);

	return name;
}

/**
 * e_contact_name_free:
 * @name: an #EContactName
 *
 * Frees @name and its contents.
 **/
void
e_contact_name_free (EContactName *name)
{
	if (!name)
		return;

	g_free (name->family);
	g_free (name->given);
	g_free (name->additional);
	g_free (name->prefixes);
	g_free (name->suffixes);

	g_free (name);
}

#define E_CONTACT_DEFINE_BOXED_TYPE(_tp,_nm)				\
	GType								\
	_tp ## _get_type (void)						\
	{								\
		static volatile gsize type_id__volatile = 0;		\
									\
		if (g_once_init_enter (&type_id__volatile)) {		\
			GType type_id;					\
									\
			type_id = g_boxed_type_register_static (_nm,	\
				(GBoxedCopyFunc) _tp ## _copy,		\
				(GBoxedFreeFunc) _tp ## _free);		\
									\
			g_once_init_leave (&type_id__volatile, type_id);\
	}								\
									\
	return type_id__volatile;					\
}

E_CONTACT_DEFINE_BOXED_TYPE (e_contact_name, "EContactName")

/**
 * e_contact_date_from_string:
 * @str: a date string in the format YYYY-MM-DD or YYYYMMDD
 *
 * Creates a new #EContactDate based on @str.
 *
 * Returns: A new #EContactDate struct.
 **/
EContactDate*
e_contact_date_from_string (const gchar *str)
{
	EContactDate* date;
	gint length;
	gchar *t;

	g_return_val_if_fail (str != NULL, NULL);

	date = e_contact_date_new();
	/* ignore time part */
	if ((t = strchr (str, 'T')) != NULL)
		length = t - str;
	else
		length = strlen(str);

	if (length == 10 ) {
		date->year = str[0] * 1000 + str[1] * 100 + str[2] * 10 + str[3] - '0' * 1111;
		date->month = str[5] * 10 + str[6] - '0' * 11;
		date->day = str[8] * 10 + str[9] - '0' * 11;
	} else if (length == 8) {
		date->year = str[0] * 1000 + str[1] * 100 + str[2] * 10 + str[3] - '0' * 1111;
		date->month = str[4] * 10 + str[5] - '0' * 11;
		date->day = str[6] * 10 + str[7] - '0' * 11;
	}

	return date;
}

/**
 * e_contact_date_to_string:
 * @dt: an #EContactDate
 *
 * Generates a date string in the format YYYY-MM-DD based
 * on the values of @dt.
 *
 * Returns: A date string, owned by the caller.
 **/
gchar *
e_contact_date_to_string (EContactDate *dt)
{
	if (dt)
		return g_strdup_printf ("%04d-%02d-%02d",
					CLAMP(dt->year, 1000, 9999),
					CLAMP(dt->month, 1, 12),
					CLAMP(dt->day, 1, 31));
	else
		return NULL;
}

/**
 * e_contact_date_equal:
 * @dt1: an #EContactDate
 * @dt2: an #EContactDate
 *
 * Checks if @dt1 and @dt2 are the same date.
 *
 * Returns: %TRUE if @dt1 and @dt2 are equal, %FALSE otherwise.
 **/
gboolean
e_contact_date_equal (EContactDate *dt1, EContactDate *dt2)
{
	if (dt1 && dt2) {
		return (dt1->year == dt2->year &&
			dt1->month == dt2->month &&
			dt1->day == dt2->day);
	} else
		return (!!dt1 == !!dt2);
}

/**
 * e_contact_date_copy:
 * @dt: an #EContactDate
 *
 * Creates a copy of @dt.
 *
 * Returns: A new #EContactDate struct identical to @dt.
 **/
static EContactDate *
e_contact_date_copy (EContactDate *dt)
{
	EContactDate *dt2 = e_contact_date_new ();
	dt2->year = dt->year;
	dt2->month = dt->month;
	dt2->day = dt->day;

	return dt2;
}

/**
 * e_contact_date_free:
 * @date: an #EContactDate
 *
 * Frees the @date struct and its contents.
 */
void
e_contact_date_free (EContactDate *date)
{
	g_free (date);
}

E_CONTACT_DEFINE_BOXED_TYPE (e_contact_date, "EContactDate")

/**
 * e_contact_date_new:
 *
 * Creates a new #EContactDate struct.
 *
 * Returns: A new #EContactDate struct.
 **/
EContactDate*
e_contact_date_new (void)
{
	return g_new0 (EContactDate, 1);
}

/**
 * e_contact_photo_free:
 * @photo: an #EContactPhoto struct
 *
 * Frees the @photo struct and its contents.
 **/
void
e_contact_photo_free (EContactPhoto *photo)
{
	if (!photo)
		return;

	switch (photo->type) {
	case E_CONTACT_PHOTO_TYPE_INLINED:
		g_free (photo->data.inlined.mime_type);
		g_free (photo->data.inlined.data);
		break;
	case E_CONTACT_PHOTO_TYPE_URI:
		g_free (photo->data.uri);
		break;
	default:
		g_warning ("Unknown EContactPhotoType %d", photo->type);
		break;
	}

	g_free (photo);
}

/**
 * e_contact_photo_copy:
 * @photo: an #EContactPhoto
 *
 * Creates a copy of @photo.
 *
 * Returns: A new #EContactPhoto struct identical to @photo.
 **/
static EContactPhoto *
e_contact_photo_copy (EContactPhoto *photo)
{
	EContactPhoto *photo2 = g_new0 (EContactPhoto, 1);
	switch (photo->type) {
	case E_CONTACT_PHOTO_TYPE_INLINED:
		photo2->type = E_CONTACT_PHOTO_TYPE_INLINED;
		photo2->data.inlined.mime_type = g_strdup (photo->data.inlined.mime_type);
		photo2->data.inlined.length = photo->data.inlined.length;
		photo2->data.inlined.data = g_malloc (photo2->data.inlined.length);
		memcpy (photo2->data.inlined.data, photo->data.inlined.data, photo->data.inlined.length);
		break;
	case E_CONTACT_PHOTO_TYPE_URI:
		photo2->type = E_CONTACT_PHOTO_TYPE_URI;
		photo2->data.uri = g_strdup (photo->data.uri);
		break;
	default:
		g_warning ("Unknown EContactPhotoType %d", photo->type);
		break;
	}
	return photo2;
}

E_CONTACT_DEFINE_BOXED_TYPE (e_contact_photo, "EContactPhoto")

/**
 * e_contact_geo_free:
 * @geo: an #EContactGeo
 *
 * Frees the @geo struct and its contents.
 *
 * Since: 1.12
 **/
void
e_contact_geo_free (EContactGeo *geo)
{
	g_free (geo);
}

static EContactGeo *
e_contact_geo_copy (EContactGeo *geo)
{
	EContactGeo *geo2 = g_new0 (EContactGeo, 1);
	geo2->latitude  = geo->latitude;
	geo2->longitude = geo->longitude;

	return geo2;
}

E_CONTACT_DEFINE_BOXED_TYPE (e_contact_geo, "EContactGeo")

/**
 * e_contact_address_free:
 * @address: an #EContactAddress
 *
 * Frees the @address struct and its contents.
 **/
void
e_contact_address_free (EContactAddress *address)
{
	if (!address)
		return;

	g_free (address->address_format);
	g_free (address->po);
	g_free (address->ext);
	g_free (address->street);
	g_free (address->locality);
	g_free (address->region);
	g_free (address->code);
	g_free (address->country);

	g_free (address);
}

static EContactAddress *
e_contact_address_copy (EContactAddress *address)
{
	EContactAddress *address2 = g_new0 (EContactAddress, 1);

	address2->address_format = g_strdup (address->address_format);
	address2->po = g_strdup (address->po);
	address2->ext = g_strdup (address->ext);
	address2->street = g_strdup (address->street);
	address2->locality = g_strdup (address->locality);
	address2->region = g_strdup (address->region);
	address2->code = g_strdup (address->code);
	address2->country = g_strdup (address->country);

	return address2;
}

E_CONTACT_DEFINE_BOXED_TYPE (e_contact_address, "EContactAddress")

/**
 * e_contact_cert_free:
 * @cert: an #EContactCert
 *
 * Frees the @cert struct and its contents.
 **/
void
e_contact_cert_free (EContactCert *cert)
{
	if (!cert)
		return;

	g_free (cert->data);
	g_free (cert);
}

static EContactCert *
e_contact_cert_copy (EContactCert *cert)
{
	EContactCert *cert2 = g_new0 (EContactCert, 1);
	cert2->length = cert->length;
	cert2->data = g_malloc (cert2->length);
	memcpy (cert2->data, cert->data, cert->length);

	return cert2;
}

E_CONTACT_DEFINE_BOXED_TYPE (e_contact_cert, "EContactCert")

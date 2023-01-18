/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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
 */

#if !defined (__CAMEL_H_INSIDE__) && !defined (CAMEL_COMPILATION)
#error "Only <camel/camel.h> can be included directly."
#endif

#ifndef CAMEL_FOLDER_SUMMARY_H
#define CAMEL_FOLDER_SUMMARY_H

#include <stdio.h>
#include <time.h>

#include <camel/camel-mime-message.h>
#include <camel/camel-mime-parser.h>
#include <camel/camel-index.h>

/* Standard GObject macros */
#define CAMEL_TYPE_FOLDER_SUMMARY \
	(camel_folder_summary_get_type ())
#define CAMEL_FOLDER_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_FOLDER_SUMMARY, CamelFolderSummary))
#define CAMEL_FOLDER_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_FOLDER_SUMMARY, CamelFolderSummaryClass))
#define CAMEL_IS_FOLDER_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_FOLDER_SUMMARY))
#define CAMEL_IS_FOLDER_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_FOLDER_SUMMARY))
#define CAMEL_FOLDER_SUMMARY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_FOLDER_SUMMARY, CamelFolderSummaryClass))

G_BEGIN_DECLS

struct _CamelFolder;
struct _CamelStore;

typedef struct _CamelFolderSummary CamelFolderSummary;
typedef struct _CamelFolderSummaryClass CamelFolderSummaryClass;
typedef struct _CamelFolderSummaryPrivate CamelFolderSummaryPrivate;

typedef struct _CamelMessageInfo CamelMessageInfo;
typedef struct _CamelMessageInfoBase CamelMessageInfoBase;

typedef struct _CamelFolderMetaSummary CamelFolderMetaSummary;

typedef struct _CamelMessageContentInfo CamelMessageContentInfo;

/* A tree of message content info structures
   describe the content structure of the message (if it has any) */
struct _CamelMessageContentInfo {
	CamelMessageContentInfo *next;

	CamelMessageContentInfo *childs;
	CamelMessageContentInfo *parent;

	CamelContentType *type;
	gchar *id;
	gchar *description;
	gchar *encoding;		/* this should be an enum?? */
	guint32 size;
};

/* system flag bits */
typedef enum _CamelMessageFlags {
	CAMEL_MESSAGE_ANSWERED = 1<<0,
	CAMEL_MESSAGE_DELETED = 1<<1,
	CAMEL_MESSAGE_DRAFT = 1<<2,
	CAMEL_MESSAGE_FLAGGED = 1<<3,
	CAMEL_MESSAGE_SEEN = 1<<4,

	/* these aren't really system flag bits, but are convenience flags */
	CAMEL_MESSAGE_ATTACHMENTS = 1<<5,
	CAMEL_MESSAGE_ANSWERED_ALL = 1<<6,
	CAMEL_MESSAGE_JUNK = 1<<7,
	CAMEL_MESSAGE_SECURE = 1<<8,
	CAMEL_MESSAGE_USER_NOT_DELETABLE = 1<<9,
	CAMEL_MESSAGE_HIDDEN = 1<<10,
	CAMEL_MESSAGE_NOTJUNK = 1<<11,
	CAMEL_MESSAGE_FORWARDED = 1<<12,

	/* following flags are for the folder, and are not really permanent flags */
	CAMEL_MESSAGE_FOLDER_FLAGGED = 1<<16, /* for use by the folder implementation */
	/* flags after 1<<16 are used by camel providers,
           if adding non permanent flags, add them to the end  */

	CAMEL_MESSAGE_JUNK_LEARN = 1<<30, /* used when setting CAMEL_MESSAGE_JUNK flag
					     to say that we request junk plugin
					     to learn that message as junk/non junk */
	CAMEL_MESSAGE_USER = 1<<31 /* supports user flags */
} CamelMessageFlags;

/* Changes to system flags will NOT trigger a folder changed event */
#define CAMEL_MESSAGE_SYSTEM_MASK (0xffff << 16)

typedef struct _CamelFlag {
	struct _CamelFlag *next;
	gchar name[1];		/* name allocated as part of the structure */
} CamelFlag;

typedef struct _CamelTag {
	struct _CamelTag *next;
	gchar *value;
	gchar name[1];		/* name allocated as part of the structure */
} CamelTag;

/* a summary messageid is a 64 bit identifier (partial md5 hash) */
typedef struct _CamelSummaryMessageID {
	union {
		guint64 id;
		guchar hash[8];
		struct {
			guint32 hi;
			guint32 lo;
		} part;
	} id;
} CamelSummaryMessageID;

/* summary references is a fixed size array of references */
typedef struct _CamelSummaryReferences {
	gint size;
	CamelSummaryMessageID references[1];
} CamelSummaryReferences;

/* accessor id's */
enum {
	CAMEL_MESSAGE_INFO_SUBJECT,
	CAMEL_MESSAGE_INFO_FROM,
	CAMEL_MESSAGE_INFO_TO,
	CAMEL_MESSAGE_INFO_CC,
	CAMEL_MESSAGE_INFO_MLIST,

	CAMEL_MESSAGE_INFO_FLAGS,
	CAMEL_MESSAGE_INFO_SIZE,

	CAMEL_MESSAGE_INFO_DATE_SENT,
	CAMEL_MESSAGE_INFO_DATE_RECEIVED,

	CAMEL_MESSAGE_INFO_MESSAGE_ID,
	CAMEL_MESSAGE_INFO_REFERENCES,
	CAMEL_MESSAGE_INFO_USER_FLAGS,
	CAMEL_MESSAGE_INFO_USER_TAGS,

	CAMEL_MESSAGE_INFO_HEADERS,
	CAMEL_MESSAGE_INFO_PREVIEW,
	CAMEL_MESSAGE_INFO_CONTENT,
	CAMEL_MESSAGE_INFO_LAST
};

/* information about a given message, use accessors */
struct _CamelMessageInfo {
	CamelFolderSummary *summary;

	guint32 refcount;	/* ??? */
	const gchar *uid;
	/*FIXME: Make it work with the CAMEL_MESSADE_DB_DIRTY flag instead of another 4 bytes*/
	guint dirty:1;
};

/* For classes wishing to do the provided i/o, or for anonymous users,
 * they must subclass or use this messageinfo structure */
/* Otherwise they can do their own thing entirely */
struct _CamelMessageInfoBase {
	CamelFolderSummary *summary;

	guint32 refcount;	/* ??? */
	const gchar *uid;
	/*FIXME: Make it work with the CAMEL_MESSADE_DB_DIRTY flag instead of another 4 bytes*/
	guint dirty:1;

	const gchar *subject;
	const gchar *from;
	const gchar *to;
	const gchar *cc;
	const gchar *mlist;

	guint32 flags;
	guint32 size;

	time_t date_sent;
	time_t date_received;

	CamelSummaryMessageID message_id;
	CamelSummaryReferences *references;/* from parent to root */

	struct _CamelFlag *user_flags;
	struct _CamelTag *user_tags;

	/* tree of content description - NULL if it is not available */
	CamelMessageContentInfo *content;
	struct _camel_header_param *headers;
	gchar *preview;
	gchar *bodystructure;
};

typedef enum _CamelFolderSummaryFlags {
	CAMEL_SUMMARY_DIRTY = 1<<0
} CamelFolderSummaryFlags;

/**
 * CamelFolderSummaryLock:
 *
 * Since: 2.32
 **/
typedef enum _CamelFolderSummaryLock {
	CAMEL_FOLDER_SUMMARY_SUMMARY_LOCK,
	CAMEL_FOLDER_SUMMARY_IO_LOCK,
	CAMEL_FOLDER_SUMMARY_FILTER_LOCK,
	CAMEL_FOLDER_SUMMARY_ALLOC_LOCK,
	CAMEL_FOLDER_SUMMARY_REF_LOCK
} CamelFolderSummaryLock;

struct _CamelFolderSummary {
	CamelObject parent;
	CamelFolderSummaryPrivate *priv;

	/* header info */
	guint32 version;	/* version of file loaded/loading */
	guint32 flags;		/* flags */
	guint32 nextuid;	/* next uid? */
	time_t time;		/* timestamp for this summary (for implementors to use) */
	guint32 saved_count;	/* how many were saved/loaded */
	guint32 unread_count;	/* handy totals */
	guint32 deleted_count;
	guint32 junk_count;
	guint32 junk_not_deleted_count;
	guint32 visible_count;

	/* memory allocators (setup automatically) */
	struct _EMemChunk *message_info_chunks;
	struct _EMemChunk *content_info_chunks;

	gchar *summary_path;
	gboolean build_content;	/* do we try and parse/index the content, or not? */

	/* New members to replace the above depreacted members */
	GPtrArray *uids;
	GHashTable *loaded_infos;

	struct _CamelFolder *folder; /* parent folder, for events */
	struct _CamelFolderMetaSummary *meta_summary; /* Meta summary */
	time_t cache_load_time;
	guint timeout_handle;

	const gchar *collate;
	const gchar *sort_by;

	/* Future ABI expansion */
	gpointer later[4];
};

struct _CamelMIRecord;
struct _CamelFIRecord;

struct _CamelFolderSummaryClass {
	CamelObjectClass parent_class;

	/* sizes of memory objects */
	gsize message_info_size;
	gsize content_info_size;

	/* load/save the global info */
	gint (*summary_header_load)(CamelFolderSummary *, FILE *);
	gint (*summary_header_save)(CamelFolderSummary *, FILE *);

	/* Load/Save folder summary from DB*/
	gint (*summary_header_from_db)(CamelFolderSummary *, struct _CamelFIRecord *);
	struct _CamelFIRecord * (*summary_header_to_db)(CamelFolderSummary *, GError **error);
	CamelMessageInfo * (*message_info_from_db) (CamelFolderSummary *, struct _CamelMIRecord*);
	struct _CamelMIRecord * (*message_info_to_db) (CamelFolderSummary *, CamelMessageInfo *);
	CamelMessageContentInfo * (*content_info_from_db) (CamelFolderSummary *, struct _CamelMIRecord *);
	gint (*content_info_to_db) (CamelFolderSummary *, CamelMessageContentInfo *, struct _CamelMIRecord *);

	/* create/save/load an individual message info */
	CamelMessageInfo * (*message_info_new_from_header)(CamelFolderSummary *, struct _camel_header_raw *);
	CamelMessageInfo * (*message_info_new_from_parser)(CamelFolderSummary *, CamelMimeParser *);
	CamelMessageInfo * (*message_info_new_from_message)(CamelFolderSummary *, CamelMimeMessage *, const gchar *);
	CamelMessageInfo * (*message_info_migrate)(CamelFolderSummary *, FILE *);
	void		   (*message_info_free)(CamelFolderSummary *, CamelMessageInfo *);
	CamelMessageInfo * (*message_info_clone)(CamelFolderSummary *, const CamelMessageInfo *);

	/* save/load individual content info's */
	CamelMessageContentInfo * (*content_info_new_from_header)(CamelFolderSummary *, struct _camel_header_raw *);
	CamelMessageContentInfo * (*content_info_new_from_parser)(CamelFolderSummary *, CamelMimeParser *);
	CamelMessageContentInfo * (*content_info_new_from_message)(CamelFolderSummary *, CamelMimePart *);
	CamelMessageContentInfo * (*content_info_migrate)(CamelFolderSummary *, FILE *);
	void			  (*content_info_free)(CamelFolderSummary *, CamelMessageContentInfo *);
	CamelMessageInfo * (*message_info_from_uid) (CamelFolderSummary *, const gchar *);
	/* get the next uid */
	gchar *(*next_uid_string)(CamelFolderSummary *);

	/* virtual accessors on messageinfo's */
	gconstpointer (*info_ptr)(const CamelMessageInfo *mi, gint id);
	guint32     (*info_uint32)(const CamelMessageInfo *mi, gint id);
	time_t      (*info_time)(const CamelMessageInfo *mi, gint id);

	gboolean    (*info_user_flag)(const CamelMessageInfo *mi, const gchar *id);
	const gchar *(*info_user_tag)(const CamelMessageInfo *mi, const gchar *id);

	/* set accessors for the modifyable bits */
	gboolean (*info_set_user_flag)(CamelMessageInfo *mi, const gchar *id, gboolean state);
	gboolean (*info_set_user_tag)(CamelMessageInfo *mi, const gchar *id, const gchar *val);
	gboolean (*info_set_flags)(CamelMessageInfo *mi, guint32 mask, guint32 set);
};

/* Meta-summary info */
struct _CamelFolderMetaSummary {
	guint32 major;		/* Major version of meta-summary */
	guint32 minor;		/* Minor version of meta-summary */
	guint32 uid_len;	/* Length of UID (for implementors to use) */
	gboolean msg_expunged;	/* Whether any message is expunged or not */
	gchar *path;		/* Path to meta-summary-file */
};

GType			 camel_folder_summary_get_type	(void);
CamelFolderSummary      *camel_folder_summary_new	(struct _CamelFolder *folder);

/* Deprecated */
void camel_folder_summary_set_filename(CamelFolderSummary *summary, const gchar *filename);

void camel_folder_summary_set_index(CamelFolderSummary *summary, CamelIndex *index);
void camel_folder_summary_set_build_content(CamelFolderSummary *summary, gboolean state);

guint32  camel_folder_summary_next_uid        (CamelFolderSummary *summary);
gchar    *camel_folder_summary_next_uid_string (CamelFolderSummary *summary);
void	 camel_folder_summary_set_uid	      (CamelFolderSummary *summary, guint32 uid);

/* load/save the full summary from/to the db */
gint camel_folder_summary_save_to_db (CamelFolderSummary *s, GError **error);
gint camel_folder_summary_load_from_db (CamelFolderSummary *s, GError **error);

/* only load the header */
gint camel_folder_summary_header_load(CamelFolderSummary *summary);
gint camel_folder_summary_header_load_from_db (CamelFolderSummary *s, struct _CamelStore *store, const gchar *folder_name, GError **error);
gint camel_folder_summary_header_save_to_db (CamelFolderSummary *s, GError **error);

/* set the dirty bit on the summary */
void camel_folder_summary_touch(CamelFolderSummary *summary);

/* add a new raw summary item */
void camel_folder_summary_add (CamelFolderSummary *summary, CamelMessageInfo *info);

/* Peek from mem only */
CamelMessageInfo * camel_folder_summary_peek_info (CamelFolderSummary *s, const gchar *uid);

/* Get only the uids of dirty/changed things to sync to server/db */
GPtrArray * camel_folder_summary_get_changed (CamelFolderSummary *s);
/* reload the summary at any required point if required */
void camel_folder_summary_prepare_fetch_all (CamelFolderSummary *s, GError **error);
/* insert mi to summary */
void camel_folder_summary_insert (CamelFolderSummary *s, CamelMessageInfo *info, gboolean load);

void camel_folder_summary_remove_index_fast (CamelFolderSummary *s, gint index);
void camel_folder_summary_remove_uid_fast (CamelFolderSummary *s, const gchar *uid);

/* build/add raw summary items */
CamelMessageInfo *camel_folder_summary_add_from_header(CamelFolderSummary *summary, struct _camel_header_raw *headers);
CamelMessageInfo *camel_folder_summary_add_from_parser(CamelFolderSummary *summary, CamelMimeParser *parser);
CamelMessageInfo *camel_folder_summary_add_from_message(CamelFolderSummary *summary, CamelMimeMessage *message);

/* Just build raw summary items */
CamelMessageInfo *camel_folder_summary_info_new_from_header(CamelFolderSummary *summary, struct _camel_header_raw *headers);
CamelMessageInfo *camel_folder_summary_info_new_from_parser(CamelFolderSummary *summary, CamelMimeParser *parser);
CamelMessageInfo *camel_folder_summary_info_new_from_message(CamelFolderSummary *summary, CamelMimeMessage *message, const gchar *bodystructure);

CamelMessageContentInfo *camel_folder_summary_content_info_new(CamelFolderSummary *summary);
void camel_folder_summary_content_info_free(CamelFolderSummary *summary, CamelMessageContentInfo *ci);

/* removes a summary item, doesn't fix content offsets */
void camel_folder_summary_remove(CamelFolderSummary *summary, CamelMessageInfo *info);
void camel_folder_summary_remove_uid(CamelFolderSummary *summary, const gchar *uid);
void camel_folder_summary_remove_index(CamelFolderSummary *summary, gint index);
void camel_folder_summary_remove_range(CamelFolderSummary *summary, gint start, gint end);

/* remove all items */
void camel_folder_summary_clear(CamelFolderSummary *summary);
void camel_folder_summary_clear_db (CamelFolderSummary *s);

/* update visible/unread/... counts based on message flags */
void camel_folder_summary_update_counts_by_flags (CamelFolderSummary *summary, guint32 flags, gboolean subtract);

/* lookup functions */
guint camel_folder_summary_count(CamelFolderSummary *summary);
CamelMessageInfo *camel_folder_summary_index(CamelFolderSummary *summary, gint index);
CamelMessageInfo *camel_folder_summary_uid(CamelFolderSummary *summary, const gchar *uid);
gchar * camel_folder_summary_uid_from_index (CamelFolderSummary *s, gint i);
gboolean camel_folder_summary_check_uid (CamelFolderSummary *s, const gchar *uid);

GPtrArray *camel_folder_summary_array(CamelFolderSummary *summary);
GHashTable *camel_folder_summary_get_hashtable(CamelFolderSummary *s);
void camel_folder_summary_free_hashtable (GHashTable *ht);
GHashTable *camel_folder_summary_get_flag_cache (CamelFolderSummary *summary);
void camel_folder_summary_update_flag_cache (CamelFolderSummary *s, const gchar *uid, guint32 flag);

/* basically like strings, but certain keywords can be compressed and de-cased */
gint camel_folder_summary_encode_token(FILE *out, const gchar *str);
gint camel_folder_summary_decode_token(FILE *in, gchar **str);

/* message flag operations */
gboolean	camel_flag_get(CamelFlag **list, const gchar *name);
gboolean	camel_flag_set(CamelFlag **list, const gchar *name, gboolean value);
gboolean	camel_flag_list_copy(CamelFlag **to, CamelFlag **from);
gint		camel_flag_list_size(CamelFlag **list);
void		camel_flag_list_free(CamelFlag **list);

guint32         camel_system_flag (const gchar *name);
gboolean        camel_system_flag_get (guint32 flags, const gchar *name);

/* message tag operations */
const gchar	*camel_tag_get(CamelTag **list, const gchar *name);
gboolean	camel_tag_set(CamelTag **list, const gchar *name, const gchar *value);
gboolean	camel_tag_list_copy(CamelTag **to, CamelTag **from);
gint		camel_tag_list_size(CamelTag **list);
void		camel_tag_list_free(CamelTag **list);

/* Summary may be null */
/* Use anonymous pointers to avoid tons of cast crap */
gpointer camel_message_info_new(CamelFolderSummary *summary);
void camel_message_info_ref(gpointer info);
CamelMessageInfo *camel_message_info_new_from_header(CamelFolderSummary *summary, struct _camel_header_raw *header);
void camel_message_info_free(gpointer info);
gpointer camel_message_info_clone(gconstpointer info);

/* accessors */
gconstpointer camel_message_info_ptr(const CamelMessageInfo *mi, gint id);
guint32 camel_message_info_uint32(const CamelMessageInfo *mi, gint id);
time_t camel_message_info_time(const CamelMessageInfo *mi, gint id);

#define camel_message_info_uid(mi) ((const gchar *)((const CamelMessageInfo *)mi)->uid)

#define camel_message_info_subject(mi) ((const gchar *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_SUBJECT))

/**
 * camel_message_info_preview:
 * @mi: a #CamelMessageInfo
 *
 * FIXME Document me!
 *
 * Since: 2.28
 **/
#define camel_message_info_preview(mi) ((const gchar *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_PREVIEW))

#define camel_message_info_from(mi) ((const gchar *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_FROM))
#define camel_message_info_to(mi) ((const gchar *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_TO))
#define camel_message_info_cc(mi) ((const gchar *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_CC))
#define camel_message_info_mlist(mi) ((const gchar *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_MLIST))

#define camel_message_info_flags(mi) camel_message_info_uint32((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_FLAGS)
#define camel_message_info_size(mi) camel_message_info_uint32((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_SIZE)

#define camel_message_info_date_sent(mi) camel_message_info_time((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_DATE_SENT)
#define camel_message_info_date_received(mi) camel_message_info_time((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_DATE_RECEIVED)

#define camel_message_info_message_id(mi) ((const CamelSummaryMessageID *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_MESSAGE_ID))
#define camel_message_info_references(mi) ((const CamelSummaryReferences *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_REFERENCES))
#define camel_message_info_user_flags(mi) ((const CamelFlag *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_USER_FLAGS))
#define camel_message_info_user_tags(mi) ((const CamelTag *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_USER_TAGS))

/**
 * camel_message_info_headers:
 * @mi: a #CamelMessageInfo
 *
 * FIXME Document me!
 *
 * Since: 2.24
 **/
#define camel_message_info_headers(mi) ((const struct _camel_header_param *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_HEADERS))

/**
 * camel_message_info_content:
 * @mi: a #CamelMessageInfo
 *
 * FIXME Document me!
 *
 * Since: 2.30
 **/
#define camel_message_info_content(mi) ((const CamelMessageContentInfo *)camel_message_info_ptr((const CamelMessageInfo *)mi, CAMEL_MESSAGE_INFO_CONTENT))

gboolean camel_message_info_user_flag(const CamelMessageInfo *mi, const gchar *id);
const gchar *camel_message_info_user_tag(const CamelMessageInfo *mi, const gchar *id);

gboolean camel_message_info_set_flags(CamelMessageInfo *mi, guint32 flags, guint32 set);
gboolean camel_message_info_set_user_flag(CamelMessageInfo *mi, const gchar *id, gboolean state);
gboolean camel_message_info_set_user_tag(CamelMessageInfo *mi, const gchar *id, const gchar *val);

void camel_folder_summary_set_need_preview (CamelFolderSummary *summary, gboolean preview);
void camel_folder_summary_add_preview (CamelFolderSummary *s, CamelMessageInfo *info);
gboolean camel_folder_summary_get_need_preview (CamelFolderSummary *summary);

const CamelMessageContentInfo * camel_folder_summary_guess_content_info (CamelMessageInfo *mi, CamelContentType *ctype);

/* debugging functions */
void camel_content_info_dump (CamelMessageContentInfo *ci, gint depth);

void camel_message_info_dump (CamelMessageInfo *mi);

/* Migration code */
gint camel_folder_summary_migrate_infos(CamelFolderSummary *s);

void camel_folder_summary_lock   (CamelFolderSummary *summary, CamelFolderSummaryLock lock);
void camel_folder_summary_unlock (CamelFolderSummary *summary, CamelFolderSummaryLock lock);

G_END_DECLS

#endif /* CAMEL_FOLDER_SUMMARY_H */

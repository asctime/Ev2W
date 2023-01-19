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

#ifndef CAMEL_MBOX_SUMMARY_H
#define CAMEL_MBOX_SUMMARY_H

#include "camel-local-summary.h"

/* Enable the use of elm/pine style "Status" & "X-Status" headers */
#define STATUS_PINE

/* Standard GObject macros */
#define CAMEL_TYPE_MBOX_SUMMARY \
	(camel_mbox_summary_get_type ())
#define CAMEL_MBOX_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_MBOX_SUMMARY, CamelMboxSummary))
#define CAMEL_MBOX_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_MBOX_SUMMARY, CamelMboxSummaryClass))
#define CAMEL_IS_MBOX_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_MBOX_SUMMARY))
#define CAMEL_IS_MBOX_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_MBOX_SUMMARY))
#define CAMEL_MBOX_SUMMARY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_MBOX_SUMMARY, CamelMboxSummaryClass))

G_BEGIN_DECLS

typedef struct _CamelMboxSummary CamelMboxSummary;
typedef struct _CamelMboxSummaryClass CamelMboxSummaryClass;

typedef struct _CamelMboxMessageContentInfo {
	CamelMessageContentInfo info;
} CamelMboxMessageContentInfo;

typedef struct _CamelMboxMessageInfo {
	CamelLocalMessageInfo info;

	goffset frompos;
} CamelMboxMessageInfo;

struct _CamelMboxSummary {
	CamelLocalSummary parent;

	CamelFolderChangeInfo *changes;	/* used to build change sets */

	guint32 version;
	gsize folder_size;	/* size of the mbox file, last sync */

	guint xstatus:1;	/* do we store/honour xstatus/status headers */
};

struct _CamelMboxSummaryClass {
	CamelLocalSummaryClass parent_class;

	/* sync in-place */
	gint (*sync_quick)(CamelMboxSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, GError **error);
	/* sync requires copy */
	gint (*sync_full)(CamelMboxSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, GError **error);
};

GType		camel_mbox_summary_get_type	(void);
CamelMboxSummary      *camel_mbox_summary_new	(struct _CamelFolder *, const gchar *filename, const gchar *mbox_name, CamelIndex *index);

/* do we honour/use xstatus headers, etc */
void camel_mbox_summary_xstatus(CamelMboxSummary *mbs, gint state);

/* build a new mbox from an existing mbox storing summary information */
gint camel_mbox_summary_sync_mbox(CamelMboxSummary *cls, guint32 flags, CamelFolderChangeInfo *changeinfo, gint fd, gint fdout, GError **error);

G_END_DECLS

#endif /* CAMEL_MBOX_SUMMARY_H */

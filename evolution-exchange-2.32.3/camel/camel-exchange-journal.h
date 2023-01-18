/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __CAMEL_EXCHANGE_JOURNAL_H__
#define __CAMEL_EXCHANGE_JOURNAL_H__

#include <stdarg.h>

#include <glib.h>

#include "camel-exchange-folder.h"

/* Standard GObject class */
#define CAMEL_TYPE_EXCHANGE_JOURNAL \
	(camel_exchange_journal_get_type ())
#define CAMEL_EXCHANGE_JOURNAL(obj) \
	(CAMEL_CHECK_CAST \
	((obj), CAMEL_TYPE_EXCHANGE_JOURNAL, CamelExchangeJournal))
#define CAMEL_EXCHANGE_JOURNAL_CLASS(cls) \
	(CAMEL_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_EXCHANGE_JOURNAL, CamelExchangeJournalClass))
#define CAMEL_IS_EXCHANGE_JOURNAL(obj) \
	(CAMEL_CHECK_TYPE \
	((obj), CAMEL_TYPE_EXCHANGE_JOURNAL))
#define CAMEL_IS_EXCHANGE_JOURNAL_CLASS(cls) \
	(CAMEL_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_EXCHANGE_JOURNAL))
#define CAMEL_EXCHANGE_JOURNAL_GET_CLASS(obj) \
	(CAMEL_CHECK_GET_CLASS \
	((obj), CAMEL_TYPE_EXCHANGE_JOURNAL, CamelExchangeJournalClass))

G_BEGIN_DECLS

typedef struct _CamelExchangeJournal CamelExchangeJournal;
typedef struct _CamelExchangeJournalClass CamelExchangeJournalClass;
typedef struct _CamelExchangeJournalEntry CamelExchangeJournalEntry;

//struct CamelExchangeFolder;

enum {
	CAMEL_EXCHANGE_JOURNAL_ENTRY_APPEND,
	CAMEL_EXCHANGE_JOURNAL_ENTRY_TRANSFER,
	CAMEL_EXCHANGE_JOURNAL_ENTRY_DELETE
};

struct _CamelExchangeJournalEntry {
	CamelDListNode node;

	gint type;

	gchar *uid;
	gchar *original_uid;
	gchar *folder_name;
	gboolean delete_original;
	guint32 flags;
	guint32 set;
};

struct _CamelExchangeJournal {
	CamelOfflineJournal parent;
};

struct _CamelExchangeJournalClass {
	CamelOfflineJournalClass parent_class;
};

GType camel_exchange_journal_get_type (void);

CamelOfflineJournal *
		camel_exchange_journal_new	(CamelExchangeFolder *folder,
						 const gchar *filename);

/* interfaces for adding a journal entry */
gboolean	camel_exchange_journal_append	(CamelExchangeJournal *journal,
						 CamelMimeMessage *message,
						 const CamelMessageInfo *mi,
						 gchar **appended_uid,
						 GError **error);

gboolean	camel_exchange_journal_transfer	(CamelExchangeJournal *journal,
						 CamelExchangeFolder *source_folder,
						 CamelMimeMessage *message,
						 const CamelMessageInfo *mi,
						 const gchar *original_uid,
						 gchar **transferred_uid,
						 gboolean delete_original,
						 GError **error);
gboolean	camel_exchange_journal_delete	(CamelExchangeJournal *journal,
						 const gchar *uid,
						 guint32 flags,
						 guint32 set,
						 GError **error);

G_END_DECLS

#endif /* __CAMEL_EXCHANGE_JOURNAL_H__ */

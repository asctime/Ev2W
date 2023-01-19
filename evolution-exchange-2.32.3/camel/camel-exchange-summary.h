/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef _CAMEL_EXCHANGE_SUMMARY_H
#define _CAMEL_EXCHANGE_SUMMARY_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_EXCHANGE_SUMMARY \
	(camel_exchange_summary_get_type ())
#define CAMEL_EXCHANGE_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_EXCHANGE_SUMMARY, CamelExchangeSummary))
#define CAMEL_EXCHANGE_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_EXCHANGE_SUMMARY, CamelExchangeSummaryClass))
#define CAMEL_IS_EXCHANGE_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_EXCHANGE_SUMMARY))
#define CAMEL_IS_EXCHANGE_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_EXCHANGE_SUMMARY))
#define CAMEL_EXCHANGE_SUMMARY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_EXCHANGE_SUMMARY, CamelExchangeSummaryClass))

G_BEGIN_DECLS

typedef struct _CamelExchangeSummary CamelExchangeSummary;
typedef struct _CamelExchangeSummaryClass CamelExchangeSummaryClass;
typedef struct _CamelExchangeMessageInfo CamelExchangeMessageInfo;

struct _CamelExchangeMessageInfo {
	CamelMessageInfoBase info;

	gchar *thread_index;
	gchar *href;
};

struct _CamelExchangeSummary {
	CamelFolderSummary parent;

	gboolean readonly;
	guint32 high_article_num;
	guint32 version;
};

struct _CamelExchangeSummaryClass {
	CamelFolderSummaryClass parent_class;
};

GType		camel_exchange_summary_get_type	(void);
CamelFolderSummary *
		camel_exchange_summary_new	(CamelFolder *folder,
						 const gchar *filename);
gboolean	camel_exchange_summary_get_readonly
						(CamelFolderSummary *summary);
void		camel_exchange_summary_set_readonly
						(CamelFolderSummary *summary,
						 gboolean readonly);
void		camel_exchange_summary_add_offline
						(CamelFolderSummary *summary,
						 const gchar *uid,
						 CamelMimeMessage *message,
						 CamelMessageInfo *info);
void		camel_exchange_summary_add_offline_uncached
						(CamelFolderSummary *summary,
						 const gchar         *uid,
						 CamelMessageInfo   *info);
guint32		camel_exchange_summary_get_article_num
						(CamelFolderSummary *summary);
void		camel_exchange_summary_set_article_num
						(CamelFolderSummary *summary,
						 guint32 high_article_num);

G_END_DECLS

#endif /* _CAMEL_EXCHANGE_SUMMARY_H */


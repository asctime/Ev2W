/*
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#ifndef CAMEL_FILTER_DRIVER_H
#define CAMEL_FILTER_DRIVER_H

#include <camel/camel-folder.h>
#include <camel/camel-uid-cache.h>

/* Standard GObject macros */
#define CAMEL_TYPE_FILTER_DRIVER \
	(camel_filter_driver_get_type ())
#define CAMEL_FILTER_DRIVER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_FILTER_DRIVER, CamelFilterDriver))
#define CAMEL_FILTER_DRIVER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_FILTER_DRIVER, CamelFilterDriverClass)
#define CAMEL_IS_FILTER_DRIVER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_FILTER_DRIVER))
#define CAMEL_IS_FILTER_DRIVER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_FILTER_DRIVER))
#define CAMEL_FILTER_DRIVER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_FILTER_DRIVER, CamelFilterDriverClass))

G_BEGIN_DECLS

struct _CamelSession;

typedef struct _CamelFilterDriver CamelFilterDriver;
typedef struct _CamelFilterDriverClass CamelFilterDriverClass;
typedef struct _CamelFilterDriverPrivate CamelFilterDriverPrivate;

struct _CamelFilterDriver {
	CamelObject parent;
	CamelFilterDriverPrivate *priv;
};

struct _CamelFilterDriverClass {
	CamelObjectClass parent_class;
};

/* FIXME: this maybe should change... */
/* type of status for a status report */
enum camel_filter_status_t {
	CAMEL_FILTER_STATUS_NONE,
	CAMEL_FILTER_STATUS_START,	/* start of new message processed */
	CAMEL_FILTER_STATUS_ACTION,	/* an action performed */
	CAMEL_FILTER_STATUS_PROGRESS,	/* (an) extra update(s), if its taking longer to process */
	CAMEL_FILTER_STATUS_END		/* end of message */
};

typedef CamelFolder * (*CamelFilterGetFolderFunc) (CamelFilterDriver *driver, const gchar *uri,
						   gpointer data, GError **error);
/* report status */
typedef void (CamelFilterStatusFunc) (CamelFilterDriver *driver, enum camel_filter_status_t status,
				      gint pc, const gchar *desc, gpointer data);

typedef void (CamelFilterShellFunc)      (CamelFilterDriver *driver, gint argc, gchar **argv, gpointer data);
typedef void (CamelFilterPlaySoundFunc)  (CamelFilterDriver *driver, const gchar *filename, gpointer data);
typedef void (CamelFilterSystemBeepFunc) (CamelFilterDriver *driver, gpointer data);

GType          camel_filter_driver_get_type (void);
CamelFilterDriver  *camel_filter_driver_new     (struct _CamelSession *);

/* modifiers */
void camel_filter_driver_set_logfile          (CamelFilterDriver *d, FILE *logfile);

void camel_filter_driver_set_status_func      (CamelFilterDriver *d, CamelFilterStatusFunc *func, gpointer data);
void camel_filter_driver_set_shell_func       (CamelFilterDriver *d, CamelFilterShellFunc *func, gpointer data);
void camel_filter_driver_set_play_sound_func  (CamelFilterDriver *d, CamelFilterPlaySoundFunc *func, gpointer data);
void camel_filter_driver_set_system_beep_func (CamelFilterDriver *d, CamelFilterSystemBeepFunc *func, gpointer data);
void camel_filter_driver_set_folder_func      (CamelFilterDriver *d, CamelFilterGetFolderFunc fetcher, gpointer data);

void camel_filter_driver_set_default_folder   (CamelFilterDriver *d, CamelFolder *def);

void camel_filter_driver_add_rule             (CamelFilterDriver *d, const gchar *name, const gchar *match,
					       const gchar *action);
gint  camel_filter_driver_remove_rule_by_name  (CamelFilterDriver *d, const gchar *name);

/*void camel_filter_driver_set_global(CamelFilterDriver *, const gchar *name, const gchar *value);*/

void camel_filter_driver_flush                (CamelFilterDriver *driver, GError **error);

gint  camel_filter_driver_filter_message       (CamelFilterDriver *driver, CamelMimeMessage *message,
					       CamelMessageInfo *info, const gchar *uid,
					       CamelFolder *source, const gchar *source_url,
					       const gchar *original_source_url, GError **error);

gint  camel_filter_driver_filter_mbox          (CamelFilterDriver *driver, const gchar *mbox,
					       const gchar *original_source_url, GError **error);

gint  camel_filter_driver_filter_folder        (CamelFilterDriver *driver, CamelFolder *folder, CamelUIDCache *cache,
					       GPtrArray *uids, gboolean remove, GError **error);

G_END_DECLS

#endif /* CAMEL_FILTER_DRIVER_H */

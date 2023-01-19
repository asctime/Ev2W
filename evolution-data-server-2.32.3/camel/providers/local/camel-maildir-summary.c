/*
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/uio.h>
#else
#include <winsock2.h>
#endif

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "camel-maildir-summary.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_MAILDIR_SUMMARY_VERSION (0x2000)

#define CAMEL_MAILDIR_SUMMARY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_MAILDIR_SUMMARY, CamelMaildirSummaryPrivate))

static CamelMessageInfo *message_info_migrate (CamelFolderSummary *s, FILE *in);
static CamelMessageInfo *message_info_new_from_header(CamelFolderSummary *, struct _camel_header_raw *);
static void message_info_free(CamelFolderSummary *, CamelMessageInfo *mi);

static gint maildir_summary_load(CamelLocalSummary *cls, gint forceindex, GError **error);
static gint maildir_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, GError **error);
static gint maildir_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, GError **error);
static CamelMessageInfo *maildir_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *, GError **error);

static gchar *maildir_summary_next_uid_string(CamelFolderSummary *s);
static gint maildir_summary_decode_x_evolution(CamelLocalSummary *cls, const gchar *xev, CamelLocalMessageInfo *mi);
static gchar *maildir_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelLocalMessageInfo *mi);

struct _CamelMaildirSummaryPrivate {
	gchar *current_file;
	gchar *hostname;

	GHashTable *load_map;
	GMutex *summary_lock;
};

G_DEFINE_TYPE (CamelMaildirSummary, camel_maildir_summary, CAMEL_TYPE_LOCAL_SUMMARY)

static void
maildir_summary_finalize (GObject *object)
{
	CamelMaildirSummaryPrivate *priv;

	priv = CAMEL_MAILDIR_SUMMARY_GET_PRIVATE (object);

	g_free (priv->hostname);
	g_mutex_free (priv->summary_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_maildir_summary_parent_class)->finalize (object);
}

static void
camel_maildir_summary_class_init (CamelMaildirSummaryClass *class)
{
	GObjectClass *object_class;
	CamelFolderSummaryClass *folder_summary_class;
	CamelLocalSummaryClass *local_summary_class;

	g_type_class_add_private (class, sizeof (CamelMaildirSummaryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = maildir_summary_finalize;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (class);
	folder_summary_class->message_info_size = sizeof (CamelMaildirMessageInfo);
	folder_summary_class->content_info_size = sizeof (CamelMaildirMessageContentInfo);
	folder_summary_class->message_info_migrate = message_info_migrate;
	folder_summary_class->message_info_new_from_header = message_info_new_from_header;
	folder_summary_class->message_info_free = message_info_free;
	folder_summary_class->next_uid_string = maildir_summary_next_uid_string;

	local_summary_class = CAMEL_LOCAL_SUMMARY_CLASS (class);
	local_summary_class->load = maildir_summary_load;
	local_summary_class->check = maildir_summary_check;
	local_summary_class->sync = maildir_summary_sync;
	local_summary_class->add = maildir_summary_add;
	local_summary_class->encode_x_evolution = maildir_summary_encode_x_evolution;
	local_summary_class->decode_x_evolution = maildir_summary_decode_x_evolution;
}

static void
camel_maildir_summary_init (CamelMaildirSummary *maildir_summary)
{
	CamelFolderSummary *folder_summary;
	gchar hostname[256];

	folder_summary = CAMEL_FOLDER_SUMMARY (maildir_summary);

	maildir_summary->priv =
		CAMEL_MAILDIR_SUMMARY_GET_PRIVATE (maildir_summary);

	/* set unique file version */
	folder_summary->version += CAMEL_MAILDIR_SUMMARY_VERSION;

	if (gethostname(hostname, 256) == 0) {
		maildir_summary->priv->hostname = g_strdup(hostname);
	} else {
		maildir_summary->priv->hostname = g_strdup("localhost");
	}
	maildir_summary->priv->summary_lock = g_mutex_new ();
}

/**
 * camel_maildir_summary_new:
 * @folder: parent folder.
 * @filename: Path to root of this maildir directory (containing new/tmp/cur directories).
 * @index: Index if one is reqiured.
 *
 * Create a new CamelMaildirSummary object.
 *
 * Returns: A new #CamelMaildirSummary object.
 **/
CamelMaildirSummary
*camel_maildir_summary_new(struct _CamelFolder *folder, const gchar *filename, const gchar *maildirdir, CamelIndex *index)
{
	CamelMaildirSummary *o;

	o = g_object_new (CAMEL_TYPE_MAILDIR_SUMMARY, NULL);
	((CamelFolderSummary *)o)->folder = folder;
	if (folder) {
		CamelStore *parent_store;

		parent_store = camel_folder_get_parent_store (folder);
		camel_db_set_collate (parent_store->cdb_r, "dreceived", NULL, NULL);
		((CamelFolderSummary *)o)->sort_by = "dreceived";
		((CamelFolderSummary *)o)->collate = NULL;
	}
	camel_local_summary_construct((CamelLocalSummary *)o, filename, maildirdir, index);
	return o;
}

/* the 'standard' maildir flags.  should be defined in sorted order. */
static struct {
	gchar flag;
	guint32 flagbit;
} flagbits[] = {
	{ 'D', CAMEL_MESSAGE_DRAFT },
	{ 'F', CAMEL_MESSAGE_FLAGGED },
	/*{ 'P', CAMEL_MESSAGE_FORWARDED },*/
	{ 'R', CAMEL_MESSAGE_ANSWERED },
	{ 'S', CAMEL_MESSAGE_SEEN },
	{ 'T', CAMEL_MESSAGE_DELETED },
};

/* convert the uid + flags into a unique:info maildir format */
gchar *camel_maildir_summary_info_to_name(const CamelMaildirMessageInfo *info)
{
	const gchar *uid;
	gchar *p, *buf;
	gint i;

	uid = camel_message_info_uid (info);
	buf = g_alloca (strlen (uid) + strlen (":2,") + G_N_ELEMENTS (flagbits) + 1);
	p = buf + sprintf (buf, "%s:2,", uid);
	for (i = 0; i < G_N_ELEMENTS (flagbits); i++) {
		if (info->info.info.flags & flagbits[i].flagbit)
			*p++ = flagbits[i].flag;
	}

	*p = 0;

	return g_strdup(buf);
}

/* returns 0 if the info matches (or there was none), otherwise we changed it */
gint camel_maildir_summary_name_to_info(CamelMaildirMessageInfo *info, const gchar *name)
{
	gchar *p, c;
	guint32 set = 0;	/* what we set */
	/*guint32 all = 0;*/	/* all flags */
	gint i;

	p = strstr (name, ":2,");

	if (p) {
		p+=3;
		while ((c = *p++)) {
			/* we could assume that the flags are in order, but its just as easy not to require */
			for (i = 0; i < G_N_ELEMENTS (flagbits); i++) {
				if (flagbits[i].flag == c && (info->info.info.flags & flagbits[i].flagbit) == 0) {
					set |= flagbits[i].flagbit;
				}
				/*all |= flagbits[i].flagbit;*/
			}
		}

		/* changed? */
		/*if ((info->flags & all) != set) {*/
		if ((info->info.info.flags & set) != set) {
			/* ok, they did change, only add the new flags ('merge flags'?) */
			/*info->flags &= all;  if we wanted to set only the new flags, which we probably dont */
			info->info.info.flags |= set;
			return 1;
		}
	}

	return 0;
}

/* for maildir, x-evolution isn't used, so dont try and get anything out of it */
static gint maildir_summary_decode_x_evolution(CamelLocalSummary *cls, const gchar *xev, CamelLocalMessageInfo *mi)
{
	return -1;
}

static gchar *maildir_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelLocalMessageInfo *mi)
{
	return NULL;
}

/* FIXME:
   both 'new' and 'add' will try and set the filename, this is not ideal ...
*/
static CamelMessageInfo *
maildir_summary_add (CamelLocalSummary *cls,
                     CamelMimeMessage *msg,
                     const CamelMessageInfo *info,
                     CamelFolderChangeInfo *changes,
                     GError **error)
{
	CamelLocalSummaryClass *local_summary_class;
	CamelMaildirMessageInfo *mi;

	/* Chain up to parent's add() method. */
	local_summary_class = CAMEL_LOCAL_SUMMARY_CLASS (camel_maildir_summary_parent_class);
	mi = (CamelMaildirMessageInfo *) local_summary_class->add (
		cls, msg, info, changes, error);
	if (mi) {
		if (info) {
			camel_maildir_info_set_filename(mi, camel_maildir_summary_info_to_name(mi));
			d(printf("Setting filename to %s\n", camel_maildir_info_filename(mi)));
		}
	}

	return (CamelMessageInfo *)mi;
}

static CamelMessageInfo *
message_info_new_from_header(CamelFolderSummary * s, struct _camel_header_raw *h)
{
	CamelMessageInfo *mi, *info;
	CamelMaildirSummary *mds = (CamelMaildirSummary *)s;
	CamelMaildirMessageInfo *mdi;
	const gchar *uid;

	mi = ((CamelFolderSummaryClass *) camel_maildir_summary_parent_class)->message_info_new_from_header(s, h);
	/* assign the uid and new filename */
	if (mi) {
		mdi = (CamelMaildirMessageInfo *)mi;

		uid = camel_message_info_uid(mi);
		if (uid==NULL || uid[0] == 0)
			mdi->info.info.uid = camel_pstring_add (camel_folder_summary_next_uid_string(s), TRUE);

		/* handle 'duplicates' */
		info = camel_folder_summary_peek_info (s, uid);
		if (info) {
			d(printf("already seen uid '%s', just summarising instead\n", uid));
			camel_message_info_free(mi);
			mdi = (CamelMaildirMessageInfo *)(mi = info);
		}

		/* with maildir we know the real received date, from the filename */
		mdi->info.info.date_received = strtoul(camel_message_info_uid(mi), NULL, 10);

		if (mds->priv->current_file) {
#if 0
			gchar *p1, *p2, *p3;
			gulong uid;
#endif
			/* if setting from a file, grab the flags from it */
			camel_maildir_info_set_filename(mi, g_strdup(mds->priv->current_file));
			camel_maildir_summary_name_to_info(mdi, mds->priv->current_file);

#if 0
			/* Actually, I dont think all this effort is worth it at all ... */

			/* also, see if we can extract the next-id from tne name, and safe-if-fy ourselves against collisions */
			/* we check for something.something_number.something */
			p1 = strchr(mdi->filename, '.');
			if (p1) {
				p2 = strchr(p1+1, '.');
				p3 = strchr(p1+1, '_');
				if (p2 && p3 && p3<p2) {
					uid = strtoul(p3+1, &p1, 10);
					if (p1 == p2 && uid>0)
						camel_folder_summary_set_uid(s, uid);
				}
			}
#endif
		} else {
			/* if creating a file, set its name from the flags we have */
			camel_maildir_info_set_filename(mdi, camel_maildir_summary_info_to_name(mdi));
			d(printf("Setting filename to %s\n", camel_maildir_info_filename(mi)));
		}
	}

	return mi;
}

static void
message_info_free(CamelFolderSummary *s, CamelMessageInfo *mi)
{
	CamelMaildirMessageInfo *mdi = (CamelMaildirMessageInfo *)mi;

	g_free(mdi->filename);

	((CamelFolderSummaryClass *) camel_maildir_summary_parent_class)->message_info_free(s, mi);
}

static gchar *maildir_summary_next_uid_string(CamelFolderSummary *s)
{
	CamelMaildirSummary *mds = (CamelMaildirSummary *)s;

	d(printf("next uid string called?\n"));

	/* if we have a current file, then use that to get the uid */
	if (mds->priv->current_file) {
		gchar *cln;

		cln = strchr(mds->priv->current_file, ':');
		if (cln)
			return g_strndup(mds->priv->current_file, cln-mds->priv->current_file);
		else
			return g_strdup(mds->priv->current_file);
	} else {
		/* the first would probably work, but just to be safe, check for collisions */
#if 0
		return g_strdup_printf("%ld.%d_%u.%s", time(0), getpid(), camel_folder_summary_next_uid(s), mds->priv->hostname);
#else
		CamelLocalSummary *cls = (CamelLocalSummary *)s;
		gchar *name = NULL, *uid = NULL;
		struct stat st;
		gint retry = 0;
		guint32 nextuid = camel_folder_summary_next_uid(s);

		/* we use time.pid_count.hostname */
		do {
			if (retry > 0) {
				g_free(name);
				g_free(uid);
				g_usleep(2*G_USEC_PER_SEC);
			}
			uid = g_strdup_printf("%lld.%d_%u.%s", time(NULL), getpid(), nextuid, mds->priv->hostname);
			name = g_strdup_printf("%s/tmp/%s", cls->folder_path, uid);
			retry++;
		} while (g_stat(name, &st) == 0 && retry<3);

		/* I dont know what we're supposed to do if it fails to find a unique name?? */

		g_free(name);
		return uid;
#endif
	}
}

static CamelMessageInfo *
message_info_migrate (CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *mi;
	CamelMaildirSummary *mds = (CamelMaildirSummary *)s;

	mi = ((CamelFolderSummaryClass *) camel_maildir_summary_parent_class)->message_info_migrate (s, in);
	if (mi) {
		gchar *name;

		if (mds->priv->load_map
		    && (name = g_hash_table_lookup(mds->priv->load_map, camel_message_info_uid(mi)))) {
			d(printf("Setting filename of %s to %s\n", camel_message_info_uid(mi), name));
			camel_maildir_info_set_filename(mi, g_strdup(name));
			camel_maildir_summary_name_to_info((CamelMaildirMessageInfo *)mi, name);
		}
	}

	return mi;
}

static gint
maildir_summary_load (CamelLocalSummary *cls,
                      gint forceindex,
                      GError **error)
{
	CamelLocalSummaryClass *local_summary_class;
	gchar *cur;
	DIR *dir;
	struct dirent *d;
	CamelMaildirSummary *mds = (CamelMaildirSummary *)cls;
	gchar *uid;
	CamelMemPool *pool;
	gint ret;

	cur = g_strdup_printf("%s/cur", cls->folder_path);

	d(printf("pre-loading uid <> filename map\n"));

	dir = opendir(cur);
	if (dir == NULL) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Cannot open maildir directory path: %s: %s"),
			cls->folder_path, g_strerror (errno));
		g_free(cur);
		return -1;
	}

	mds->priv->load_map = g_hash_table_new(g_str_hash, g_str_equal);
	pool = camel_mempool_new(1024, 512, CAMEL_MEMPOOL_ALIGN_BYTE);

	while ((d = readdir(dir))) {
		if (d->d_name[0] == '.')
			continue;

		/* map the filename -> uid */
		uid = strchr(d->d_name, ':');
		if (uid) {
			gint len = uid-d->d_name;
			uid = camel_mempool_alloc(pool, len+1);
			memcpy(uid, d->d_name, len);
			uid[len] = 0;
			g_hash_table_insert(mds->priv->load_map, uid, camel_mempool_strdup(pool, d->d_name));
		} else {
			uid = camel_mempool_strdup(pool, d->d_name);
			g_hash_table_insert(mds->priv->load_map, uid, uid);
		}
	}
	closedir(dir);
	g_free(cur);

	/* Chain up to parent's load() method. */
	local_summary_class = CAMEL_LOCAL_SUMMARY_CLASS (camel_maildir_summary_parent_class);
	ret = local_summary_class->load (cls, forceindex, error);

	g_hash_table_destroy(mds->priv->load_map);
	mds->priv->load_map = NULL;
	camel_mempool_destroy(pool);

	return ret;
}

static gint
camel_maildir_summary_add (CamelLocalSummary *cls, const gchar *name, gint forceindex)
{
	CamelMaildirSummary *maildirs = (CamelMaildirSummary *)cls;
	gchar *filename = g_strdup_printf("%s/cur/%s", cls->folder_path, name);
	gint fd;
	CamelMimeParser *mp;

	d(printf("summarising: %s\n", name));

	fd = open(filename, O_RDONLY|O_LARGEFILE);
	if (fd == -1) {
		g_warning ("Cannot summarise/index: %s: %s", filename, g_strerror (errno));
		g_free(filename);
		return -1;
	}
	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from(mp, FALSE);
	camel_mime_parser_init_with_fd(mp, fd);
	if (cls->index && (forceindex || !camel_index_has_name(cls->index, name))) {
		d(printf("forcing indexing of message content\n"));
		camel_folder_summary_set_index((CamelFolderSummary *)maildirs, cls->index);
	} else {
		camel_folder_summary_set_index((CamelFolderSummary *)maildirs, NULL);
	}
	maildirs->priv->current_file = (gchar *)name;
	camel_folder_summary_add_from_parser((CamelFolderSummary *)maildirs, mp);
	g_object_unref (mp);
	maildirs->priv->current_file = NULL;
	camel_folder_summary_set_index((CamelFolderSummary *)maildirs, NULL);
	g_free(filename);
	return 0;
}

struct _remove_data {
	CamelLocalSummary *cls;
	CamelFolderChangeInfo *changes;
};

static void
remove_summary(gchar *key, CamelMessageInfo *info, struct _remove_data *rd)
{
	d(printf("removing message %s from summary\n", key));
	if (rd->cls->index)
		camel_index_delete_name(rd->cls->index, camel_message_info_uid(info));
	if (rd->changes)
		camel_folder_change_info_remove_uid(rd->changes, key);
	camel_folder_summary_remove((CamelFolderSummary *)rd->cls, info);
	camel_message_info_free(info);
}

static gint
maildir_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changes, GError **error)
{
	DIR *dir;
	struct dirent *d;
	gchar *p;
	CamelMessageInfo *info;
	CamelMaildirMessageInfo *mdi;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	GHashTable *left;
	gint i, count, total;
	gint forceindex;
	gchar *new, *cur;
	gchar *uid;
	struct _remove_data rd = { cls, changes };

	g_mutex_lock (((CamelMaildirSummary *) cls)->priv->summary_lock);

	new = g_strdup_printf("%s/new", cls->folder_path);
	cur = g_strdup_printf("%s/cur", cls->folder_path);

	d(printf("checking summary ...\n"));

	camel_operation_start(NULL, _("Checking folder consistency"));

	/* scan the directory, check for mail files not in the index, or index entries that
	   no longer exist */
	dir = opendir(cur);
	if (dir == NULL) {
		g_set_error (
			error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			_("Cannot open maildir directory path: %s: %s"),
			cls->folder_path, g_strerror (errno));
		g_free(cur);
		g_free(new);
		camel_operation_end(NULL);
		g_mutex_unlock (((CamelMaildirSummary *) cls)->priv->summary_lock);
		return -1;
	}

	/* keeps track of all uid's that have not been processed */
	left = g_hash_table_new(g_str_hash, g_str_equal);
	camel_folder_summary_prepare_fetch_all (s, error);
	count = camel_folder_summary_count (s);
	forceindex = count == 0;
	for (i=0;i<count;i++) {
		info = camel_folder_summary_index((CamelFolderSummary *)cls, i);
		if (info) {
			g_hash_table_insert(left, (gchar *)camel_message_info_uid(info), info);
		}
	}

	/* joy, use this to pre-count the total, so we can report progress meaningfully */
	total = 0;
	count = 0;
	while ((d = readdir(dir)))
		total++;
	rewinddir(dir);

	while ((d = readdir(dir))) {
		gint pc = count * 100 / total;

		camel_operation_progress(NULL, pc);
		count++;

		/* FIXME: also run stat to check for regular file */
		p = d->d_name;
		if (p[0] == '.')
			continue;

		/* map the filename -> uid */
		uid = strchr(d->d_name, ':');
		if (uid)
			uid = g_strndup(d->d_name, uid-d->d_name);
		else
			uid = g_strdup(d->d_name);

		info = g_hash_table_lookup(left, uid);
		if (info) {
			camel_message_info_free(info);
			g_hash_table_remove(left, uid);
		}

		info = camel_folder_summary_uid((CamelFolderSummary *)cls, uid);
		if (info == NULL) {
			/* must be a message incorporated by another client, this is not a 'recent' uid */
			if (camel_maildir_summary_add (cls, d->d_name, forceindex) == 0)
				if (changes)
					camel_folder_change_info_add_uid(changes, uid);
		} else {
			const gchar *filename;

			if (cls->index && (!camel_index_has_name(cls->index, uid))) {
				/* message_info_new will handle duplicates */
				camel_maildir_summary_add(cls, d->d_name, forceindex);
			}

			mdi = (CamelMaildirMessageInfo *)info;
			filename = camel_maildir_info_filename(mdi);
			/* TODO: only store the extension in the mdi->filename struct, not the whole lot */
			if (filename == NULL || strcmp(filename, d->d_name) != 0) {
				g_free(mdi->filename);
				mdi->filename = g_strdup(d->d_name);
			}
			camel_message_info_free(info);
		}
		g_free(uid);
	}
	closedir(dir);
	g_hash_table_foreach(left, (GHFunc)remove_summary, &rd);
	g_hash_table_destroy(left);

	camel_operation_end(NULL);

	camel_operation_start(NULL, _("Checking for new messages"));

	/* now, scan new for new messages, and copy them to cur, and so forth */
	dir = opendir(new);
	if (dir != NULL) {
		total = 0;
		count = 0;
		while ((d = readdir(dir)))
			total++;
		rewinddir(dir);

		while ((d = readdir(dir))) {
			gchar *name, *newname, *destname, *destfilename;
			gchar *src, *dest;
			gint pc = count * 100 / total;

			camel_operation_progress(NULL, pc);
			count++;

			name = d->d_name;
			if (name[0] == '.')
				continue;

			/* already in summary?  shouldn't happen, but just incase ... */
			if ((info = camel_folder_summary_uid((CamelFolderSummary *)cls, name))) {
				camel_message_info_free(info);
				newname = destname = camel_folder_summary_next_uid_string(s);
			} else {
				gchar *nm;
				newname = g_strdup (name);
				nm =strrchr (newname, ':');
				if (nm)
					*nm = '\0';
				destname = newname;
			}

			/* copy this to the destination folder, use 'standard' semantics for maildir info field */
			src = g_strdup_printf("%s/%s", new, name);
			destfilename = g_strdup_printf("%s:2,", destname);
			dest = g_strdup_printf("%s/%s", cur, destfilename);

			/* FIXME: This should probably use link/unlink */

			if (g_rename (src, dest) == 0) {
				camel_maildir_summary_add (cls, destfilename, forceindex);
				if (changes) {
					camel_folder_change_info_add_uid(changes, destname);
					camel_folder_change_info_recent_uid(changes, destname);
				}
			} else {
				/* else?  we should probably care about failures, but wont */
				g_warning("Failed to move new maildir message %s to cur %s", src, dest);
			}

			/* c strings are painful to work with ... */
			g_free(destfilename);
			g_free(newname);
			g_free(src);
			g_free(dest);
		}
		camel_operation_end(NULL);
		closedir(dir);
	}

	g_free(new);
	g_free(cur);

	g_mutex_unlock (((CamelMaildirSummary *) cls)->priv->summary_lock);

	return 0;
}

/* sync the summary with the ondisk files. */
static gint
maildir_summary_sync (CamelLocalSummary *cls,
                      gboolean expunge,
                      CamelFolderChangeInfo *changes,
                      GError **error)
{
	CamelLocalSummaryClass *local_summary_class;
	gint count, i;
	CamelMessageInfo *info;
	CamelMaildirMessageInfo *mdi;
	gchar *name;
	struct stat st;

	d(printf("summary_sync(expunge=%s)\n", expunge?"true":"false"));

	if (camel_local_summary_check(cls, changes, error) == -1)
		return -1;

	camel_operation_start(NULL, _("Storing folder"));

	camel_folder_summary_prepare_fetch_all ((CamelFolderSummary *)cls, error);
	count = camel_folder_summary_count((CamelFolderSummary *)cls);
	for (i=count-1;i>=0;i--) {
		camel_operation_progress(NULL, (count-i)*100/count);

		info = camel_folder_summary_index((CamelFolderSummary *)cls, i);
		mdi = (CamelMaildirMessageInfo *)info;
		if (mdi && (mdi->info.info.flags & CAMEL_MESSAGE_DELETED) && expunge) {
			name = g_strdup_printf("%s/cur/%s", cls->folder_path, camel_maildir_info_filename(mdi));
			d(printf("deleting %s\n", name));
			if (unlink(name) == 0 || errno==ENOENT) {

				/* FIXME: put this in folder_summary::remove()? */
				if (cls->index)
					camel_index_delete_name(cls->index, camel_message_info_uid(info));

				camel_folder_change_info_remove_uid(changes, camel_message_info_uid(info));
				camel_folder_summary_remove((CamelFolderSummary *)cls, info);
			}
			g_free(name);
		} else if (mdi && (mdi->info.info.flags & CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			gchar *newname = camel_maildir_summary_info_to_name(mdi);
			gchar *dest;

			/* do we care about additional metainfo stored inside the message? */
			/* probably should all go in the filename? */

			/* have our flags/ i.e. name changed? */
			if (strcmp(newname, camel_maildir_info_filename(mdi))) {
				name = g_strdup_printf("%s/cur/%s", cls->folder_path, camel_maildir_info_filename(mdi));
				dest = g_strdup_printf("%s/cur/%s", cls->folder_path, newname);
				g_rename (name, dest);
				if (g_stat(dest, &st) == -1) {
					/* we'll assume it didn't work, but dont change anything else */
					g_free(newname);
				} else {
					/* TODO: If this is made mt-safe, then this code could be a problem, since
					   the estrv is being modified.
					   Sigh, this may mean the maildir name has to be cached another way */
					g_free(mdi->filename);
					mdi->filename = newname;
				}
				g_free(name);
				g_free(dest);
			} else {
				g_free(newname);
			}

			/* strip FOLDER_MESSAGE_FLAGED, etc */
			mdi->info.info.flags &= 0xffff;
		}
		camel_message_info_free(info);
	}

	camel_operation_end(NULL);

	/* Chain up to parent's sync() method. */
	local_summary_class = CAMEL_LOCAL_SUMMARY_CLASS (camel_maildir_summary_parent_class);
	return local_summary_class->sync (cls, expunge, changes, error);
}


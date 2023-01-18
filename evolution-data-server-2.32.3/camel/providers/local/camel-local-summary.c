/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "camel-local-summary.h"

#define w(x)
#define io(x)
#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_LOCAL_SUMMARY_VERSION (1)

#define EXTRACT_FIRST_DIGIT(val) val=strtoul (part, &part, 10);

static CamelFIRecord * summary_header_to_db (CamelFolderSummary *, GError **error);
static gint summary_header_from_db (CamelFolderSummary *, CamelFIRecord *);

static gint summary_header_load (CamelFolderSummary *, FILE *);
static gint summary_header_save (CamelFolderSummary *, FILE *);

static CamelMessageInfo * message_info_new_from_header (CamelFolderSummary *, struct _camel_header_raw *);

static gint local_summary_decode_x_evolution(CamelLocalSummary *cls, const gchar *xev, CamelLocalMessageInfo *mi);
static gchar *local_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelLocalMessageInfo *mi);

static gint local_summary_load(CamelLocalSummary *cls, gint forceindex, GError **error);
static gint local_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, GError **error);
static gint local_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, GError **error);
static CamelMessageInfo *local_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *, GError **error);
static gint local_summary_need_index(void);

G_DEFINE_TYPE (CamelLocalSummary, camel_local_summary, CAMEL_TYPE_FOLDER_SUMMARY)

static void
local_summary_dispose (GObject *object)
{
	CamelLocalSummary *local_summary;

	local_summary = CAMEL_LOCAL_SUMMARY (object);

	if (local_summary->index != NULL) {
		g_object_unref (local_summary->index);
		local_summary->index = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (camel_local_summary_parent_class)->dispose (object);
}

static void
local_summary_finalize (GObject *object)
{
	CamelLocalSummary *local_summary;

	local_summary = CAMEL_LOCAL_SUMMARY (object);

	g_free (local_summary->folder_path);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_local_summary_parent_class)->finalize (object);
}

static void
camel_local_summary_class_init (CamelLocalSummaryClass *class)
{
	GObjectClass *object_class;
	CamelFolderSummaryClass *folder_summary_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = local_summary_dispose;
	object_class->finalize = local_summary_finalize;

	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (class);
	folder_summary_class->message_info_size = sizeof (CamelLocalMessageInfo);
	folder_summary_class->content_info_size = sizeof (CamelMessageContentInfo);
	folder_summary_class->summary_header_load = summary_header_load;
	folder_summary_class->summary_header_save = summary_header_save;
	folder_summary_class->summary_header_from_db = summary_header_from_db;
	folder_summary_class->summary_header_to_db = summary_header_to_db;
	folder_summary_class->message_info_new_from_header  = message_info_new_from_header;

	class->load = local_summary_load;
	class->check = local_summary_check;
	class->sync = local_summary_sync;
	class->add = local_summary_add;
	class->encode_x_evolution = local_summary_encode_x_evolution;
	class->decode_x_evolution = local_summary_decode_x_evolution;
	class->need_index = local_summary_need_index;
}

static void
camel_local_summary_init (CamelLocalSummary *local_summary)
{
	CamelFolderSummary *folder_summary;

	folder_summary = CAMEL_FOLDER_SUMMARY (local_summary);

	/* and a unique file version */
	folder_summary->version += CAMEL_LOCAL_SUMMARY_VERSION;
}

void
camel_local_summary_construct(CamelLocalSummary *new, const gchar *filename, const gchar *local_name, CamelIndex *index)
{
	camel_folder_summary_set_build_content(CAMEL_FOLDER_SUMMARY(new), FALSE);
	camel_folder_summary_set_filename(CAMEL_FOLDER_SUMMARY(new), filename);
	new->folder_path = g_strdup(local_name);
	new->index = index;
	if (index)
		g_object_ref (index);
}

static gint
local_summary_load(CamelLocalSummary *cls, gint forceindex, GError **error)
{
	d(g_print ("\nlocal_summary_load called \n"));
	return camel_folder_summary_load_from_db ((CamelFolderSummary *)cls, error);
}

/* load/check the summary */
gint
camel_local_summary_load(CamelLocalSummary *cls, gint forceindex, GError **error)
{
	CamelLocalSummaryClass *class;

	d(printf("Loading summary ...\n"));

	class = CAMEL_LOCAL_SUMMARY_GET_CLASS (cls);

	if ((forceindex && class->need_index())
	    || class->load(cls, forceindex, error) == -1) {
		w(g_warning("Could not load summary: flags may be reset"));
		camel_folder_summary_clear((CamelFolderSummary *)cls);
		return -1;
	}

	return 0;
}

void camel_local_summary_check_force(CamelLocalSummary *cls)
{
	cls->check_force = 1;
}

gchar *
camel_local_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelLocalMessageInfo *info)
{
	return CAMEL_LOCAL_SUMMARY_GET_CLASS (cls)->encode_x_evolution (cls, info);
}

gint
camel_local_summary_decode_x_evolution(CamelLocalSummary *cls, const gchar *xev, CamelLocalMessageInfo *info)
{
	return CAMEL_LOCAL_SUMMARY_GET_CLASS (cls)->decode_x_evolution (cls, xev, info);
}

/*#define DOSTATS*/
#ifdef DOSTATS
struct _stat_info {
	gint mitotal;
	gint micount;
	gint citotal;
	gint cicount;
	gint msgid;
	gint msgcount;
};

static void
do_stat_ci(CamelLocalSummary *cls, struct _stat_info *info, CamelMessageContentInfo *ci)
{
	info->cicount++;
	info->citotal += ((CamelFolderSummary *)cls)->content_info_size /*+ 4 memchunks are 1/4 byte overhead per mi */;
	if (ci->id)
		info->citotal += strlen(ci->id) + 4;
	if (ci->description)
		info->citotal += strlen(ci->description) + 4;
	if (ci->encoding)
		info->citotal += strlen(ci->encoding) + 4;
	if (ci->type) {
		CamelContentType *ct = ci->type;
		struct _camel_header_param *param;

		info->citotal += sizeof(*ct) + 4;
		if (ct->type)
			info->citotal += strlen(ct->type) + 4;
		if (ct->subtype)
			info->citotal += strlen(ct->subtype) + 4;
		param = ct->params;
		while (param) {
			info->citotal += sizeof(*param) + 4;
			if (param->name)
				info->citotal += strlen(param->name)+4;
			if (param->value)
				info->citotal += strlen(param->value)+4;
			param = param->next;
		}
	}
	ci = ci->childs;
	while (ci) {
		do_stat_ci(cls, info, ci);
		ci = ci->next;
	}
}

static void
do_stat_mi(CamelLocalSummary *cls, struct _stat_info *info, CamelMessageInfo *mi)
{
	info->micount++;
	info->mitotal += ((CamelFolderSummary *)cls)->content_info_size /*+ 4*/;

	if (mi->subject)
		info->mitotal += strlen(mi->subject) + 4;
	if (mi->to)
		info->mitotal += strlen(mi->to) + 4;
	if (mi->from)
		info->mitotal += strlen(mi->from) + 4;
	if (mi->cc)
		info->mitotal += strlen(mi->cc) + 4;
	if (mi->uid)
		info->mitotal += strlen(mi->uid) + 4;

	if (mi->references) {
		info->mitotal += (mi->references->size-1) * sizeof(CamelSummaryMessageID) + sizeof(CamelSummaryReferences) + 4;
		info->msgid += (mi->references->size) * sizeof(CamelSummaryMessageID);
		info->msgcount += mi->references->size;
	}

	/* dont have any user flags yet */

	if (mi->content) {
		do_stat_ci(cls, info, mi->content);
	}
}

#endif

gint
camel_local_summary_check (CamelLocalSummary *cls,
                           CamelFolderChangeInfo *changeinfo,
                           GError **error)
{
	CamelLocalSummaryClass *local_summary_class;
	gint ret;

	local_summary_class = CAMEL_LOCAL_SUMMARY_GET_CLASS (cls);
	ret = local_summary_class->check(cls, changeinfo, error);

#ifdef DOSTATS
	if (ret != -1) {
		gint i;
		CamelFolderSummary *s = (CamelFolderSummary *)cls;
		struct _stat_info stats = { 0 };

		for (i=0;i<camel_folder_summary_count(s);i++) {
			CamelMessageInfo *info = camel_folder_summary_index(s, i);
			do_stat_mi(cls, &stats, info);
			camel_message_info_free(info);
		}

		printf("\nMemory used by summary:\n\n");
		printf("Total of %d messages\n", camel_folder_summary_count(s));
		printf("Total: %d bytes (ave %f)\n", stats.citotal + stats.mitotal,
		       (double)(stats.citotal+stats.mitotal)/(double)camel_folder_summary_count(s));
		printf("Message Info: %d (ave %f)\n", stats.mitotal, (double)stats.mitotal/(double)stats.micount);
		printf("Content Info; %d (ave %f) count %d\n", stats.citotal, (double)stats.citotal/(double)stats.cicount, stats.cicount);
		printf("message id's: %d (ave %f) count %d\n", stats.msgid, (double)stats.msgid/(double)stats.msgcount, stats.msgcount);
	}
#endif
	return ret;
}

gint
camel_local_summary_sync (CamelLocalSummary *cls,
                          gboolean expunge,
                          CamelFolderChangeInfo *changeinfo,
                          GError **error)
{
	CamelLocalSummaryClass *local_summary_class;

	local_summary_class = CAMEL_LOCAL_SUMMARY_GET_CLASS (cls);

	return local_summary_class->sync (cls, expunge, changeinfo, error);
}

CamelMessageInfo *
camel_local_summary_add (CamelLocalSummary *cls,
                         CamelMimeMessage *msg,
                         const CamelMessageInfo *info,
                         CamelFolderChangeInfo *ci,
                         GError **error)
{
	CamelLocalSummaryClass *local_summary_class;

	local_summary_class = CAMEL_LOCAL_SUMMARY_GET_CLASS (cls);

	return local_summary_class->add (cls, msg, info, ci, error);
}

/**
 * camel_local_summary_write_headers:
 * @fd:
 * @header:
 * @xevline:
 * @status:
 * @xstatus:
 *
 * Write a bunch of headers to the file @fd.  IF xevline is non NULL, then
 * an X-Evolution header line is created at the end of all of the headers.
 * If @status is non NULL, then a Status header line is also written.
 * The headers written are termianted with a blank line.
 *
 * Returns: -1 on error, otherwise the number of bytes written.
 **/
gint
camel_local_summary_write_headers(gint fd, struct _camel_header_raw *header, const gchar *xevline, const gchar *status, const gchar *xstatus)
{
	gint outlen = 0, len;
	gint newfd;
	FILE *out;

	/* dum de dum, maybe the whole sync function should just use stdio for output */
	newfd = dup(fd);
	if (newfd == -1)
		return -1;

	out = fdopen(newfd, "w");
	if (out == NULL) {
		close(newfd);
		errno = EINVAL;
		return -1;
	}

	while (header) {
		if (strcmp(header->name, "X-Evolution") != 0
		    && (status == NULL || strcmp(header->name, "Status") != 0)
		    && (xstatus == NULL || strcmp(header->name, "X-Status") != 0)) {
			len = fprintf(out, "%s:%s\n", header->name, header->value);
			if (len == -1) {
				fclose(out);
				return -1;
			}
			outlen += len;
		}
		header = header->next;
	}

	if (status) {
		len = fprintf(out, "Status: %s\n", status);
		if (len == -1) {
			fclose(out);
			return -1;
		}
		outlen += len;
	}

	if (xstatus) {
		len = fprintf(out, "X-Status: %s\n", xstatus);
		if (len == -1) {
			fclose(out);
			return -1;
		}
		outlen += len;
	}

	if (xevline) {
		len = fprintf(out, "X-Evolution: %s\n", xevline);
		if (len == -1) {
			fclose(out);
			return -1;
		}
		outlen += len;
	}

	len = fprintf(out, "\n");
	if (len == -1) {
		fclose(out);
		return -1;
	}
	outlen += len;

	if (fclose(out) == -1)
		return -1;

	return outlen;
}

static gint
local_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, GError **error)
{
	/* FIXME: sync index here ? */
	return 0;
}

static gint
local_summary_sync (CamelLocalSummary *cls,
                    gboolean expunge,
                    CamelFolderChangeInfo *changeinfo,
                    GError **error)
{
	CamelFolderSummary *folder_summary;

	folder_summary = CAMEL_FOLDER_SUMMARY (cls);

	if (camel_folder_summary_save_to_db (folder_summary, error) == -1) {
		g_warning ("Could not save summary for local providers");
		return -1;
	}

	if (cls->index && camel_index_sync(cls->index) == -1) {
		g_warning ("Could not sync index for %s: %s", cls->folder_path, g_strerror (errno));
		return -1;
	}

	return 0;
}

static gint
local_summary_need_index(void) {
	return 1;
}

static void
update_summary (CamelFolderSummary *summary, CamelMessageInfoBase *info, CamelMessageInfoBase *old)
{
	gint unread=0, deleted=0, junk=0;
	guint32 flags = info->flags;
	guint32 oldflags = old->flags;

	if ((flags & CAMEL_MESSAGE_SEEN) != (oldflags & CAMEL_MESSAGE_SEEN))
		unread = (oldflags & CAMEL_MESSAGE_SEEN) ? 1 : -1;

	if ((flags & CAMEL_MESSAGE_DELETED) != (oldflags & CAMEL_MESSAGE_DELETED))
		deleted = (oldflags & CAMEL_MESSAGE_DELETED) ? 1 : -1;

	if ((flags & CAMEL_MESSAGE_JUNK) != (oldflags & CAMEL_MESSAGE_JUNK))
		junk = (oldflags & CAMEL_MESSAGE_JUNK) ? 1 : -1;

	/* Things would already be flagged */

	if (summary) {

		if (unread)
			summary->unread_count -= unread;
		if (deleted)
			summary->deleted_count += deleted;
		if (junk)
			summary->junk_count += junk;
		if (junk && !deleted)
			summary->junk_not_deleted_count += junk;
		if (junk ||  deleted)
			summary->visible_count -= junk ? junk : deleted;
	}

}

static CamelMessageInfo *
local_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *ci, GError **error)
{
	CamelLocalMessageInfo *mi;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	gchar *xev;

	d(printf("Adding message to summary\n"));

	mi = (CamelLocalMessageInfo *)camel_folder_summary_add_from_message((CamelFolderSummary *)cls, msg);
	if (mi) {
		/*d(printf("Added, uid = %s\n", mi->uid));*/
		if (info) {
			const CamelTag *tag = camel_message_info_user_tags(info);
			const CamelFlag *flag = camel_message_info_user_flags(info);

			while (flag) {
				camel_message_info_set_user_flag((CamelMessageInfo *)mi, flag->name, TRUE);
				flag = flag->next;
			}

			while (tag) {
				camel_message_info_set_user_tag((CamelMessageInfo *)mi, tag->name, tag->value);
				tag = tag->next;
			}

			update_summary (s, (CamelMessageInfoBase *) mi, (CamelMessageInfoBase *) info);
			mi->info.flags |= (camel_message_info_flags(info) & 0xffff);
			camel_folder_summary_update_flag_cache (s, mi->info.uid, mi->info.flags);
			mi->info.size = camel_message_info_size(info);
		}

		/* we need to calculate the size ourselves */
		if (mi->info.size == 0) {
			CamelStreamNull *sn = (CamelStreamNull *)camel_stream_null_new();

			camel_data_wrapper_write_to_stream((CamelDataWrapper *)msg, (CamelStream *)sn, NULL);
			mi->info.size = sn->written;
			g_object_unref (sn);
		}

		mi->info.flags &= ~(CAMEL_MESSAGE_FOLDER_NOXEV|CAMEL_MESSAGE_FOLDER_FLAGGED);
		xev = camel_local_summary_encode_x_evolution(cls, mi);
		camel_medium_set_header((CamelMedium *)msg, "X-Evolution", xev);
		g_free(xev);
		camel_folder_change_info_add_uid(ci, camel_message_info_uid(mi));
	} else {
		d(printf("Failed!\n"));
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Unable to add message to summary: unknown reason"));
	}
	return (CamelMessageInfo *)mi;
}

static gchar *
local_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelLocalMessageInfo *mi)
{
	GString *out = g_string_new("");
	struct _camel_header_param *params = NULL;
	GString *val = g_string_new("");
	CamelFlag *flag = mi->info.user_flags;
	CamelTag *tag = mi->info.user_tags;
	gchar *ret;
	const gchar *p, *uidstr;
	guint32 uid;

	/* FIXME: work out what to do with uid's that aren't stored here? */
	/* FIXME: perhaps make that a mbox folder only issue?? */
	p = uidstr = camel_message_info_uid(mi);
	while (*p && isdigit(*p))
		p++;
	if (*p == 0 && sscanf (uidstr, "%u", &uid) == 1) {
		g_string_printf (out, "%08x-%04x", uid, mi->info.flags & 0xffff);
	} else {
		g_string_printf (out, "%s-%04x", uidstr, mi->info.flags & 0xffff);
	}

	if (flag || tag) {
		val = g_string_new ("");

		if (flag) {
			while (flag) {
				g_string_append (val, flag->name);
				if (flag->next)
					g_string_append_c (val, ',');
				flag = flag->next;
			}
			camel_header_set_param (&params, "flags", val->str);
			g_string_truncate (val, 0);
		}
		if (tag) {
			while (tag) {
				g_string_append (val, tag->name);
				g_string_append_c (val, '=');
				g_string_append (val, tag->value);
				if (tag->next)
					g_string_append_c (val, ',');
				tag = tag->next;
			}
			camel_header_set_param (&params, "tags", val->str);
		}
		g_string_free (val, TRUE);
		camel_header_param_list_format_append (out, params);
		camel_header_param_list_free (params);
	}
	ret = out->str;
	g_string_free (out, FALSE);

	return ret;
}

static gint
local_summary_decode_x_evolution(CamelLocalSummary *cls, const gchar *xev, CamelLocalMessageInfo *mi)
{
	struct _camel_header_param *params, *scan;
	guint32 uid, flags;
	gchar *header;
	gint i;
	gchar uidstr[20];

	uidstr[0] = 0;

	/* check for uid/flags */
	header = camel_header_token_decode(xev);
	if (header && strlen(header) == strlen("00000000-0000")
	    && sscanf(header, "%08x-%04x", &uid, &flags) == 2) {
		if (mi)
			sprintf(uidstr, "%u", uid);
	} else {
		g_free(header);
		return -1;
	}
	g_free(header);

	if (mi == NULL)
		return 0;

	/* check for additional data */
	header = strchr(xev, ';');
	if (header) {
		params = camel_header_param_list_decode(header+1);
		scan = params;
		while (scan) {
			if (!g_ascii_strcasecmp(scan->name, "flags")) {
				gchar **flagv = g_strsplit(scan->value, ",", 1000);

				for (i=0;flagv[i];i++)
					camel_message_info_set_user_flag((CamelMessageInfo *)mi, flagv[i], TRUE);
				g_strfreev(flagv);
			} else if (!g_ascii_strcasecmp(scan->name, "tags")) {
				gchar **tagv = g_strsplit(scan->value, ",", 10000);
				gchar *val;

				for (i=0;tagv[i];i++) {
					val = strchr(tagv[i], '=');
					if (val) {
						*val++ = 0;
						camel_message_info_set_user_tag((CamelMessageInfo *)mi, tagv[i], val);
						val[-1]='=';
					}
				}
				g_strfreev(tagv);
			}
			scan = scan->next;
		}
		camel_header_param_list_free(params);
	}

	mi->info.uid = camel_pstring_strdup(uidstr);
	mi->info.flags = flags;

	return 0;
}

static gint
summary_header_from_db (CamelFolderSummary *s, CamelFIRecord *fir)
{
	CamelLocalSummary *cls = (CamelLocalSummary *)s;
	gchar *part, *tmp;

	/* We dont actually add our own headers, but version that we don't anyway */

	if (CAMEL_FOLDER_SUMMARY_CLASS (camel_local_summary_parent_class)->summary_header_from_db(s, fir) == -1)
		return -1;

	part = fir->bdata;
	if (part) {
		EXTRACT_FIRST_DIGIT (cls->version)
	}

	/* keep only the rest of the bdata there (strip our version digit) */
	tmp = g_strdup (part);
	g_free (fir->bdata);
	fir->bdata = tmp;

	return 0;
}

static gint
summary_header_load(CamelFolderSummary *s, FILE *in)
{
	CamelLocalSummary *cls = (CamelLocalSummary *)s;

	/* We dont actually add our own headers, but version that we don't anyway */

	if (CAMEL_FOLDER_SUMMARY_CLASS (camel_local_summary_parent_class)->summary_header_load(s, in) == -1)
		return -1;

	/* Legacy version, version is in summary only */
	if ((s->version & 0xfff) == 0x20c)
		return 0;

	/* otherwise load the version number */
	return camel_file_util_decode_fixed_int32(in, (gint32 *) &cls->version);
}

static struct _CamelFIRecord *
summary_header_to_db (CamelFolderSummary *s, GError **error)
{
	CamelFolderSummaryClass *folder_summary_class;
	struct _CamelFIRecord *fir;

	/* Chain up to parent's summary_header_to_db() method. */
	folder_summary_class = CAMEL_FOLDER_SUMMARY_CLASS (camel_local_summary_parent_class);
	fir = folder_summary_class->summary_header_to_db (s, NULL);
	if (fir)
		fir->bdata = g_strdup_printf ("%d", CAMEL_LOCAL_SUMMARY_VERSION);

	return fir;
}

static gint
summary_header_save(CamelFolderSummary *s, FILE *out)
{
	/*CamelLocalSummary *cls = (CamelLocalSummary *)s;*/

	if (CAMEL_FOLDER_SUMMARY_CLASS (camel_local_summary_parent_class)->summary_header_save(s, out) == -1)
		return -1;

	return camel_file_util_encode_fixed_int32(out, CAMEL_LOCAL_SUMMARY_VERSION);
}

static CamelMessageInfo *
message_info_new_from_header(CamelFolderSummary *s, struct _camel_header_raw *h)
{
	CamelLocalMessageInfo *mi;
	CamelLocalSummary *cls = (CamelLocalSummary *)s;

	mi = (CamelLocalMessageInfo *)CAMEL_FOLDER_SUMMARY_CLASS (camel_local_summary_parent_class)->message_info_new_from_header(s, h);
	if (mi) {
		const gchar *xev;
		gint doindex = FALSE;

		xev = camel_header_raw_find(&h, "X-Evolution", NULL);
		if (xev==NULL || camel_local_summary_decode_x_evolution(cls, xev, mi) == -1) {
			/* to indicate it has no xev header */
			mi->info.flags |= CAMEL_MESSAGE_FOLDER_FLAGGED | CAMEL_MESSAGE_FOLDER_NOXEV;
			camel_pstring_free (mi->info.uid);
			mi->info.uid = camel_pstring_add (camel_folder_summary_next_uid_string(s), TRUE);

			/* shortcut, no need to look it up in the index library */
			doindex = TRUE;
		}

		if (cls->index
		    && (doindex
			|| cls->index_force
			|| !camel_index_has_name(cls->index, camel_message_info_uid(mi)))) {
			d(printf("Am indexing message %s\n", camel_message_info_uid(mi)));
			camel_folder_summary_set_index(s, cls->index);
		} else {
			d(printf("Not indexing message %s\n", camel_message_info_uid(mi)));
			camel_folder_summary_set_index(s, NULL);
		}
	}

	return (CamelMessageInfo *)mi;
}

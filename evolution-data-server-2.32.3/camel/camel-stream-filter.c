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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "camel-stream-filter.h"

#define d(x)

/* use my malloc debugger? */
/*extern void g_check(gpointer mp);*/
#define g_check(x)

#define CAMEL_STREAM_FILTER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), CAMEL_TYPE_STREAM_FILTER, CamelStreamFilterPrivate))

struct _filter {
	struct _filter *next;
	gint id;
	CamelMimeFilter *filter;
};

struct _CamelStreamFilterPrivate {

	CamelStream *source;

	struct _filter *filters;
	gint filterid;		/* next filter id */

	gchar *realbuffer;	/* buffer - READ_PAD */
	gchar *buffer;		/* READ_SIZE bytes */

	gchar *filtered;		/* the filtered data */
	gsize filteredlen;

	guint last_was_read:1;	/* was the last op read or write? */
	guint flushed:1;        /* were the filters flushed? */
};

#define READ_PAD (128)		/* bytes padded before buffer */
#define READ_SIZE (4096)

G_DEFINE_TYPE (CamelStreamFilter, camel_stream_filter, CAMEL_TYPE_STREAM)

static void
stream_filter_finalize (GObject *object)
{
	CamelStreamFilter *stream = CAMEL_STREAM_FILTER (object);
	struct _filter *fn, *f;

	f = stream->priv->filters;
	while (f) {
		fn = f->next;
		g_object_unref (f->filter);
		g_free (f);
		f = fn;
	}

	g_free (stream->priv->realbuffer);
	g_object_unref (stream->priv->source);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (camel_stream_filter_parent_class)->finalize (object);
}

static gssize
stream_filter_read (CamelStream *stream,
                    gchar *buffer,
                    gsize n,
                    GError **error)
{
	CamelStreamFilterPrivate *priv;
	gssize size;
	struct _filter *f;

	priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	priv->last_was_read = TRUE;

	g_check(priv->realbuffer);

	if (priv->filteredlen<=0) {
		gsize presize = READ_PAD;

		size = camel_stream_read (
			priv->source, priv->buffer, READ_SIZE, error);
		if (size <= 0) {
			/* this is somewhat untested */
			if (camel_stream_eos(priv->source)) {
				f = priv->filters;
				priv->filtered = priv->buffer;
				priv->filteredlen = 0;
				while (f) {
					camel_mime_filter_complete (
						f->filter, priv->filtered, priv->filteredlen,
						presize, &priv->filtered, &priv->filteredlen, &presize);
					g_check(priv->realbuffer);
					f = f->next;
				}
				size = priv->filteredlen;
				priv->flushed = TRUE;
			}
			if (size <= 0)
				return size;
		} else {
			f = priv->filters;
			priv->filtered = priv->buffer;
			priv->filteredlen = size;

			d(printf ("\n\nOriginal content (%s): '", ((CamelObject *)priv->source)->class->name));
			d(fwrite(priv->filtered, sizeof(gchar), priv->filteredlen, stdout));
			d(printf("'\n"));

			while (f) {
				camel_mime_filter_filter (
					f->filter, priv->filtered, priv->filteredlen, presize,
					&priv->filtered, &priv->filteredlen, &presize);
				g_check(priv->realbuffer);

				d(printf ("Filtered content (%s): '", ((CamelObject *)f->filter)->class->name));
				d(fwrite(priv->filtered, sizeof(gchar), priv->filteredlen, stdout));
				d(printf("'\n"));

				f = f->next;
			}
		}
	}

	size = MIN(n, priv->filteredlen);
	memcpy(buffer, priv->filtered, size);
	priv->filteredlen -= size;
	priv->filtered += size;

	g_check(priv->realbuffer);

	return size;
}

/* Note: Since the caller expects to write out as much as they asked us to
   write (for 'success'), we return what they asked us to write (for 'success')
   rather than the true number of written bytes */
static gssize
stream_filter_write (CamelStream *stream,
                     const gchar *buf,
                     gsize n,
                     GError **error)
{
	CamelStreamFilterPrivate *priv;
	struct _filter *f;
	gsize presize, len, left = n;
	gchar *buffer, realbuffer[READ_SIZE+READ_PAD];

	priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	priv->last_was_read = FALSE;

	d(printf ("\n\nWriting: Original content (%s): '", ((CamelObject *)priv->source)->class->name));
	d(fwrite(buf, sizeof(gchar), n, stdout));
	d(printf("'\n"));

	g_check(priv->realbuffer);

	while (left) {
		/* Sigh, since filters expect non const args, copy the input first, we do this in handy sized chunks */
		len = MIN(READ_SIZE, left);
		buffer = realbuffer + READ_PAD;
		memcpy(buffer, buf, len);
		buf += len;
		left -= len;

		f = priv->filters;
		presize = READ_PAD;
		while (f) {
			camel_mime_filter_filter(f->filter, buffer, len, presize, &buffer, &len, &presize);

			g_check(priv->realbuffer);

			d(printf ("Filtered content (%s): '", ((CamelObject *)f->filter)->class->name));
			d(fwrite(buffer, sizeof(gchar), len, stdout));
			d(printf("'\n"));

			f = f->next;
		}

		if (camel_stream_write (priv->source, buffer, len, error) != len)
			return -1;
	}

	g_check(priv->realbuffer);

	return n;
}

static gint
stream_filter_flush (CamelStream *stream,
                     GError **error)
{
	CamelStreamFilterPrivate *priv;
	struct _filter *f;
	gchar *buffer;
	gsize presize;
	gsize len;

	priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	if (priv->last_was_read)
		return 0;

	buffer = (gchar *) "";
	len = 0;
	presize = 0;
	f = priv->filters;

	d(printf ("\n\nFlushing: Original content (%s): '", ((CamelObject *)priv->source)->class->name));
	d(fwrite(buffer, sizeof(gchar), len, stdout));
	d(printf("'\n"));

	while (f) {
		camel_mime_filter_complete(f->filter, buffer, len, presize, &buffer, &len, &presize);

		d(printf ("Filtered content (%s): '", ((CamelObject *)f->filter)->class->name));
		d(fwrite(buffer, sizeof(gchar), len, stdout));
		d(printf("'\n"));

		f = f->next;
	}

	if (len > 0 && camel_stream_write (priv->source, buffer, len, error) == -1)
		return -1;

	return camel_stream_flush (priv->source, error);
}

static gint
stream_filter_close (CamelStream *stream,
                     GError **error)
{
	CamelStreamFilterPrivate *priv;

	priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	/* Ignore errors while flushing. */
	if (!priv->last_was_read)
		stream_filter_flush (stream, NULL);

	return camel_stream_close (priv->source, error);
}

static gboolean
stream_filter_eos (CamelStream *stream)
{
	CamelStreamFilterPrivate *priv;

	priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	if (priv->filteredlen > 0)
		return FALSE;

	if (!priv->flushed)
		return FALSE;

	return camel_stream_eos (priv->source);
}

static gint
stream_filter_reset (CamelStream *stream,
                     GError **error)
{
	CamelStreamFilterPrivate *priv;
	struct _filter *f;

	priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	priv->filteredlen = 0;
	priv->flushed = FALSE;

	/* and reset filters */
	f = priv->filters;
	while (f) {
		camel_mime_filter_reset (f->filter);
		f = f->next;
	}

	return camel_stream_reset (priv->source, error);
}

static void
camel_stream_filter_class_init (CamelStreamFilterClass *class)
{
	GObjectClass *object_class;
	CamelStreamClass *stream_class;

	g_type_class_add_private (class, sizeof (CamelStreamFilterPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = stream_filter_finalize;

	stream_class = CAMEL_STREAM_CLASS (class);
	stream_class->read = stream_filter_read;
	stream_class->write = stream_filter_write;
	stream_class->flush = stream_filter_flush;
	stream_class->close = stream_filter_close;
	stream_class->eos = stream_filter_eos;
	stream_class->reset = stream_filter_reset;
}

static void
camel_stream_filter_init (CamelStreamFilter *stream)
{
	stream->priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	stream->priv->realbuffer = g_malloc(READ_SIZE + READ_PAD);
	stream->priv->buffer = stream->priv->realbuffer + READ_PAD;
	stream->priv->last_was_read = TRUE;
	stream->priv->flushed = FALSE;
}

/**
 * camel_stream_filter_new:
 *
 * Create a new #CamelStreamFilter object.
 *
 * Returns: a new #CamelStreamFilter object.
 *
 * Since: 2.32
 **/
CamelStream *
camel_stream_filter_new (CamelStream *source)
{
	CamelStream *stream;
	CamelStreamFilterPrivate *priv;

	g_return_val_if_fail (CAMEL_IS_STREAM (source), NULL);

	stream = g_object_new (CAMEL_TYPE_STREAM_FILTER, NULL);
	priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	priv->source = g_object_ref (source);

	return stream;
}

/**
 * camel_stream_filter_get_source:
 * @stream: a #CamelStreamFilter
 *
 * Since: 2.32
 **/
CamelStream *
camel_stream_filter_get_source (CamelStreamFilter *stream)
{
	g_return_val_if_fail (CAMEL_IS_STREAM_FILTER (stream), NULL);

	return stream->priv->source;
}

/**
 * camel_stream_filter_add:
 * @stream: a #CamelStreamFilter object
 * @filter: a #CamelMimeFilter object
 *
 * Add a new #CamelMimeFilter to execute during the processing of this
 * stream.  Each filter added is processed after the previous one.
 *
 * Note that a filter should only be added to a single stream
 * at a time, otherwise unpredictable results may occur.
 *
 * Returns: a filter id for the added @filter.
 **/
gint
camel_stream_filter_add (CamelStreamFilter *stream,
                         CamelMimeFilter *filter)
{
	CamelStreamFilterPrivate *priv;
	struct _filter *fn, *f;

	g_return_val_if_fail (CAMEL_IS_STREAM_FILTER (stream), -1);
	g_return_val_if_fail (CAMEL_IS_MIME_FILTER (filter), -1);

	priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	fn = g_malloc(sizeof(*fn));
	fn->id = priv->filterid++;
	fn->filter = g_object_ref (filter);

	/* sure, we could use a GList, but we wouldn't save much */
	f = (struct _filter *)&priv->filters;
	while (f->next)
		f = f->next;
	f->next = fn;
	fn->next = NULL;
	return fn->id;
}

/**
 * camel_stream_filter_remove:
 * @stream: a #CamelStreamFilter object
 * @id: Filter id, as returned from #camel_stream_filter_add
 *
 * Remove a processing filter from the stream by id.
 **/
void
camel_stream_filter_remove (CamelStreamFilter *stream,
                            gint id)
{
	CamelStreamFilterPrivate *priv;
	struct _filter *fn, *f;

	g_return_if_fail (CAMEL_IS_STREAM_FILTER (stream));

	priv = CAMEL_STREAM_FILTER_GET_PRIVATE (stream);

	f = (struct _filter *)&priv->filters;
	while (f && f->next) {
		fn = f->next;
		if (fn->id == id) {
			f->next = fn->next;
			g_object_unref (fn->filter);
			g_free(fn);
		}
		f = f->next;
	}
}

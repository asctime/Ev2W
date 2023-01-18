/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 * Author:
 *  Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include "camel-nntp-stream.h"

#define dd(x) (camel_debug ("nntp:stream")?(x):0)

#ifndef ECONNRESET
#define ECONNRESET EIO
#endif

#define CAMEL_NNTP_STREAM_SIZE (4096)
#define CAMEL_NNTP_STREAM_LINE_SIZE (1024) /* maximum line size */

G_DEFINE_TYPE (CamelNNTPStream, camel_nntp_stream, CAMEL_TYPE_STREAM)

static void
nntp_stream_dispose (GObject *object)
{
	CamelNNTPStream *stream = CAMEL_NNTP_STREAM (object);

	if (stream->source != NULL) {
		g_object_unref (stream->source);
		stream->source = NULL;
	}

	/* Chain up to parent's dispose () method. */
	G_OBJECT_CLASS (camel_nntp_stream_parent_class)->dispose (object);
}

static void
nntp_stream_finalize (GObject *object)
{
	CamelNNTPStream *stream = CAMEL_NNTP_STREAM (object);

	g_free (stream->buf);
	g_free (stream->linebuf);

	/* Chain up to parent's finalize () method. */
	G_OBJECT_CLASS (camel_nntp_stream_parent_class)->finalize (object);
}

static gint
nntp_stream_fill (CamelNNTPStream *is,
                  GError **error)
{
	gint left = 0;

	if (is->source) {
		left = is->end - is->ptr;
		memcpy (is->buf, is->ptr, left);
		is->end = is->buf + left;
		is->ptr = is->buf;
		left = camel_stream_read (
			is->source, (gchar *) is->end,
			CAMEL_NNTP_STREAM_SIZE - (is->end - is->buf), error);
		if (left > 0) {
			is->end += left;
			is->end[0] = '\n';
			return is->end - is->ptr;
		} else {
			if (left == 0) {
				errno = ECONNRESET;
				g_set_error (
					error, G_IO_ERROR,
					g_io_error_from_errno (errno),
					"%s", g_strerror (errno));
			}
			dd (printf ("NNTP_STREAM_FILL (ERROR): %d - '%s'\n", left, g_strerror (errno)));
			return -1;
		}
	}

	return 0;
}

static gssize
nntp_stream_read (CamelStream *stream,
                  gchar *buffer,
                  gsize n,
                  GError **error)
{
	CamelNNTPStream *is = (CamelNNTPStream *)stream;
	gchar *o, *oe;
	guchar *p, *e, c;
	gint state;

	if (is->mode != CAMEL_NNTP_STREAM_DATA || n == 0)
		return 0;

	o = buffer;
	oe = buffer + n;
	state = is->state;

	/* Need to copy/strip '.'s and whatnot */
	p = is->ptr;
	e = is->end;

	switch (state) {
	state_0:
	case 0:		/* start of line, always read at least 3 chars */
		while (e - p < 3) {
			is->ptr = p;
			if (nntp_stream_fill (is, error) == -1)
				return -1;
			p = is->ptr;
			e = is->end;
		}
		if (p[0] == '.') {
			if (p[1] == '\r' && p[2] == '\n') {
				is->ptr = p+3;
				is->mode = CAMEL_NNTP_STREAM_EOD;
				is->state = 0;
				dd (printf ("NNTP_STREAM_READ (%d):\n%.*s\n", (gint)(o-buffer), (gint)(o-buffer), buffer));
				return o-buffer;
			}
			p++;
		}
		state = 1;
		/* FALLS THROUGH */
	case 1:		/* looking for next sol */
		while (o < oe) {
			c = *p++;
			if (c == '\n') {
				/* end of input sentinal check */
				if (p > e) {
					is->ptr = e;
					if (nntp_stream_fill (is, error) == -1)
						return -1;
					p = is->ptr;
					e = is->end;
				} else {
					*o++ = '\n';
					state = 0;
					goto state_0;
				}
			} else if (c != '\r') {
				*o++ = c;
			}
		}
		break;
	}

	is->ptr = p;
	is->state = state;

	dd (printf ("NNTP_STREAM_READ (%d):\n%.*s\n", (gint)(o-buffer), (gint)(o-buffer), buffer));

	return o-buffer;
}

static gssize
nntp_stream_write (CamelStream *stream,
                   const gchar *buffer,
                   gsize n,
                   GError **error)
{
	CamelNNTPStream *is = (CamelNNTPStream *)stream;

	return camel_stream_write (is->source, buffer, n, error);
}

static gint
nntp_stream_close (CamelStream *stream,
                   GError **error)
{
	/* nop? */
	return 0;
}

static gint
nntp_stream_flush (CamelStream *stream,
                   GError **error)
{
	/* nop? */
	return 0;
}

static gboolean
nntp_stream_eos (CamelStream *stream)
{
	CamelNNTPStream *is = (CamelNNTPStream *)stream;

	return is->mode != CAMEL_NNTP_STREAM_DATA;
}

static gint
nntp_stream_reset (CamelStream *stream,
                   GError **error)
{
	/* nop?  reset literal mode? */
	return 0;
}

static void
camel_nntp_stream_class_init (CamelNNTPStreamClass *class)
{
	GObjectClass *object_class;
	CamelStreamClass *stream_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = nntp_stream_dispose;
	object_class->finalize = nntp_stream_finalize;

	stream_class = CAMEL_STREAM_CLASS (class);
	stream_class->read = nntp_stream_read;
	stream_class->write = nntp_stream_write;
	stream_class->close = nntp_stream_close;
	stream_class->flush = nntp_stream_flush;
	stream_class->eos = nntp_stream_eos;
	stream_class->reset = nntp_stream_reset;
}

static void
camel_nntp_stream_init (CamelNNTPStream *is)
{
	/* +1 is room for appending a 0 if we need to for a line */
	is->ptr = is->end = is->buf = g_malloc (CAMEL_NNTP_STREAM_SIZE+1);
	is->lineptr = is->linebuf = g_malloc (CAMEL_NNTP_STREAM_LINE_SIZE+1);
	is->lineend = is->linebuf + CAMEL_NNTP_STREAM_LINE_SIZE;

	/* init sentinal */
	is->ptr[0] = '\n';

	is->state = 0;
	is->mode = CAMEL_NNTP_STREAM_LINE;
}

/**
 * camel_nntp_stream_new:
 *
 * Returns a NULL stream.  A null stream is always at eof, and
 * always returns success for all reads and writes.
 *
 * Returns: the stream
 **/
CamelStream *
camel_nntp_stream_new (CamelStream *source)
{
	CamelNNTPStream *is;

	is = g_object_new (CAMEL_TYPE_NNTP_STREAM, NULL);
	is->source = g_object_ref (source);

	return (CamelStream *)is;
}

/* Get one line from the nntp stream */
gint
camel_nntp_stream_line (CamelNNTPStream *is,
                        guchar **data,
                        guint *len,
                        GError **error)
{
	register guchar c, *p, *o, *oe;
	gint newlen, oldlen;
	guchar *e;

	if (is->mode == CAMEL_NNTP_STREAM_EOD) {
		*data = is->linebuf;
		*len = 0;
		return 0;
	}

	o = is->linebuf;
	oe = is->lineend - 1;
	p = is->ptr;
	e = is->end;

	/* Data mode, convert leading '..' to '.', and stop when we reach a solitary '.' */
	if (is->mode == CAMEL_NNTP_STREAM_DATA) {
		/* need at least 3 chars in buffer */
		while (e-p < 3) {
			is->ptr = p;
			if (nntp_stream_fill (is, error) == -1)
				return -1;
			p = is->ptr;
			e = is->end;
		}

		/* check for isolated '.\r\n' or begging of line '.' */
		if (p[0] == '.') {
			if (p[1] == '\r' && p[2] == '\n') {
				is->ptr = p+3;
				is->mode = CAMEL_NNTP_STREAM_EOD;
				*data = is->linebuf;
				*len = 0;
				is->linebuf[0] = 0;

				dd (printf ("NNTP_STREAM_LINE (END)\n"));

				return 0;
			}
			p++;
		}
	}

	while (1) {
		while (o < oe) {
			c = *p++;
			if (c == '\n') {
				/* sentinal? */
				if (p> e) {
					is->ptr = e;
					if (nntp_stream_fill (is, error) == -1)
						return -1;
					p = is->ptr;
					e = is->end;
				} else {
					is->ptr = p;
					*data = is->linebuf;
					*len = o - is->linebuf;
					*o = 0;

					dd (printf ("NNTP_STREAM_LINE (%d): '%s'\n", *len, *data));

					return 1;
				}
			} else if (c != '\r') {
				*o++ = c;
			}
		}

		/* limit this for bad server data? */
		oldlen = o - is->linebuf;
		newlen = (is->lineend - is->linebuf) * 3 / 2;
		is->lineptr = is->linebuf = g_realloc (is->linebuf, newlen);
		is->lineend = is->linebuf + newlen;
		oe = is->lineend - 1;
		o = is->linebuf + oldlen;
	}
}

/* returns -1 on error, 0 if last lot of data, >0 if more remaining */
gint
camel_nntp_stream_gets (CamelNNTPStream *is,
                        guchar **start,
                        guint *len)
{
	gint max;
	guchar *end;

	*len = 0;

	max = is->end - is->ptr;
	if (max == 0) {
		max = nntp_stream_fill (is, NULL);
		if (max <= 0)
			return max;
	}

	*start = is->ptr;
	end = memchr (is->ptr, '\n', max);
	if (end)
		max = (end - is->ptr) + 1;
	*start = is->ptr;
	*len = max;
	is->ptr += max;

	dd (printf ("NNTP_STREAM_GETS (%s,%d): '%.*s'\n", end==NULL?"more":"last", *len, (gint)*len, *start));

	return end == NULL?1:0;
}

void
camel_nntp_stream_set_mode (CamelNNTPStream *is,
                            camel_nntp_stream_mode_t mode)
{
	is->mode = mode;
}

/* returns -1 on erorr, 0 if last data, >0 if more data left */
gint
camel_nntp_stream_getd (CamelNNTPStream *is,
                        guchar **start,
                        guint *len)
{
	guchar *p, *e, *s;
	gint state;

	*len = 0;

	if (is->mode == CAMEL_NNTP_STREAM_EOD)
		return 0;

	if (is->mode == CAMEL_NNTP_STREAM_LINE) {
		g_warning ("nntp_stream reading data in line mode\n");
		return 0;
	}

	state = is->state;
	p = is->ptr;
	e = is->end;

	while (e - p < 3) {
		is->ptr = p;
		if (nntp_stream_fill (is, NULL) == -1)
			return -1;
		p = is->ptr;
		e = is->end;
	}

	s = p;

	do {
		switch (state) {
		case 0:
			/* check leading '.', ... */
			if (p[0] == '.') {
				if (p[1] == '\r' && p[2] == '\n') {
					is->ptr = p+3;
					*len = p-s;
					*start = s;
					is->mode = CAMEL_NNTP_STREAM_EOD;
					is->state = 0;

					dd (printf ("NNTP_STREAM_GETD (%s,%d): '%.*s'\n", "last", *len, (gint)*len, *start));

					return 0;
				}

				/* If at start, just skip '.', else return data upto '.' but skip it */
				if (p == s) {
					s++;
					p++;
				} else {
					is->ptr = p+1;
					*len = p-s;
					*start = s;
					is->state = 1;

					dd (printf ("NNTP_STREAM_GETD (%s,%d): '%.*s'\n", "more", *len, (gint)*len, *start));

					return 1;
				}
			}
			state = 1;
		case 1:
			/* Scan for sentinal */
			while ((*p++)!='\n')
				;

			if (p > e) {
				p = e;
			} else {
				state = 0;
			}
			break;
		}
	} while ((e-p) >= 3);

	is->state = state;
	is->ptr = p;
	*len = p-s;
	*start = s;

	dd (printf ("NNTP_STREAM_GETD (%s,%d): '%.*s'\n", "more", *len, (gint)*len, *start));
	return 1;
}


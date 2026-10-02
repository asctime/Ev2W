/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Jeffrey Stedfast <fejj@ximian.com>
 *   Dan Winship <danw@ximian.com>
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
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gi18n-lib.h>

#include "camel-file-utils.h"
#include "camel-object.h"
#include "camel-operation.h"
#include "camel-url.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#ifndef __MINGW32__
#define EWOULDBLOCK EAGAIN
#endif
#endif

#define IO_TIMEOUT (60*4)

#ifdef G_OS_WIN32
/* Winsock recv/send take a 32-bit length. Clamp so a gsize cannot wrap. */
static int
camel_io_chunk (gsize n)
{
	if (n > (gsize) INT_MAX)
		return INT_MAX;
	return (int) n;
}

static int
camel_winsock_errno (int wsa_err)
{
	switch (wsa_err) {
	case 0:			return 0;
	case WSAEINTR:		return EINTR;
	case WSAEBADF:		return EBADF;
	case WSAEACCES:		return EACCES;
	case WSAEFAULT:		return EFAULT;
	case WSAEINVAL:		return EINVAL;
	case WSAEMFILE:		return EMFILE;
	case WSAEWOULDBLOCK:	return EWOULDBLOCK;
	case WSAEINPROGRESS:	return EINPROGRESS;
	case WSAEALREADY:	return EALREADY;
	case WSAENOTSOCK:	return ENOTSOCK;
	case WSAEMSGSIZE:	return EMSGSIZE;
	case WSAEPROTONOSUPPORT:return EPROTONOSUPPORT;
	case WSAEOPNOTSUPP:	return EOPNOTSUPP;
	case WSAEAFNOSUPPORT:	return EAFNOSUPPORT;
	case WSAEADDRINUSE:	return EADDRINUSE;
	case WSAEADDRNOTAVAIL:	return EADDRNOTAVAIL;
	case WSAENETDOWN:	return ENETDOWN;
	case WSAENETUNREACH:	return ENETUNREACH;
	case WSAENETRESET:	return ENETRESET;
	case WSAECONNABORTED:	return ECONNABORTED;
	case WSAECONNRESET:	return ECONNRESET;
	case WSAENOBUFS:	return ENOBUFS;
	case WSAEISCONN:	return EISCONN;
	case WSAENOTCONN:	return ENOTCONN;
	case WSAESHUTDOWN:	return EPIPE;
	case WSAETIMEDOUT:	return ETIMEDOUT;
	case WSAECONNREFUSED:	return ECONNREFUSED;
	case WSAEHOSTUNREACH:	return EHOSTUNREACH;
	default:		return EIO;
	}
}

static void
camel_set_errno_from_winsock (void)
{
	errno = camel_winsock_errno (WSAGetLastError ());
}

/* Wait until a non-blocking socket can transfer. select() sleeps;
 * spinning on WSAEWOULDBLOCK pegs a core on MinGW64. */
static int
camel_sock_wait (SOCKET sock,
                 gboolean for_write)
{
	fd_set set;
	int res;

	FD_ZERO (&set);
	FD_SET (sock, &set);
	if (for_write)
		res = select (0, NULL, &set, NULL, NULL);
	else
		res = select (0, &set, NULL, NULL, NULL);
	if (res == SOCKET_ERROR) {
		camel_set_errno_from_winsock ();
		return -1;
	}
	if (res == 0) {
		errno = ETIMEDOUT;
		return -1;
	}
	return 0;
}
#endif

/**
 * camel_file_util_encode_uint32:
 * @out: file to output to
 * @value: value to output
 *
 * Utility function to save an uint32 to a file.
 *
 * Returns: %0 on success, %-1 on error.
 **/
gint
camel_file_util_encode_uint32 (FILE *out, guint32 value)
{
	gint i;

	for (i = 28; i > 0; i -= 7) {
		if (value >= (1 << i)) {
			guint c = (value >> i) & 0x7f;
			if (fputc (c, out) == -1)
				return -1;
		}
	}
	return fputc (value | 0x80, out);
}

/**
 * camel_file_util_decode_uint32:
 * @in: file to read from
 * @dest: pointer to a variable to store the value in
 *
 * Retrieve an encoded uint32 from a file.
 *
 * Returns: %0 on success, %-1 on error.  @*dest will contain the
 * decoded value.
 **/
gint
camel_file_util_decode_uint32 (FILE *in, guint32 *dest)
{
        guint32 value = 0;
	gint v;

        /* until we get the last byte, keep decoding 7 bits at a time */
        while ( ((v = fgetc (in)) & 0x80) == 0 && v!=EOF) {
                value |= v;
                value <<= 7;
        }
	if (v == EOF) {
		*dest = value >> 7;
		return -1;
	}
	*dest = value | (v & 0x7f);

        return 0;
}

/**
 * camel_file_util_encode_fixed_int32:
 * @out: file to output to
 * @value: value to output
 *
 * Encode a gint32, performing no compression, but converting
 * to network order.
 *
 * Returns: %0 on success, %-1 on error.
 **/
gint
camel_file_util_encode_fixed_int32 (FILE *out, gint32 value)
{
	guint32 save;

	save = g_htonl (value);
	if (fwrite (&save, sizeof (save), 1, out) != 1)
		return -1;
	return 0;
}

/**
 * camel_file_util_decode_fixed_int32:
 * @in: file to read from
 * @dest: pointer to a variable to store the value in
 *
 * Retrieve a gint32.
 *
 * Returns: %0 on success, %-1 on error.
 **/
gint
camel_file_util_decode_fixed_int32 (FILE *in, gint32 *dest)
{
	guint32 save;

	if (fread (&save, sizeof (save), 1, in) == 1) {
		*dest = g_ntohl (save);
		return 0;
	} else {
		return -1;
	}
}

/* Shift through guint64. time_t and off_t are signed 64-bit on MinGW64,
 * and a signed left shift into the sign bit is undefined. */
#define CFU_ENCODE_T(type)						\
gint									\
camel_file_util_encode_##type(FILE *out, type value)			\
{									\
	guint64 u = (guint64) value;					\
	gint i;								\
									\
	for (i = (gint) sizeof (type) - 1; i >= 0; i--) {		\
		if (fputc ((int) ((u >> (i * 8)) & 0xff), out) == -1)	\
			return -1;					\
	}								\
	return 0;							\
}

#define CFU_DECODE_T(type)						\
gint									\
camel_file_util_decode_##type(FILE *in, type *dest)			\
{									\
	guint64 save = 0;						\
	gint i = (gint) sizeof (type) - 1;				\
	gint v = EOF;							\
									\
        while (i >= 0 && (v = fgetc (in)) != EOF) {			\
		save |= ((guint64) (v & 0xff)) << (i * 8);		\
		i--;							\
	}								\
	*dest = (type) save;						\
	if (v == EOF)							\
		return -1;						\
	return 0;							\
}

/**
 * camel_file_util_encode_time_t:
 * @out: file to output to
 * @value: value to output
 *
 * Encode a time_t value to the file.
 *
 * Returns: %0 on success, %-1 on error.
 **/
CFU_ENCODE_T(time_t)

/**
 * camel_file_util_decode_time_t:
 * @in: file to read from
 * @dest: pointer to a variable to store the value in
 *
 * Decode a time_t value.
 *
 * Returns: %0 on success, %-1 on error.
 **/
CFU_DECODE_T(time_t)

/**
 * camel_file_util_encode_off_t:
 * @out: file to output to
 * @value: value to output
 *
 * Encode an off_t type.
 *
 * Returns: %0 on success, %-1 on error.
 **/
CFU_ENCODE_T(off_t)

/**
 * camel_file_util_decode_off_t:
 * @in: file to read from
 * @dest: pointer to a variable to put the value in
 *
 * Decode an off_t type.
 *
 * Returns: %0 on success, %-1 on failure.
 **/
CFU_DECODE_T(off_t)

/**
 * camel_file_util_encode_gsize:
 * @out: file to output to
 * @value: value to output
 *
 * Encode an gsize type.
 *
 * Returns: %0 on success, %-1 on error.
 **/
CFU_ENCODE_T(gsize)

/**
 * camel_file_util_decode_gsize:
 * @in: file to read from
 * @dest: pointer to a variable to put the value in
 *
 * Decode an gsize type.
 *
 * Returns: %0 on success, %-1 on failure.
 **/
CFU_DECODE_T(gsize)

/**
 * camel_file_util_encode_string:
 * @out: file to output to
 * @str: value to output
 *
 * Encode a normal string and save it in the output file.
 *
 * Returns: %0 on success, %-1 on error.
 **/
gint
camel_file_util_encode_string (FILE *out, const gchar *str)
{
	register gint len;

	if (str == NULL)
		return camel_file_util_encode_uint32 (out, 1);

	if ((len = strlen (str)) > 65536)
		len = 65536;

	if (camel_file_util_encode_uint32 (out, len+1) == -1)
		return -1;
	if (len == 0 || fwrite (str, len, 1, out) == 1)
		return 0;
	return -1;
}

/**
 * camel_file_util_decode_string:
 * @in: file to read from
 * @str: pointer to a variable to store the value in
 *
 * Decode a normal string from the input file.
 *
 * Returns: %0 on success, %-1 on error.
 **/
gint
camel_file_util_decode_string (FILE *in, gchar **str)
{
	guint32 len;
	register gchar *ret;

	if (camel_file_util_decode_uint32 (in, &len) == -1) {
		*str = NULL;
		return -1;
	}

	len--;
	if (len > 65536) {
		*str = NULL;
		return -1;
	}

	ret = g_malloc (len+1);
	if (len > 0 && fread (ret, len, 1, in) != 1) {
		g_free (ret);
		*str = NULL;
		return -1;
	}

	ret[len] = 0;
	*str = ret;
	return 0;
}

/**
 * camel_file_util_encode_fixed_string:
 * @out: file to output to
 * @str: value to output
 * @len: total-len of str to store
 *
 * Encode a normal string and save it in the output file.
 * Unlike @camel_file_util_encode_string, it pads the
 * @str with "NULL" bytes, if @len is > strlen(str)
 *
 * Returns: %0 on success, %-1 on error.
 **/
gint
camel_file_util_encode_fixed_string (FILE *out, const gchar *str, gsize len)
{
	gchar buf[len];

	/* Don't allow empty strings to be written */
	if (len < 1)
		return -1;

	/* Max size is 64K */
	if (len > 65536)
		len = 65536;

	memset(buf, 0x00, len);
	g_strlcpy(buf, str, len);

	if (fwrite (buf, len, 1, out) == len)
		return 0;

	return -1;
}

/**
 * camel_file_util_decode_fixed_string:
 * @in: file to read from
 * @str: pointer to a variable to store the value in
 * @len: total-len to decode.
 *
 * Decode a normal string from the input file.
 *
 * Returns: %0 on success, %-1 on error.
 **/
gint
camel_file_util_decode_fixed_string (FILE *in, gchar **str, gsize len)
{
	register gchar *ret;

	if (len > 65536) {
		*str = NULL;
		return -1;
	}

	ret = g_malloc (len+1);
	if (len > 0 && fread (ret, len, 1, in) != 1) {
		g_free (ret);
		*str = NULL;
		return -1;
	}

	ret[len] = 0;
	*str = ret;
	return 0;
}

/**
 * camel_file_util_safe_filename:
 * @name: string to 'flattened' into a safe filename
 *
 * 'Flattens' @name into a safe filename string by hex encoding any
 * chars that may cause problems on the filesystem.
 *
 * Returns: a safe filename string.
 **/
gchar *
camel_file_util_safe_filename (const gchar *name)
{
#ifdef G_OS_WIN32
	const gchar *unsafe_chars = "/?()'*<>:\"\\|";
#else
	const gchar *unsafe_chars = "/?()'*";
#endif

	if (name == NULL)
		return NULL;

	return camel_url_encode(name, unsafe_chars);
}

/* FIXME: poll() might be more efficient and more portable? */

/**
 * camel_read:
 * @fd: file descriptor
 * @buf: buffer to fill
 * @n: number of bytes to read into @buf
 * @error: return location for a #GError, or %NULL
 *
 * Cancellable libc read() replacement.
 *
 * Code that intends to be portable to Win32 should call this function
 * only on file descriptors returned from open(), not on sockets.
 *
 * Returns: number of bytes read or -1 on fail. On failure, errno will
 * be set appropriately.
 **/
gssize
camel_read (gint fd,
            gchar *buf,
            gsize n,
            GError **error)
{
	gssize nread;
	gint cancel_fd;

	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		g_set_error (
			error, G_IO_ERROR,
			G_IO_ERROR_CANCELLED,
			_("Cancelled"));
		return -1;
	}

#ifndef G_OS_WIN32
	cancel_fd = camel_operation_cancel_fd (NULL);
#else
	cancel_fd = -1;
#endif
	if (cancel_fd == -1) {
		do {
#ifdef G_OS_WIN32
			/* CRT read() takes an unsigned int, not a gsize. */
			nread = read (fd, buf, (unsigned int) camel_io_chunk (n));
#else
			nread = read (fd, buf, n);
#endif
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
	} else {
#ifndef G_OS_WIN32
		gint errnosav, flags, fdmax;
		fd_set rdset;

		flags = fcntl (fd, F_GETFL);
		fcntl (fd, F_SETFL, flags | O_NONBLOCK);

		do {
			struct timeval tv;
			gint res;

			FD_ZERO (&rdset);
			FD_SET (fd, &rdset);
			FD_SET (cancel_fd, &rdset);
			fdmax = MAX (fd, cancel_fd) + 1;
			tv.tv_sec = IO_TIMEOUT;
			tv.tv_usec = 0;
			nread = -1;

			res = select(fdmax, &rdset, 0, 0, &tv);
			if (res == -1)
				;
			else if (res == 0)
				errno = ETIMEDOUT;
			else if (FD_ISSET (cancel_fd, &rdset)) {
				errno = EINTR;
				goto failed;
			} else {
				do {
					nread = read (fd, buf, n);
				} while (nread == -1 && errno == EINTR);
			}
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
	failed:
		errnosav = errno;
		fcntl (fd, F_SETFL, flags);
		errno = errnosav;
#endif
	}

	if (nread == -1) {
		if (errno == EINTR)
			g_set_error (
				error, G_IO_ERROR,
				G_IO_ERROR_CANCELLED,
				_("Cancelled"));
		else
			g_set_error (
				error, G_IO_ERROR,
				g_io_error_from_errno (errno),
				"%s", g_strerror (errno));
	}

	return nread;
}

/**
 * camel_write:
 * @fd: file descriptor
 * @buf: buffer to write
 * @n: number of bytes of @buf to write
 * @error: return location for a #GError, or %NULL
 *
 * Cancellable libc write() replacement.
 *
 * Code that intends to be portable to Win32 should call this function
 * only on file descriptors returned from open(), not on sockets.
 *
 * Returns: number of bytes written or -1 on fail. On failure, errno will
 * be set appropriately.
 **/
gssize
camel_write (gint fd,
             const gchar *buf,
             gsize n,
             GError **error)
{
	gssize w, written = 0;
	gint cancel_fd;

	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		g_set_error (
			error, G_IO_ERROR,
			G_IO_ERROR_CANCELLED,
			_("Cancelled"));
		return -1;
	}

#ifndef G_OS_WIN32
	cancel_fd = camel_operation_cancel_fd (NULL);
#else
	cancel_fd = -1;
#endif
	if (cancel_fd == -1) {
		do {
			do {
#ifdef G_OS_WIN32
				w = write (fd, buf + written,
					(unsigned int) camel_io_chunk (n - written));
#else
				w = write (fd, buf + written, n - written);
#endif
			} while (w == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
			if (w > 0)
				written += w;
		} while (w > 0 && written < n);
	} else {
#ifndef G_OS_WIN32
		gint errnosav, flags, fdmax;
		fd_set rdset, wrset;

		flags = fcntl (fd, F_GETFL);
		fcntl (fd, F_SETFL, flags | O_NONBLOCK);

		fdmax = MAX (fd, cancel_fd) + 1;
		do {
			struct timeval tv;
			gint res;

			FD_ZERO (&rdset);
			FD_ZERO (&wrset);
			FD_SET (fd, &wrset);
			FD_SET (cancel_fd, &rdset);
			tv.tv_sec = IO_TIMEOUT;
			tv.tv_usec = 0;
			w = -1;

			res = select (fdmax, &rdset, &wrset, 0, &tv);
			if (res == -1) {
				if (errno == EINTR)
					w = 0;
			} else if (res == 0)
				errno = ETIMEDOUT;
			else if (FD_ISSET (cancel_fd, &rdset))
				errno = EINTR;
			else {
				do {
					w = write (fd, buf + written, n - written);
				} while (w == -1 && errno == EINTR);

				if (w == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						w = 0;
				} else
					written += w;
			}
		} while (w != -1 && written < n);

		errnosav = errno;
		fcntl (fd, F_SETFL, flags);
		errno = errnosav;
#endif
	}

	if (w == -1) {
		if (errno == EINTR)
			g_set_error (
				error, G_IO_ERROR,
				G_IO_ERROR_CANCELLED,
				_("Cancelled"));
		else
			g_set_error (
				error, G_IO_ERROR,
				g_io_error_from_errno (errno),
				"%s", g_strerror (errno));
		return -1;
	}

	return written;
}

/**
 * camel_read_socket:
 * @fd: a socket
 * @buf: buffer to fill
 * @n: number of bytes to read into @buf
 * @error: return location for a #GError, or %NULL
 *
 * Cancellable read() replacement for sockets. Code that intends to be
 * portable to Win32 should call this function only on sockets
 * returned from socket(), or accept().
 *
 * Returns: number of bytes read or -1 on fail. On failure, errno will
 * be set appropriately. If the socket is nonblocking
 * camel_read_socket() will retry the read until it gets something.
 **/
gssize
camel_read_socket (gint fd,
                   gchar *buf,
                   gsize n,
                   GError **error)
{
#ifndef G_OS_WIN32
	return camel_read (fd, buf, n, error);
#else
	gssize nread;
	gint cancel_fd;

	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		g_set_error (
			error, G_IO_ERROR,
			G_IO_ERROR_CANCELLED,
			_("Canceled"));
		return -1;
	}
	cancel_fd = camel_operation_cancel_fd (NULL);

	if (cancel_fd == -1) {
		for (;;) {
			nread = recv ((SOCKET) fd, buf, camel_io_chunk (n), 0);
			if (nread != SOCKET_ERROR)
				break;
			if (WSAGetLastError () != WSAEWOULDBLOCK) {
				camel_set_errno_from_winsock ();
				nread = -1;
				break;
			}
			if (camel_sock_wait ((SOCKET) fd, FALSE) != 0) {
				nread = -1;
				break;
			}
		}
	} else {
		gint fdmax;
		fd_set rdset;
		u_long yes = 1;
		int chunk = camel_io_chunk (n);

		ioctlsocket ((SOCKET) fd, FIONBIO, &yes);
		fdmax = MAX (fd, cancel_fd) + 1;
		for (;;) {
			struct timeval tv;
			gint res;

			FD_ZERO (&rdset);
			FD_SET ((SOCKET) fd, &rdset);
			FD_SET ((SOCKET) cancel_fd, &rdset);
			tv.tv_sec = IO_TIMEOUT;
			tv.tv_usec = 0;
			nread = -1;

			res = select (fdmax, &rdset, 0, 0, &tv);
			if (res == SOCKET_ERROR) {
				camel_set_errno_from_winsock ();
				break;
			} else if (res == 0) {
				continue;
			} else if (FD_ISSET ((SOCKET) cancel_fd, &rdset)) {
				errno = EINTR;
				break;
			} else {
				nread = recv ((SOCKET) fd, buf, chunk, 0);
				if (nread == SOCKET_ERROR) {
					if (WSAGetLastError () == WSAEWOULDBLOCK)
						continue;
					camel_set_errno_from_winsock ();
					nread = -1;
				}
				break;
			}
		}
	}

	if (nread == -1) {
		if (errno == EINTR)
			g_set_error (
				error, G_IO_ERROR,
				G_IO_ERROR_CANCELLED,
				_("Cancelled"));
		else
			g_set_error (
				error, G_IO_ERROR,
				g_io_error_from_errno (errno),
				"%s", g_strerror (errno));
	}

	return nread;
#endif
}

/**
 * camel_write_socket:
 * @fd: file descriptor
 * @buf: buffer to write
 * @n: number of bytes of @buf to write
 * @error: return location for a #GError, or %NULL
 *
 * Cancellable write() replacement for sockets. Code that intends to
 * be portable to Win32 should call this function only on sockets
 * returned from socket() or accept().
 *
 * Returns: number of bytes written or -1 on fail. On failure, errno will
 * be set appropriately.
 **/
gssize
camel_write_socket (gint fd,
                    const gchar *buf,
                    gsize n,
                    GError **error)
{
#ifndef G_OS_WIN32
	return camel_write (fd, buf, n, error);
#else
	gssize w, written = 0;
	gint cancel_fd;

	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		g_set_error (
			error, G_IO_ERROR,
			G_IO_ERROR_CANCELLED,
			_("Canceled"));
		return -1;
	}

	cancel_fd = camel_operation_cancel_fd (NULL);
	if (cancel_fd == -1) {
		do {
			for (;;) {
				w = send ((SOCKET) fd, buf + written,
					camel_io_chunk (n - written), 0);
				if (w == SOCKET_ERROR && WSAGetLastError () == WSAEWOULDBLOCK) {
					if (camel_sock_wait ((SOCKET) fd, TRUE) != 0) {
						w = -1;
						break;
					}
					continue;
				}
				if (w == SOCKET_ERROR) {
					camel_set_errno_from_winsock ();
					w = -1;
				}
				break;
			}
			if (w > 0)
				written += w;
		} while (w > 0 && written < n);
	} else {
		gint fdmax;
		fd_set rdset, wrset;
		u_long arg = 1;

		ioctlsocket ((SOCKET) fd, FIONBIO, &arg);
		fdmax = MAX (fd, cancel_fd) + 1;
		do {
			struct timeval tv;
			gint res;

			FD_ZERO (&rdset);
			FD_ZERO (&wrset);
			FD_SET ((SOCKET) fd, &wrset);
			FD_SET ((SOCKET) cancel_fd, &rdset);
			tv.tv_sec = IO_TIMEOUT;
			tv.tv_usec = 0;
			w = -1;

			res = select (fdmax, &rdset, &wrset, 0, &tv);
			if (res == SOCKET_ERROR) {
				camel_set_errno_from_winsock ();
			} else if (res == 0) {
				/* Poll interval elapsed. w must stay non-error or the
				 * loop treats the timeout as a failed write. */
				w = 0;
				continue;
			} else if (FD_ISSET ((SOCKET) cancel_fd, &rdset)) {
				errno = EINTR;
			} else {
				w = send ((SOCKET) fd, buf + written,
					camel_io_chunk (n - written), 0);
				if (w == SOCKET_ERROR) {
					if (WSAGetLastError () == WSAEWOULDBLOCK) {
						w = 0;
					} else {
						camel_set_errno_from_winsock ();
						w = -1;
					}
				} else if (w > 0) {
					written += w;
				} else {
					break;
				}
			}
		} while (w != -1 && written < n);
		arg = 0;
		ioctlsocket ((SOCKET) fd, FIONBIO, &arg);
	}

	if (w == -1) {
		if (errno == EINTR)
			g_set_error (
				error, G_IO_ERROR,
				G_IO_ERROR_CANCELLED,
				_("Canceled"));
		else
			g_set_error (
				error, G_IO_ERROR,
				g_io_error_from_errno (errno),
				"%s", g_strerror (errno));
		return -1;
	}

	return written;
#endif
}

/**
 * camel_file_util_savename:
 * @filename: a pathname
 *
 * Builds a pathname where the basename is of the form ".#" + the
 * basename of @filename, for instance used in a two-stage commit file
 * write.
 *
 * Returns: The new pathname.  It must be free'd with g_free().
 **/
gchar *
camel_file_util_savename(const gchar *filename)
{
	gchar *dirname, *retval;

	dirname = g_path_get_dirname(filename);

	if (strcmp (dirname, ".") == 0) {
		retval = g_strconcat (".#", filename, NULL);
	} else {
		gchar *basename = g_path_get_basename(filename);
		gchar *newbasename = g_strconcat (".#", basename, NULL);

		retval = g_build_filename (dirname, newbasename, NULL);

		g_free (newbasename);
		g_free (basename);
	}
	g_free (dirname);

	return retval;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-debug-log.c: Ring buffer for logging debug messages
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Federico Mena-Quintero <federico@novell.com>
 */

#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "e-debug-log.h"

#define DEFAULT_RING_BUFFER_NUM_LINES 1000

#define KEY_FILE_GROUP		"debug log"
#define KEY_FILE_DOMAINS_KEY	"enable domains"
#define KEY_FILE_MAX_LINES_KEY	"max lines"

static GStaticMutex log_mutex = G_STATIC_MUTEX_INIT;

static GHashTable *domains_hash;
static gchar **ring_buffer;
static gint ring_buffer_next_index;
static gint ring_buffer_num_lines;
static gint ring_buffer_max_lines = DEFAULT_RING_BUFFER_NUM_LINES;

static GSList *milestones_head;
static GSList *milestones_tail;

static void
lock (void)
{
	g_static_mutex_lock (&log_mutex);
}

static void
unlock (void)
{
	g_static_mutex_unlock (&log_mutex);
}

/**
 * e_debug_log:
 *
 * Since: 2.32
 **/
void
e_debug_log (gboolean is_milestone, const gchar *domain, const gchar *format, ...)
{
	va_list args;

	va_start (args, format);
	e_debug_logv (is_milestone, domain, format, args);
	va_end (args);
}

static gboolean
is_domain_enabled (const gchar *domain)
{
	/* User actions are always logged */
	if (strcmp (domain, E_DEBUG_LOG_DOMAIN_USER) == 0)
		return TRUE;

	if (!domains_hash)
		return FALSE;

	return (g_hash_table_lookup (domains_hash, domain) != NULL);
}

static void
ensure_ring (void)
{
	if (ring_buffer)
		return;

	ring_buffer = g_new0 (gchar *, ring_buffer_max_lines);
	ring_buffer_next_index = 0;
	ring_buffer_num_lines = 0;
}

static void
add_to_ring (gchar *str)
{
	ensure_ring ();

	g_assert (str != NULL);

	if (ring_buffer_num_lines == ring_buffer_max_lines) {
		/* We have an overlap, and the ring_buffer_next_index pogints to
		 * the "first" item.  Free it to make room for the new item.
		 */

		g_assert (ring_buffer[ring_buffer_next_index] != NULL);
		g_free (ring_buffer[ring_buffer_next_index]);
	} else
		ring_buffer_num_lines++;

	g_assert (ring_buffer_num_lines <= ring_buffer_max_lines);

	ring_buffer[ring_buffer_next_index] = str;

	ring_buffer_next_index++;
	if (ring_buffer_next_index == ring_buffer_max_lines) {
		ring_buffer_next_index = 0;
		g_assert (ring_buffer_num_lines == ring_buffer_max_lines);
	}
}

static void
add_to_milestones (const gchar *str)
{
	gchar *str_copy;

	str_copy = g_strdup (str);

	if (milestones_tail) {
		milestones_tail = g_slist_append (milestones_tail, str_copy);
		milestones_tail = milestones_tail->next;
	} else {
		milestones_head = milestones_tail = g_slist_append (NULL, str_copy);
	}

	g_assert (milestones_head != NULL && milestones_tail != NULL);
}

/**
 * e_debug_logv:
 *
 * Since: 2.32
 **/
void
e_debug_logv (gboolean is_milestone, const gchar *domain, const gchar *format, va_list args)
{
	gchar *str;
	gchar *debug_str;
	struct timeval tv;
	struct tm tm;

	lock ();

	if (!(is_milestone || is_domain_enabled (domain)))
		goto out;

	str = g_strdup_vprintf (format, args);
	gettimeofday (&tv, NULL);

#ifdef __MINGW64__ /* MINGW64 sourceforge.net/p/mingw-w64/bugs/720/ */
  time_t t = tv.tv_sec;
	tm = *localtime (&t);
#else
	tm = *localtime (&tv.tv_sec);
#endif

	debug_str = g_strdup_printf ("%p;%04d/%02d/%02d;%02d:%02d:%02d.%04d;(%s);%s",
				     g_thread_self (),
				     tm.tm_year + 1900,
				     tm.tm_mon + 1,
				     tm.tm_mday,
				     tm.tm_hour,
				     tm.tm_min,
				     tm.tm_sec,
				     (gint) (tv.tv_usec / 100),
				     domain,
				     str);
	g_free (str);

	add_to_ring (debug_str);
	if (is_milestone)
		add_to_milestones (debug_str);

 out:
	unlock ();
}

/**
 * e_debug_log_load_configuration:
 *
 * Since: 2.32
 **/
gboolean
e_debug_log_load_configuration (const gchar *filename, GError **error)
{
	GKeyFile *key_file;
	gchar **strings;
	gsize num_strings;
	gint num;
	GError *my_error;

	g_assert (filename != NULL);
	g_assert (error == NULL || *error == NULL);

	key_file = g_key_file_new ();

	if (!g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, error)) {
		g_key_file_free (key_file);
		return FALSE;
	}

	/* Domains */

	my_error = NULL;
	strings = g_key_file_get_string_list (key_file, KEY_FILE_GROUP, KEY_FILE_DOMAINS_KEY, &num_strings, &my_error);
	if (my_error)
		g_error_free (my_error);
	else {
		gint i;

		for (i = 0; i < num_strings; i++)
			strings[i] = g_strstrip (strings[i]);

		e_debug_log_enable_domains ((const gchar **) strings, num_strings);
		g_strfreev (strings);
	}

	/* Number of lines */

	my_error = NULL;
	num = g_key_file_get_integer (key_file, KEY_FILE_GROUP, KEY_FILE_MAX_LINES_KEY, &my_error);
	if (my_error)
		g_error_free (my_error);
	else
		e_debug_log_set_max_lines (num);

	g_key_file_free (key_file);
	return TRUE;
}

/**
 * e_debug_log_enable_domains:
 *
 * Since: 2.32
 **/
void
e_debug_log_enable_domains (const gchar **domains, gint n_domains)
{
	gint i;

	g_assert (domains != NULL);
	g_assert (n_domains >= 0);

	lock ();

	if (!domains_hash)
		domains_hash = g_hash_table_new (g_str_hash, g_str_equal);

	for (i = 0; i < n_domains; i++) {
		g_assert (domains[i] != NULL);

		if (strcmp (domains[i], E_DEBUG_LOG_DOMAIN_USER) == 0)
			continue; /* user actions are always enabled */

		if (g_hash_table_lookup (domains_hash, domains[i]) == NULL) {
			gchar *domain;

			domain = g_strdup (domains[i]);
			g_hash_table_insert (domains_hash, domain, domain);
		}
	}

	unlock ();
}

/**
 * e_debug_log_disable_domains:
 *
 * Since: 2.32
 **/
void
e_debug_log_disable_domains (const gchar **domains, gint n_domains)
{
	gint i;

	g_assert (domains != NULL);
	g_assert (n_domains >= 0);

	lock ();

	if (domains_hash) {
		for (i = 0; i < n_domains; i++) {
			gchar *domain;

			g_assert (domains[i] != NULL);

			if (strcmp (domains[i], E_DEBUG_LOG_DOMAIN_USER) == 0)
				continue; /* user actions are always enabled */

			domain = g_hash_table_lookup (domains_hash, domains[i]);
			if (domain) {
				g_hash_table_remove (domains_hash, domain);
				g_free (domain);
			}
		}
	} /* else, there is nothing to disable */

	unlock ();
}

/**
 * e_debug_log_is_domain_enabled:
 *
 * Since: 2.32
 **/
gboolean
e_debug_log_is_domain_enabled (const gchar *domain)
{
	gboolean retval;

	g_assert (domain != NULL);

	lock ();
	retval = is_domain_enabled (domain);
	unlock ();

	return retval;
}

struct domains_dump_closure {
	gchar **domains;
	gint num_domains;
};

static void
domains_foreach_dump_cb (gpointer key, gpointer value, gpointer data)
{
	struct domains_dump_closure *closure;
	gchar *domain;

	closure = data;
	domain = key;

	closure->domains[closure->num_domains] = domain;
	closure->num_domains++;
}

static GKeyFile *
make_key_file_from_configuration (void)
{
	GKeyFile *key_file;
	struct domains_dump_closure closure;
	gint num_domains;

	key_file = g_key_file_new ();

	/* domains */

	if (domains_hash) {
		num_domains = g_hash_table_size (domains_hash);
		if (num_domains != 0) {
			closure.domains = g_new (gchar *, num_domains);
			closure.num_domains = 0;

			g_hash_table_foreach (domains_hash, domains_foreach_dump_cb, &closure);
			g_assert (num_domains == closure.num_domains);

			g_key_file_set_string_list (key_file, KEY_FILE_GROUP, KEY_FILE_DOMAINS_KEY,
						    (const gchar * const *) closure.domains, closure.num_domains);
			g_free (closure.domains);
		}
	}

	/* max lines */

	g_key_file_set_integer (key_file, KEY_FILE_GROUP, KEY_FILE_MAX_LINES_KEY, ring_buffer_max_lines);

	return key_file;
}

static gboolean
write_string (const gchar *filename, FILE *file, const gchar *str, GError **error)
{
	if (fputs (str, file) == EOF) {
		gint saved_errno;

		saved_errno = errno;
		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (saved_errno),
			     "error when writing to log file %s", filename);

		return FALSE;
	}

	return TRUE;
}

static gboolean
dump_configuration (const gchar *filename, FILE *file, GError **error)
{
	GKeyFile *key_file;
	gchar *data;
	gsize length;
	gboolean success;

	if (!write_string (filename, file,
			   "\n\n"
			   "This configuration for the debug log can be re-created\n"
			   "by putting the following in ~/evolution-data-server-debug-log.conf\n"
			   "(use ';' to separate domain names):\n\n",
			   error)) {
		return FALSE;
	}

	success = FALSE;

	key_file = make_key_file_from_configuration ();

	data = g_key_file_to_data (key_file, &length, error);
	if (!data)
		goto out;

	if (!write_string (filename, file, data, error)) {
		goto out;
	}

	success = TRUE;
 out:
	g_key_file_free (key_file);
	return success;
}

static gboolean
dump_milestones (const gchar *filename, FILE *file, GError **error)
{
	GSList *l;

	if (!write_string (filename, file, "===== BEGIN MILESTONES =====\n", error))
		return FALSE;

	for (l = milestones_head; l; l = l->next) {
		const gchar *str;

		str = l->data;
		if (!(write_string (filename, file, str, error)
		      && write_string (filename, file, "\n", error)))
			return FALSE;
	}

	if (!write_string (filename, file, "===== END MILESTONES =====\n", error))
		return FALSE;

	return TRUE;
}

static gboolean
dump_ring_buffer (const gchar *filename, FILE *file, GError **error)
{
	gint start_index;
	gint i;

	if (!write_string (filename, file, "===== BEGIN RING BUFFER =====\n", error))
		return FALSE;

	if (ring_buffer_num_lines == ring_buffer_max_lines)
		start_index = ring_buffer_next_index;
	else
		start_index = 0;

	for (i = 0; i < ring_buffer_num_lines; i++) {
		gint idx;

		idx = (start_index + i) % ring_buffer_max_lines;

		if (!(write_string (filename, file, ring_buffer[idx], error)
		      && write_string (filename, file, "\n", error))) {
			return FALSE;
		}
	}

	if (!write_string (filename, file, "===== END RING BUFFER =====\n", error))
		return FALSE;

	return TRUE;
}

/**
 * e_debug_log_dump:
 *
 * Since: 2.32
 **/
gboolean
e_debug_log_dump (const gchar *filename, GError **error)
{
	FILE *file;
	gboolean success;

	g_assert (error == NULL || *error == NULL);

	lock ();

	success = FALSE;

	file = fopen (filename, "w");
	if (!file) {
		gint saved_errno;

		saved_errno = errno;
		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (saved_errno),
			     "could not open log file %s", filename);
		goto out;
	}

	if (!(dump_milestones (filename, file, error)
	      && dump_ring_buffer (filename, file, error)
	      && dump_configuration (filename, file, error))) {
		goto do_close;
	}

	success = TRUE;

 do_close:

	if (fclose (file) != 0) {
		gint saved_errno;

		saved_errno = errno;

		if (error && *error) {
			g_error_free (*error);
			*error = NULL;
		}

		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (saved_errno),
			     "error when closing log file %s", filename);
		success = FALSE;
	}

 out:

	unlock ();
	return success;
}

/**
 * e_debug_log_dump_to_dated_file:
 *
 * Since: 2.32
 **/
gboolean
e_debug_log_dump_to_dated_file (GError **error)
{
	time_t t;
	struct tm tm;
	gchar *basename;
	gchar *filename;
	gboolean retval;

	g_assert (error == NULL || *error == NULL);

	t = time (NULL);
	tm = *localtime (&t);

	basename = g_strdup_printf ("e-debug-log-%04d-%02d-%02d-%02d-%02d-%02d.txt",
				    tm.tm_year + 1900,
				    tm.tm_mon + 1,
				    tm.tm_mday,
				    tm.tm_hour,
				    tm.tm_min,
				    tm.tm_sec);
	filename = g_build_filename (g_get_home_dir (), basename, NULL);

	retval = e_debug_log_dump (filename, error);

	g_free (basename);
	g_free (filename);

	return retval;
}

/**
 * e_debug_log_set_max_lines:
 *
 * Since: 2.32
 **/
void
e_debug_log_set_max_lines (gint num_lines)
{
	gchar **new_buffer;
	gint lines_to_copy;

	g_assert (num_lines > 0);

	lock ();

	if (num_lines == ring_buffer_max_lines)
		goto out;

	new_buffer = g_new0 (gchar *, num_lines);

	lines_to_copy = MIN (num_lines, ring_buffer_num_lines);

	if (ring_buffer) {
		gint start_index;
		gint i;

		if (ring_buffer_num_lines == ring_buffer_max_lines)
			start_index = (ring_buffer_next_index + ring_buffer_max_lines - lines_to_copy) % ring_buffer_max_lines;
		else
			start_index = ring_buffer_num_lines - lines_to_copy;

		g_assert (start_index >= 0 && start_index < ring_buffer_max_lines);

		for (i = 0; i < lines_to_copy; i++) {
			gint idx;

			idx = (start_index + i) % ring_buffer_max_lines;

			new_buffer[i] = ring_buffer[idx];
			ring_buffer[idx] = NULL;
		}

		for (i = 0; i < ring_buffer_max_lines; i++)
			g_free (ring_buffer[i]);

		g_free (ring_buffer);
	}

	ring_buffer = new_buffer;
	ring_buffer_next_index = lines_to_copy;
	ring_buffer_num_lines = lines_to_copy;
	ring_buffer_max_lines = num_lines;

 out:

	unlock ();
}

/**
 * e_debug_log_get_max_lines:
 *
 * Since: 2.32
 **/
gint
e_debug_log_get_max_lines (void)
{
	gint retval;

	lock ();
	retval = ring_buffer_max_lines;
	unlock ();

	return retval;
}

/**
 * e_debug_log_clear:
 *
 * Since: 2.32
 **/
void
e_debug_log_clear (void)
{
	gint i;

	lock ();

	if (!ring_buffer)
		goto out;

	for (i = 0; i < ring_buffer_max_lines; i++) {
		g_free (ring_buffer[i]);
		ring_buffer[i] = NULL;
	}

	ring_buffer_next_index = 0;
	ring_buffer_num_lines = 0;

 out:
	unlock ();
}


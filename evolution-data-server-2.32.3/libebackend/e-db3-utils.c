/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include "config.h"

#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include "db.h"

#include "e-db3-utils.h"

static gchar *
get_check_filename (const gchar *filename)
{
	return g_strdup_printf ("%s-upgrading", filename);
}

static gchar *
get_copy_filename (const gchar *filename)
{
	return g_strdup_printf ("%s-copy", filename);
}

static gint
cp_file (const gchar *src, const gchar *dest)
{
	gint i;
	gint o;
	gchar buffer[1024];
	gint length;
	gint place;

	i = g_open (src, O_RDONLY | O_BINARY, 0);
	if (i == -1)
		return -1;
	o = g_open (dest, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
	if (o == -1) {
		close (i);
		return -1;
	}
	while (1) {
		length = read (i, &buffer, sizeof (buffer));

		if (length == 0)
			break;

		if (length == -1) {
			if (errno == EINTR)
				continue;
			else {
				close (i);
				close (o);
				g_unlink (dest);
				return -1;
			}
		}

		place = 0;
		while (length != 0) {
			gint count;
			count = write (o, buffer + place, length);
			if (count == -1) {
				if (errno == EINTR)
					continue;
				else {
					close (i);
					close (o);
					g_unlink (dest);
					return -1;
				}
			}

			length -= count;
			place += count;
		}
	}
	i = close (i);
	if (close (o) == -1)
		i = -1;
	return (i == -1) ? -1 : 0;
}

static gint
touch_file (const gchar *file)
{
	gint o;
	o = g_open (file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
	if (o == -1)
		return -1;

	return close (o);
}

static gint
resume_upgrade (const gchar *filename, const gchar *copy_filename, const gchar *check_filename)
{
	DB *db;
	gint ret_val;

	ret_val = db_create (&db, NULL, 0);

	if (ret_val == 0)
		ret_val = cp_file (copy_filename, filename);

	if (ret_val == 0)
		ret_val = db->upgrade (db, filename, 0);

	if (ret_val == 0)
		ret_val = g_unlink (check_filename);
	if (ret_val == 0)
		ret_val = g_unlink (copy_filename);

	db->close (db, 0);

	return ret_val;
}

gint
e_db3_utils_maybe_recover (const gchar *filename)
{
	gint ret_val = 0;
	gchar *copy_filename;
	gchar *check_filename;

	copy_filename = get_copy_filename (filename);
	check_filename = get_check_filename (filename);

	if (g_file_test(check_filename, G_FILE_TEST_EXISTS)) {
		ret_val = resume_upgrade(filename, copy_filename, check_filename);
	} else if (g_file_test (copy_filename, G_FILE_TEST_EXISTS)) {
		g_unlink (copy_filename);
	}

	g_free (copy_filename);
	g_free (check_filename);
	return ret_val;
}

gint
e_db3_utils_upgrade_format (const gchar *filename)
{
	gchar *copy_filename;
	gchar *check_filename;
	DB *db;
	gint ret_val;

	ret_val = db_create (&db, NULL, 0);
	if (ret_val != 0)
		return ret_val;

	copy_filename = get_copy_filename (filename);
	check_filename = get_check_filename (filename);

	ret_val = cp_file (filename, copy_filename);

	if (ret_val == 0)
		ret_val = touch_file (check_filename);
	if (ret_val == 0)
		ret_val = db->upgrade (db, filename, 0);
	if (ret_val == 0)
		ret_val = g_unlink (check_filename);

	if (ret_val == 0)
		ret_val = g_unlink (copy_filename);

	db->close (db, 0);

	g_free (check_filename);
	g_free (copy_filename);
	return ret_val;
}

/*
 * e-autosave-utils.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include "e-autosave-utils.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <camel/camel.h>

#include <e-util/e-util.h>

#define SNAPSHOT_FILE_KEY	"e-composer-snapshot-file"
#define SNAPSHOT_FILE_PREFIX	".evolution-composer.autosave"
#define SNAPSHOT_FILE_SEED	SNAPSHOT_FILE_PREFIX "-XXXXXX"

typedef struct _LoadContext LoadContext;
typedef struct _SaveContext SaveContext;

struct _LoadContext {
	EMsgComposer *composer;
};

struct _SaveContext {
	GCancellable *cancellable;
};

static void
load_context_free (LoadContext *context)
{
	if (context->composer != NULL)
		g_object_unref (context->composer);

	g_slice_free (LoadContext, context);
}

static void
save_context_free (SaveContext *context)
{
	if (context->cancellable != NULL)
		g_object_unref (context->cancellable);

	g_slice_free (SaveContext, context);
}

static void
delete_snapshot_file (GFile *snapshot_file)
{
	g_file_delete (snapshot_file, NULL, NULL);
	g_object_unref (snapshot_file);
}

static GFile *
create_snapshot_file (EMsgComposer *composer,
                      GError **error)
{
	GFile *snapshot_file;
	const gchar *user_data_dir;
	gchar *path;
	gint fd;

	snapshot_file = e_composer_get_snapshot_file (composer);

	if (G_IS_FILE (snapshot_file))
		return snapshot_file;

	user_data_dir = e_get_user_data_dir ();
	path = g_build_filename (user_data_dir, SNAPSHOT_FILE_SEED, NULL);

	/* g_mkstemp() modifies the XXXXXX part of the
	 * template string to form the actual filename. */
	errno = 0;
	fd = g_mkstemp (path);
	if (fd == -1) {
		g_set_error (
			error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"%s", g_strerror (errno));
		g_free (path);
		return FALSE;
	}

	close (fd);

	snapshot_file = g_file_new_for_path (path);

	/* Save the GFile for subsequent snapshots. */
	g_object_set_data_full (
		G_OBJECT (composer),
		SNAPSHOT_FILE_KEY, snapshot_file,
		(GDestroyNotify) delete_snapshot_file);

	return snapshot_file;
}

static void
load_snapshot_loaded_cb (GFile *snapshot_file,
                         GAsyncResult *result,
                         GSimpleAsyncResult *simple)
{
	EShell *shell;
	GObject *object;
	LoadContext *context;
	EMsgComposer *composer;
	CamelMimeMessage *message;
	CamelStream *camel_stream;
	gchar *contents = NULL;
	gsize length;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	g_file_load_contents_finish (
		snapshot_file, result, &contents, &length, NULL, &error);

	if (error != NULL) {
		g_warn_if_fail (contents == NULL);
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);
		g_error_free (error);
		return;
	}

	/* Create an in-memory buffer for the MIME parser to read from.
	 * We have to do this because CamelStreams are syncrhonous-only,
	 * and feeding the parser a direct file stream would block. */
	message = camel_mime_message_new ();
	camel_stream = camel_stream_mem_new_with_buffer (contents, length);
	camel_data_wrapper_construct_from_stream (
		CAMEL_DATA_WRAPPER (message), camel_stream, &error);
	g_object_unref (camel_stream);
	g_free (contents);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (message);
		g_error_free (error);
		return;
	}

	/* g_async_result_get_source_object() returns a new reference. */
	object = g_async_result_get_source_object (G_ASYNC_RESULT (simple));

	/* Create a new composer window from the loaded message and
	 * restore its snapshot file so it continues auto-saving to
	 * the same file. */
	shell = E_SHELL (object);
	g_object_ref (snapshot_file);
	composer = e_msg_composer_new_with_message (shell, message);
	g_object_set_data_full (
		G_OBJECT (composer),
		SNAPSHOT_FILE_KEY, snapshot_file,
		(GDestroyNotify) delete_snapshot_file);
	context->composer = g_object_ref_sink (composer);
	g_object_unref (message);

	g_object_unref (object);

	g_simple_async_result_complete (simple);
	g_object_unref (simple);
}

static void
save_snapshot_splice_cb (GOutputStream *output_stream,
                         GAsyncResult *result,
                         GSimpleAsyncResult *simple)
{
	GError *error = NULL;

	g_output_stream_splice_finish (output_stream, result, &error);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}

	g_simple_async_result_complete (simple);
	g_object_unref (simple);
}

static void
save_snapshot_replace_cb (GFile *snapshot_file,
                          GAsyncResult *result,
                          GSimpleAsyncResult *simple)
{
	GObject *object;
	EMsgComposer *composer;
	SaveContext *context;
	CamelMimeMessage *message;
	GFileOutputStream *output_stream;
	GInputStream *input_stream;
	CamelStream *camel_stream;
	GByteArray *buffer;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	output_stream = g_file_replace_finish (snapshot_file, result, &error);

	if (error != NULL) {
		g_warn_if_fail (output_stream == NULL);
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
		g_error_free (error);
		return;
	}

	g_return_if_fail (G_IS_OUTPUT_STREAM (output_stream));

	/* g_async_result_get_source_object() returns a new reference. */
	object = g_async_result_get_source_object (G_ASYNC_RESULT (simple));

	/* Extract a MIME message from the composer. */
	composer = E_MSG_COMPOSER (object);
	message = e_msg_composer_get_message_draft (composer, &error);

	g_object_unref (object);

	if (error != NULL) {
		g_warn_if_fail (message == NULL);
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (output_stream);
		g_object_unref (simple);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	/* Decode the message to an in-memory buffer.  We have to do this
	 * because CamelStreams are synchronous-only, and using threads is
	 * dangerous because CamelDataWrapper is not reentrant. */
	buffer = g_byte_array_new ();
	camel_stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (
		CAMEL_STREAM_MEM (camel_stream), buffer);
	camel_data_wrapper_decode_to_stream (
		CAMEL_DATA_WRAPPER (message), camel_stream, NULL);
	g_object_unref (camel_stream);
	g_object_unref (message);

	/* Load the buffer into a GMemoryInputStream. */
	input_stream = g_memory_input_stream_new ();
	if (buffer->len > 0)
		g_memory_input_stream_add_data (
			G_MEMORY_INPUT_STREAM (input_stream),
			buffer->data, (gssize) buffer->len,
			(GDestroyNotify) g_free);
	g_byte_array_free (buffer, FALSE);

	/* Splice the input and output streams. */
	g_output_stream_splice_async (
		G_OUTPUT_STREAM (output_stream), input_stream,
		G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
		G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
		G_PRIORITY_DEFAULT, context->cancellable,
		(GAsyncReadyCallback) save_snapshot_splice_cb,
		simple);

	g_object_unref (input_stream);
	g_object_unref (output_stream);
}

static EMsgComposer *
composer_registry_lookup (GQueue *registry,
                          const gchar *basename)
{
	GList *iter;

	/* Find the composer with the given snapshot filename. */
	for (iter = registry->head; iter != NULL; iter = iter->next) {
		EMsgComposer *composer;
		GFile *snapshot_file;
		gchar *snapshot_name;

		composer = E_MSG_COMPOSER (iter->data);
		snapshot_file = e_composer_get_snapshot_file (composer);

		if (!G_IS_FILE (snapshot_file))
			continue;

		snapshot_name = g_file_get_basename (snapshot_file);
		if (g_strcmp0 (basename, snapshot_name) == 0) {
			g_free (snapshot_name);
			return composer;
		}

		g_free (snapshot_name);
	}

	return NULL;
}

GList *
e_composer_find_orphans (GQueue *registry,
                         GError **error)
{
	GDir *dir;
	const gchar *dirname;
	const gchar *basename;
	GList *orphans = NULL;

	g_return_val_if_fail (registry != NULL, NULL);

	dirname = e_get_user_data_dir ();
	dir = g_dir_open (dirname, 0, error);
	if (dir == NULL)
		return NULL;

	/* Scan the user data directory for snapshot files. */
	while ((basename = g_dir_read_name (dir)) != NULL) {
		const gchar *errmsg;
		gchar *filename;
#ifdef __MINGW64__
		struct _stat64 st;
#else
		struct stat st;
#endif

		/* Is this a snapshot file? */
		if (!g_str_has_prefix (basename, SNAPSHOT_FILE_PREFIX))
			continue;

		/* Is this an orphaned snapshot file? */
		if (composer_registry_lookup (registry, basename) != NULL)
			continue;

		filename = g_build_filename (dirname, basename, NULL);

		/* Try to examine the snapshot file.  Failure here
		 * is non-fatal; just emit a warning and move on. */
		errno = 0;
		if (g_stat (filename, &st) < 0) {
			errmsg = g_strerror (errno);
			g_warning ("%s: %s", filename, errmsg);
			g_free (filename);
			continue;
		}

		/* If the file is empty, delete it.  Failure here
		 * is non-fatal; just emit a warning and move on. */
		if (st.st_size == 0) {
			errno = 0;
			if (g_unlink (filename) < 0) {
				errmsg = g_strerror (errno);
				g_warning ("%s: %s", filename, errmsg);
			}
			g_free (filename);
			continue;
		}

		orphans = g_list_prepend (
			orphans, g_file_new_for_path (filename));

		g_free (filename);
	}

	g_dir_close (dir);

	return g_list_reverse (orphans);
}

void
e_composer_load_snapshot (EShell *shell,
                          GFile *snapshot_file,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
	GSimpleAsyncResult *simple;
	LoadContext *context;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (G_IS_FILE (snapshot_file));

	context = g_slice_new0 (LoadContext);

	simple = g_simple_async_result_new (
		G_OBJECT (shell), callback, user_data,
		e_composer_load_snapshot);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) load_context_free);

	g_file_load_contents_async (
		snapshot_file, cancellable, (GAsyncReadyCallback)
		load_snapshot_loaded_cb, simple);
}

EMsgComposer *
e_composer_load_snapshot_finish (EShell *shell,
                                 GAsyncResult *result,
                                 GError **error)
{
	GSimpleAsyncResult *simple;
	LoadContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
			result, G_OBJECT (shell),
			e_composer_load_snapshot), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (context->composer), NULL);

	return g_object_ref (context->composer);
}

void
e_composer_save_snapshot (EMsgComposer *composer,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
	GSimpleAsyncResult *simple;
	SaveContext *context;
	GFile *snapshot_file;
	GError *error = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	context = g_slice_new0 (SaveContext);

	if (G_IS_CANCELLABLE (cancellable))
		context->cancellable = g_object_ref (cancellable);

	simple = g_simple_async_result_new (
		G_OBJECT (composer), callback, user_data,
		e_composer_save_snapshot);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) save_context_free);

	snapshot_file = e_composer_get_snapshot_file (composer);

	if (!G_IS_FILE (snapshot_file))
		snapshot_file = create_snapshot_file (composer, &error);

	if (error != NULL) {
		g_warn_if_fail (snapshot_file == NULL);
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
		g_error_free (error);
		return;
	}

	g_return_if_fail (G_IS_FILE (snapshot_file));

	g_file_replace_async (
		snapshot_file, NULL, FALSE,
		G_FILE_CREATE_PRIVATE, G_PRIORITY_DEFAULT,
		context->cancellable, (GAsyncReadyCallback)
		save_snapshot_replace_cb, simple);
}

gboolean
e_composer_save_snapshot_finish (EMsgComposer *composer,
                                 GAsyncResult *result,
                                 GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
			result, G_OBJECT (composer),
			e_composer_save_snapshot), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Success is assumed unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

GFile *
e_composer_get_snapshot_file (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return g_object_get_data (G_OBJECT (composer), SNAPSHOT_FILE_KEY);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include <config.h>

#include <libedataserver/e-data-server-util.h>

#include "e-data-book-view.h"
#include "e-data-book.h"
#include "e-book-backend.h"

#define E_BOOK_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_BOOK_BACKEND, EBookBackendPrivate))

struct _EBookBackendPrivate {
	GMutex *open_mutex;

	GMutex *clients_mutex;
	GList *clients;

	ESource *source;
	gboolean loaded, writable, removed, online;

	GMutex *views_mutex;
	EList *views;

	gchar *cache_dir;
};

/* Property IDs */
enum {
	PROP_0,
	PROP_CACHE_DIR
};

/* Signal IDs */
enum {
	LAST_CLIENT_GONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EBookBackend, e_book_backend, G_TYPE_OBJECT)

static void
book_backend_set_default_cache_dir (EBookBackend *backend)
{
	ESource *source;
	const gchar *user_cache_dir;
	gchar *mangled_uri;
	gchar *filename;

	user_cache_dir = e_get_user_cache_dir ();

	source = e_book_backend_get_source (backend);
	g_return_if_fail (source != NULL);

	/* Mangle the URI to not contain invalid characters. */
	mangled_uri = g_strdelimit (e_source_get_uri (source), ":/", '_');

	filename = g_build_filename (
		user_cache_dir, "addressbook", mangled_uri, NULL);
	e_book_backend_set_cache_dir (backend, filename);
	g_free (filename);

	g_free (mangled_uri);
}

static void
book_backend_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CACHE_DIR:
			e_book_backend_set_cache_dir (
				E_BOOK_BACKEND (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_backend_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CACHE_DIR:
			g_value_set_string (
				value, e_book_backend_get_cache_dir (
				E_BOOK_BACKEND (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_backend_dispose (GObject *object)
{
	EBookBackendPrivate *priv;

	priv = E_BOOK_BACKEND_GET_PRIVATE (object);

	if (priv->views != NULL) {
		g_object_unref (priv->views);
		priv->views = NULL;
	}

	if (priv->source != NULL) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_book_backend_parent_class)->dispose (object);
}

static void
book_backend_finalize (GObject *object)
{
	EBookBackendPrivate *priv;

	priv = E_BOOK_BACKEND_GET_PRIVATE (object);

	g_list_free (priv->clients);

	g_mutex_free (priv->open_mutex);
	g_mutex_free (priv->clients_mutex);
	g_mutex_free (priv->views_mutex);

	g_free (priv->cache_dir);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_book_backend_parent_class)->finalize (object);
}

static void
e_book_backend_class_init (EBookBackendClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EBookBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = book_backend_set_property;
	object_class->get_property = book_backend_get_property;
	object_class->dispose = book_backend_dispose;
	object_class->finalize = book_backend_finalize;

	g_object_class_install_property (
		object_class,
		PROP_CACHE_DIR,
		g_param_spec_string (
			"cache-dir",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	signals[LAST_CLIENT_GONE] = g_signal_new (
		"last-client-gone",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EBookBackendClass, last_client_gone),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_book_backend_init (EBookBackend *backend)
{
	backend->priv = E_BOOK_BACKEND_GET_PRIVATE (backend);

	backend->priv->views = e_list_new (
		(EListCopyFunc) NULL, (EListFreeFunc) NULL, NULL);

	backend->priv->open_mutex = g_mutex_new ();
	backend->priv->clients_mutex = g_mutex_new ();
	backend->priv->views_mutex = g_mutex_new ();
}

/**
 * e_book_backend_get_cache_dir:
 * @backend: an #EBookBackend
 *
 * Returns the cache directory for the given backend.
 *
 * Returns: the cache directory for the backend
 *
 * Since: 2.32
 **/
const gchar *
e_book_backend_get_cache_dir (EBookBackend *backend)
{
	g_return_val_if_fail (E_IS_BOOK_BACKEND (backend), NULL);

	return backend->priv->cache_dir;
}

/**
 * e_book_backend_set_cache_dir:
 * @backend: an #EBookBackend
 * @cache_dir: a local cache directory
 *
 * Sets the cache directory for the given backend.
 *
 * Note that #EBookBackend is initialized with a usable default based on
 * the #ESource given to e_book_backend_load_source().  Backends should
 * not override the default without good reason.
 *
 * Since: 2.32
 **/
void
e_book_backend_set_cache_dir (EBookBackend *backend,
                              const gchar *cache_dir)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (cache_dir != NULL);

	g_free (backend->priv->cache_dir);
	backend->priv->cache_dir = g_strdup (cache_dir);

	g_object_notify (G_OBJECT (backend), "cache-dir");
}

/**
 * e_book_backend_load_source:
 * @backend: an #EBookBackend
 * @source: an #ESource to load
 * @only_if_exists: %TRUE to prevent the creation of a new book
 * @error: #GError to set, when something fails
 *
 * Loads @source into @backend.
 **/
void
e_book_backend_load_source (EBookBackend           *backend,
			    ESource                *source,
			    gboolean                only_if_exists,
			    GError		  **error)
{
	GError *local_error = NULL;

	e_return_data_book_error_if_fail (E_IS_BOOK_BACKEND (backend), E_DATA_BOOK_STATUS_INVALID_ARG);
	e_return_data_book_error_if_fail (source, E_DATA_BOOK_STATUS_INVALID_ARG);
	e_return_data_book_error_if_fail (backend->priv->loaded == FALSE, E_DATA_BOOK_STATUS_INVALID_ARG);

	/* Subclasses may need to call e_book_backend_get_cache_dir() in
	 * their load_source() methods, so get the "cache-dir" property
	 * initialized before we call the method. */
	backend->priv->source = g_object_ref (source);
	book_backend_set_default_cache_dir (backend);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->load_source);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->load_source) (backend, source, only_if_exists, &local_error);

	if (g_error_matches (local_error, E_DATA_BOOK_ERROR,
		E_DATA_BOOK_STATUS_INVALID_SERVER_VERSION))
		g_error_free (local_error);
	else if (local_error != NULL)
		g_propagate_error (error, local_error);
}

/**
 * e_book_backend_get_source:
 * @backend: An addressbook backend.
 *
 * Queries the source that an addressbook backend is serving.
 *
 * Returns: ESource for the backend.
 **/
ESource *
e_book_backend_get_source (EBookBackend *backend)
{
	g_return_val_if_fail (E_IS_BOOK_BACKEND (backend), NULL);

	return backend->priv->source;
}

/**
 * e_book_backend_open:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 * @only_if_exists: %TRUE to prevent the creation of a new book
 *
 * Executes an 'open' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_open (EBookBackend *backend,
		     EDataBook    *book,
		     guint32       opid,
		     gboolean      only_if_exists)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));

	g_mutex_lock (backend->priv->open_mutex);

	if (backend->priv->loaded) {
		e_data_book_report_writable (book, backend->priv->writable);
		e_data_book_report_connection_status (book, backend->priv->online);

		e_data_book_respond_open (book, opid, NULL);
	} else {
		GError *error = NULL;

		e_book_backend_load_source (backend, e_data_book_get_source (book), only_if_exists, &error);

		if (!error || g_error_matches (error, E_DATA_BOOK_ERROR, E_DATA_BOOK_STATUS_INVALID_SERVER_VERSION)) {
			e_data_book_report_writable (book, backend->priv->writable);
			e_data_book_report_connection_status (book, backend->priv->online);
		}

		e_data_book_respond_open (book, opid, error);
	}

	g_mutex_unlock (backend->priv->open_mutex);
}

/**
 * e_book_backend_remove:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 *
 * Executes a 'remove' request to remove all of @backend's data,
 * specified by @opid on @book.
 **/
void
e_book_backend_remove (EBookBackend *backend,
		       EDataBook    *book,
		       guint32       opid)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->remove);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->remove) (backend, book, opid);
}

/**
 * e_book_backend_create_contact:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 * @vcard: the VCard to add
 *
 * Executes a 'create contact' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_create_contact (EBookBackend *backend,
			       EDataBook    *book,
			       guint32       opid,
			       const gchar   *vcard)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));
	g_return_if_fail (vcard);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->create_contact);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->create_contact) (backend, book, opid, vcard);
}

/**
 * e_book_backend_remove_contacts:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 * @id_list: list of string IDs to remove
 *
 * Executes a 'remove contacts' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_remove_contacts (EBookBackend *backend,
				EDataBook    *book,
				guint32       opid,
				GList        *id_list)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));
	g_return_if_fail (id_list);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->remove_contacts);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->remove_contacts) (backend, book, opid, id_list);
}

/**
 * e_book_backend_modify_contact:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 * @vcard: the VCard to update
 *
 * Executes a 'modify contact' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_modify_contact (EBookBackend *backend,
			       EDataBook    *book,
			       guint32       opid,
			       const gchar   *vcard)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));
	g_return_if_fail (vcard);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->modify_contact);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->modify_contact) (backend, book, opid, vcard);
}

/**
 * e_book_backend_get_contact:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 * @id: the ID of the contact to get
 *
 * Executes a 'get contact' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_get_contact (EBookBackend *backend,
			    EDataBook    *book,
			    guint32       opid,
			    const gchar   *id)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));
	g_return_if_fail (id);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->get_contact);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->get_contact) (backend, book, opid, id);
}

/**
 * e_book_backend_get_contact_list:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 * @query: the s-expression to match
 *
 * Executes a 'get contact list' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_get_contact_list (EBookBackend *backend,
				 EDataBook    *book,
				 guint32       opid,
				 const gchar   *query)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));
	g_return_if_fail (query);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->get_contact_list);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->get_contact_list) (backend, book, opid, query);
}

/**
 * e_book_backend_start_book_view:
 * @backend: an #EBookBackend
 * @book_view: the #EDataBookView to start
 *
 * Starts running the query specified by @book_view, emitting
 * signals for matching contacts.
 **/
void
e_book_backend_start_book_view (EBookBackend  *backend,
				EDataBookView *book_view)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK_VIEW (book_view));

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->start_book_view);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->start_book_view) (backend, book_view);
}

/**
 * e_book_backend_stop_book_view:
 * @backend: an #EBookBackend
 * @book_view: the #EDataBookView to stop
 *
 * Stops running the query specified by @book_view, emitting
 * no more signals.
 **/
void
e_book_backend_stop_book_view (EBookBackend  *backend,
			       EDataBookView *book_view)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK_VIEW (book_view));

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->stop_book_view);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->stop_book_view) (backend, book_view);
}

/**
 * e_book_backend_get_changes:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 * @change_id: the ID of the changeset
 *
 * Executes a 'get changes' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_get_changes (EBookBackend *backend,
			    EDataBook    *book,
			    guint32       opid,
			    const gchar   *change_id)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));
	g_return_if_fail (change_id);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->get_changes);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->get_changes) (backend, book, opid, change_id);
}

/**
 * e_book_backend_authenticate_user:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 * @user: the name of the user account
 * @passwd: the user's password
 * @auth_method: the authentication method to use
 *
 * Executes an 'authenticate' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_authenticate_user (EBookBackend *backend,
				  EDataBook    *book,
				  guint32       opid,
				  const gchar   *user,
				  const gchar   *passwd,
				  const gchar   *auth_method)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));
	g_return_if_fail (user && passwd && auth_method);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->authenticate_user);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->authenticate_user) (backend, book, opid, user, passwd, auth_method);
}

/**
 * e_book_backend_get_required_fields:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 *
 * Executes a 'get required fields' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_get_required_fields (EBookBackend *backend,
				     EDataBook    *book,
				     guint32       opid)

{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->get_required_fields);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->get_required_fields) (backend, book, opid);
}

/**
 * e_book_backend_get_supported_fields:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 *
 * Executes a 'get supported fields' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_get_supported_fields (EBookBackend *backend,
				     EDataBook    *book,
				     guint32       opid)

{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->get_supported_fields);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->get_supported_fields) (backend, book, opid);
}

/**
 * e_book_backend_get_supported_auth_methods:
 * @backend: an #EBookBackend
 * @book: an #EDataBook
 * @opid: the ID to use for this operation
 *
 * Executes a 'get supported auth methods' request specified by @opid on @book
 * using @backend.
 **/
void
e_book_backend_get_supported_auth_methods (EBookBackend *backend,
					   EDataBook    *book,
					   guint32       opid)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->get_supported_auth_methods);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->get_supported_auth_methods) (backend, book, opid);
}

/**
 * e_book_backend_cancel_operation:
 * @backend: an #EBookBackend
 * @book: an #EDataBook whose operation should be cancelled
 * @error: #GError to set, when something fails
 *
 * Cancel @book's running operation on @backend.
 **/
void
e_book_backend_cancel_operation (EBookBackend *backend,
				 EDataBook    *book,
				 GError      **error)
{
	e_return_data_book_error_if_fail (E_IS_BOOK_BACKEND (backend), E_DATA_BOOK_STATUS_INVALID_ARG);
	e_return_data_book_error_if_fail (E_IS_DATA_BOOK (book), E_DATA_BOOK_STATUS_INVALID_ARG);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->cancel_operation);

	(* E_BOOK_BACKEND_GET_CLASS (backend)->cancel_operation) (backend, book, error);
}

static void
last_client_gone (EBookBackend *backend)
{
	g_signal_emit (backend, signals[LAST_CLIENT_GONE], 0);
}

/**
 * e_book_backend_get_book_views:
 * @backend: an #EBookBackend
 *
 * Gets the list of #EDataBookView views running on this backend.
 *
 * Returns: An #EList of #EDataBookView objects.
 **/
EList*
e_book_backend_get_book_views (EBookBackend *backend)
{
	g_return_val_if_fail (E_IS_BOOK_BACKEND (backend), NULL);

	return g_object_ref (backend->priv->views);
}

/**
 * e_book_backend_add_book_view:
 * @backend: an #EBookBackend
 * @view: an #EDataBookView
 *
 * Adds @view to @backend for querying.
 **/
void
e_book_backend_add_book_view (EBookBackend *backend,
			      EDataBookView *view)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));

	g_mutex_lock (backend->priv->views_mutex);

	e_list_append (backend->priv->views, view);

	g_mutex_unlock (backend->priv->views_mutex);
}

/**
 * e_book_backend_remove_book_view:
 * @backend: an #EBookBackend
 * @view: an #EDataBookView
 *
 * Removes @view from @backend.
 **/
void
e_book_backend_remove_book_view (EBookBackend *backend,
				 EDataBookView *view)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));

	g_mutex_lock (backend->priv->views_mutex);

	e_list_remove (backend->priv->views, view);

	g_mutex_unlock (backend->priv->views_mutex);
}

/**
 * e_book_backend_add_client:
 * @backend: An addressbook backend.
 * @book: the corba object representing the client connection.
 *
 * Adds a client to an addressbook backend.
 *
 * Returns: TRUE on success, FALSE on failure to add the client.
 */
gboolean
e_book_backend_add_client (EBookBackend      *backend,
			   EDataBook         *book)
{
	g_return_val_if_fail (E_IS_BOOK_BACKEND (backend), FALSE);
	g_return_val_if_fail (E_IS_DATA_BOOK (book), FALSE);

	g_mutex_lock (backend->priv->clients_mutex);
	backend->priv->clients = g_list_prepend (backend->priv->clients, book);
	g_mutex_unlock (backend->priv->clients_mutex);

	return TRUE;
}

/**
 * e_book_backend_remove_client:
 * @backend: an #EBookBackend
 * @book: an #EDataBook to remove
 *
 * Removes @book from the list of @backend's clients.
 **/
void
e_book_backend_remove_client (EBookBackend *backend,
			      EDataBook    *book)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));
	g_return_if_fail (E_IS_DATA_BOOK (book));

	/* up our backend's refcount here so that last_client_gone
	   doesn't end up unreffing us (while we're holding the
	   lock) */
	g_object_ref (backend);

	/* Disconnect */
	g_mutex_lock (backend->priv->clients_mutex);
	backend->priv->clients = g_list_remove (backend->priv->clients, book);

	/* When all clients go away, notify the parent factory about it so that
	 * it may decide whether to kill the backend or not.
	 */
	if (!backend->priv->clients)
		last_client_gone (backend);

	g_mutex_unlock (backend->priv->clients_mutex);

	g_object_unref (backend);
}

/**
 * e_book_backend_has_out_of_proc_clients:
 * @backend: an #EBookBackend
 *
 * Checks if @backend has clients running in other system processes.
 *
 * Returns: %TRUE if there are clients in other processes, %FALSE otherwise.
 **/
gboolean
e_book_backend_has_out_of_proc_clients (EBookBackend *backend)
{
	return TRUE;
}

/**
 * e_book_backend_get_static_capabilities:
 * @backend: an #EBookBackend
 *
 * Gets the capabilities offered by this @backend.
 *
 * Returns: A string listing the capabilities.
 **/
gchar *
e_book_backend_get_static_capabilities (EBookBackend *backend)
{
	g_return_val_if_fail (E_IS_BOOK_BACKEND (backend), NULL);

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->get_static_capabilities);

	return E_BOOK_BACKEND_GET_CLASS (backend)->get_static_capabilities (backend);
}

/**
 * e_book_backend_is_loaded:
 * @backend: an #EBookBackend
 *
 * Checks if @backend's storage has been opened and the backend
 * itself is ready for accessing.
 *
 * Returns: %TRUE if loaded, %FALSE otherwise.
 **/
gboolean
e_book_backend_is_loaded (EBookBackend *backend)
{
	g_return_val_if_fail (E_IS_BOOK_BACKEND (backend), FALSE);

	return backend->priv->loaded;
}

/**
 * e_book_backend_set_is_loaded:
 * @backend: an #EBookBackend
 * @is_loaded: A flag indicating whether the backend is loaded
 *
 * Sets the flag indicating whether @backend is loaded to @is_loaded.
 * Meant to be used by backend implementations.
 **/
void
e_book_backend_set_is_loaded (EBookBackend *backend, gboolean is_loaded)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));

	backend->priv->loaded = is_loaded;
}

/**
 * e_book_backend_is_writable:
 * @backend: an #EBookBackend
 *
 * Checks if we can write to @backend.
 *
 * Returns: %TRUE if writeable, %FALSE if not.
 **/
gboolean
e_book_backend_is_writable (EBookBackend *backend)
{
	g_return_val_if_fail (E_IS_BOOK_BACKEND (backend), FALSE);

	return backend->priv->writable;
}

/**
 * e_book_backend_set_is_writable:
 * @backend: an #EBookBackend
 * @is_writable: A flag indicating whether the backend is writeable
 *
 * Sets the flag indicating whether @backend is writeable to @is_writeable.
 * Meant to be used by backend implementations.
 **/
void
e_book_backend_set_is_writable (EBookBackend *backend, gboolean is_writable)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));

	backend->priv->writable = is_writable;
}

/**
 * e_book_backend_is_removed:
 * @backend: an #EBookBackend
 *
 * Checks if @backend has been removed from its physical storage.
 *
 * Returns: %TRUE if @backend has been removed, %FALSE otherwise.
 **/
gboolean
e_book_backend_is_removed (EBookBackend *backend)
{
	g_return_val_if_fail (E_IS_BOOK_BACKEND (backend), FALSE);

	return backend->priv->removed;
}

/**
 * e_book_backend_set_is_removed:
 * @backend: an #EBookBackend
 * @is_removed: A flag indicating whether the backend's storage was removed
 *
 * Sets the flag indicating whether @backend was removed to @is_removed.
 * Meant to be used by backend implementations.
 **/
void
e_book_backend_set_is_removed (EBookBackend *backend, gboolean is_removed)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));

	backend->priv->removed = is_removed;
}

/**
 * e_book_backend_set_mode:
 * @backend: an #EBookbackend
 * @mode: a mode indicating the online/offline status of the backend
 *
 * Sets @backend's online/offline mode to @mode. Mode can be 1 for offline
 * or 2 indicating that it is connected and online.
 **/
void
e_book_backend_set_mode (EBookBackend *backend,
			 EDataBookMode mode)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));

	g_assert (E_BOOK_BACKEND_GET_CLASS (backend)->set_mode);

        (* E_BOOK_BACKEND_GET_CLASS (backend)->set_mode) (backend,  mode);

}

/**
 * e_book_backend_sync:
 * @backend: an #EBookbackend
 *
 * Write all pending data to disk.  This is only required under special
 * circumstances (for example before a live backup) and should not be used in
 * normal use.
 *
 * Since: 1.12
 */
void
e_book_backend_sync (EBookBackend *backend)
{
	g_return_if_fail (E_IS_BOOK_BACKEND (backend));

	if (E_BOOK_BACKEND_GET_CLASS (backend)->sync)
		(* E_BOOK_BACKEND_GET_CLASS (backend)->sync) (backend);
}

/**
 * e_book_backend_change_add_new:
 * @vcard: a VCard string
 *
 * Creates a new change item indicating @vcard was added.
 * Meant to be used by backend implementations.
 *
 * Returns: A new #EDataBookChange.
 **/
EDataBookChange *
e_book_backend_change_add_new     (const gchar *vcard)
{
  EDataBookChange *new_change = g_new (EDataBookChange, 1);

	new_change->change_type = E_DATA_BOOK_BACKEND_CHANGE_ADDED;
	new_change->vcard = g_strdup (vcard);

	return new_change;
}

/**
 * e_book_backend_change_modify_new:
 * @vcard: a VCard string
 *
 * Creates a new change item indicating @vcard was modified.
 * Meant to be used by backend implementations.
 *
 * Returns: A new #EDataBookChange.
 **/
EDataBookChange *
e_book_backend_change_modify_new  (const gchar *vcard)
{
  EDataBookChange *new_change = g_new (EDataBookChange, 1);

	new_change->change_type = E_DATA_BOOK_BACKEND_CHANGE_MODIFIED;
	new_change->vcard = g_strdup (vcard);

	return new_change;
}

/**
 * e_book_backend_change_delete_new:
 * @vcard: a VCard string
 *
 * Creates a new change item indicating @vcard was deleted.
 * Meant to be used by backend implementations.
 *
 * Returns: A new #EDataBookChange.
 **/
EDataBookChange *
e_book_backend_change_delete_new  (const gchar *vcard)
{
  EDataBookChange *new_change = g_new (EDataBookChange, 1);

	new_change->change_type = E_DATA_BOOK_BACKEND_CHANGE_DELETED;
	new_change->vcard = g_strdup (vcard);

	return new_change;
}



static void
e_book_backend_foreach_view (EBookBackend *backend,
			     void (*callback) (EDataBookView *, gpointer),
			     gpointer user_data)
{
	EList *views;
	EDataBookView *view;
	EIterator *iter;

	views = e_book_backend_get_book_views (backend);
	iter = e_list_get_iterator (views);

	while (e_iterator_is_valid (iter)) {
		view = (EDataBookView*)e_iterator_get (iter);

		e_data_book_view_ref (view);
		callback (view, user_data);
		e_data_book_view_unref (view);

		e_iterator_next (iter);
	}

	g_object_unref (iter);
	g_object_unref (views);
}

static void
view_notify_update (EDataBookView *view, gpointer contact)
{
	e_data_book_view_notify_update (view, contact);
}

/**
 * e_book_backend_notify_update:
 * @backend: an #EBookBackend
 * @contact: a new or modified contact
 *
 * Notifies all of @backend's book views about the new or modified
 * contacts @contact.
 *
 * e_data_book_respond_create() and e_data_book_respond_modify() call this
 * function for you. You only need to call this from your backend if
 * contacts are created or modified by another (non-PAS-using) client.
 **/
void
e_book_backend_notify_update (EBookBackend *backend, EContact *contact)
{
	e_book_backend_foreach_view (backend, view_notify_update, contact);
}

static void
view_notify_remove (EDataBookView *view, gpointer id)
{
	e_data_book_view_notify_remove (view, id);
}

/**
 * e_book_backend_notify_remove:
 * @backend: an #EBookBackend
 * @id: a contact id
 *
 * Notifies all of @backend's book views that the contact with UID
 * @id has been removed.
 *
 * e_data_book_respond_remove_contacts() calls this function for you. You
 * only need to call this from your backend if contacts are removed by
 * another (non-PAS-using) client.
 **/
void
e_book_backend_notify_remove (EBookBackend *backend, const gchar *id)
{
	e_book_backend_foreach_view (backend, view_notify_remove, (gpointer)id);
}

static void
view_notify_complete (EDataBookView *view, gpointer unused)
{
	e_data_book_view_notify_complete (view, NULL /* SUCCESS */);
}

/**
 * e_book_backend_notify_complete:
 * @backend: an #EBookbackend
 *
 * Notifies all of @backend's book views that the current set of
 * notifications is complete; use this after a series of
 * e_book_backend_notify_update() and e_book_backend_notify_remove() calls.
 **/
void
e_book_backend_notify_complete (EBookBackend *backend)
{
	e_book_backend_foreach_view (backend, view_notify_complete, NULL);
}


/**
 * e_book_backend_notify_writable:
 * @backend: an #EBookBackend
 * @is_writable: flag indicating writable status
 *
 * Notifies all backends clients about the current writable state.
 **/
void
e_book_backend_notify_writable (EBookBackend *backend, gboolean is_writable)
{
	EBookBackendPrivate *priv;
	GList *clients;

	priv = backend->priv;
	priv->writable = is_writable;
	g_mutex_lock (priv->clients_mutex);

	for (clients = priv->clients; clients != NULL; clients = g_list_next (clients))
		e_data_book_report_writable (E_DATA_BOOK (clients->data), is_writable);

	g_mutex_unlock (priv->clients_mutex);

}

/**
 * e_book_backend_notify_connection_status:
 * @backend: an #EBookBackend
 * @is_online: flag indicating whether @backend is connected and online
 *
 * Notifies clients of @backend's connection status indicated by @is_online.
 * Meant to be used by backend implementations.
 **/
void
e_book_backend_notify_connection_status (EBookBackend *backend, gboolean is_online)
{
	EBookBackendPrivate *priv;
	GList *clients;

	priv = backend->priv;
	priv->online = is_online;
	g_mutex_lock (priv->clients_mutex);

	for (clients = priv->clients; clients != NULL; clients = g_list_next (clients))
		e_data_book_report_connection_status (E_DATA_BOOK (clients->data), is_online);

	g_mutex_unlock (priv->clients_mutex);
}

/**
 * e_book_backend_notify_auth_required:
 * @backend: an #EBookBackend
 *
 * Notifies clients that @backend requires authentication in order to
 * connect. Means to be used by backend implementations.
 **/
void
e_book_backend_notify_auth_required (EBookBackend *backend)
{
	EBookBackendPrivate *priv;
	GList *clients;

	priv = backend->priv;
	g_mutex_lock (priv->clients_mutex);

	for (clients = priv->clients; clients != NULL; clients = g_list_next (clients))
		e_data_book_report_auth_required (E_DATA_BOOK (clients->data));
	g_mutex_unlock (priv->clients_mutex);
}


/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-composer-post-header.h"

#include <string.h>
#include <glib/gi18n.h>
#include <camel/camel.h>

#define E_COMPOSER_POST_HEADER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COMPOSER_POST_HEADER, EComposerPostHeaderPrivate))

enum {
	PROP_0,
	PROP_ACCOUNT
};

struct _EComposerPostHeaderPrivate {
	EAccount *account;
	gchar *base_url;  /* derived from account */
	gboolean custom;
};

G_DEFINE_TYPE (
	EComposerPostHeader,
	e_composer_post_header,
	E_TYPE_COMPOSER_TEXT_HEADER)

static gchar *
composer_post_header_folder_name_to_string (EComposerPostHeader *header,
                                            const gchar *url)
{
	gchar *res = NULL;
	const gchar *base_url = header->priv->base_url;

	if (base_url != NULL) {
		gsize length = strlen (base_url);

		if (g_ascii_strncasecmp (url, base_url, length) == 0) {
			res = g_uri_unescape_string (url + length, NULL);
			if (!res)
				res = g_strdup (url + length);
		}
	}

	if (!res) {
		res = g_uri_unescape_string (url, NULL);
		if (!res)
			res = g_strdup (url);
	}

	return res;
}

static void
composer_post_header_set_base_url (EComposerPostHeader *header)
{
	EAccount *account = header->priv->account;
	CamelURL *camel_url;
	gchar *url;

	if (account == NULL || account->source == NULL)
		return;

	url = account->source->url;
	if (url == NULL || *url == '\0')
		return;

	camel_url = camel_url_new (url, NULL);
	if (camel_url == NULL)
		return;

	url = camel_url_to_string (camel_url, CAMEL_URL_HIDE_ALL);
	camel_url_free (camel_url);

	g_free (header->priv->base_url);
	header->priv->base_url = url;
}

static GList *
composer_post_header_split_csv (const gchar *csv)
{
	GList *list = NULL;
	gchar **strv;
	guint length, ii;

	strv = g_strsplit (csv, ",", 0);
	length = g_strv_length (strv);

	for (ii = 0; ii < length; ii++)
		if (*g_strstrip (strv[ii]) != '\0')
			list = g_list_prepend (list, g_strdup (strv[ii]));

	g_strfreev (strv);

	return g_list_reverse (list);
}

static GObject *
composer_post_header_constructor (GType type,
                                  guint n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
	GObject *object;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (
		e_composer_post_header_parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	e_composer_header_set_title_tooltip (
		E_COMPOSER_HEADER (object),
		_("Click here to select folders to post to"));

	return object;
}

static void
composer_post_header_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT:
			e_composer_post_header_set_account (
				E_COMPOSER_POST_HEADER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_post_header_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT:
			g_value_set_object (
				value, e_composer_post_header_get_account (
				E_COMPOSER_POST_HEADER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_post_header_dispose (GObject *object)
{
	EComposerPostHeaderPrivate *priv;

	priv = E_COMPOSER_POST_HEADER_GET_PRIVATE (object);

	if (priv->account != NULL) {
		g_object_unref (priv->account);
		priv->account = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_composer_post_header_parent_class)->dispose (object);
}

static void
composer_post_header_finalize (GObject *object)
{
	EComposerPostHeaderPrivate *priv;

	priv = E_COMPOSER_POST_HEADER_GET_PRIVATE (object);

	g_free (priv->base_url);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_composer_post_header_parent_class)->finalize (object);
}

static void
composer_post_header_changed (EComposerHeader *header)
{
	EComposerPostHeaderPrivate *priv;

	priv = E_COMPOSER_POST_HEADER_GET_PRIVATE (header);

	priv->custom = TRUE;
}

static void
composer_post_header_clicked (EComposerHeader *header)
{
	EComposerPostHeaderPrivate *priv;

	priv = E_COMPOSER_POST_HEADER_GET_PRIVATE (header);

	priv->custom = FALSE;
}

static void
e_composer_post_header_class_init (EComposerPostHeaderClass *class)
{
	GObjectClass *object_class;
	EComposerHeaderClass *header_class;

	g_type_class_add_private (class, sizeof (EComposerPostHeaderPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = composer_post_header_constructor;
	object_class->set_property = composer_post_header_set_property;
	object_class->get_property = composer_post_header_get_property;
	object_class->dispose = composer_post_header_dispose;
	object_class->finalize = composer_post_header_finalize;

	header_class = E_COMPOSER_HEADER_CLASS (class);
	header_class->changed = composer_post_header_changed;
	header_class->clicked = composer_post_header_clicked;

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT,
		g_param_spec_object (
			"account",
			NULL,
			NULL,
			E_TYPE_ACCOUNT,
			G_PARAM_READWRITE));
}

static void
e_composer_post_header_init (EComposerPostHeader *header)
{
	header->priv = E_COMPOSER_POST_HEADER_GET_PRIVATE (header);
}

EComposerHeader *
e_composer_post_header_new (const gchar *label)
{
	return g_object_new (
		E_TYPE_COMPOSER_POST_HEADER,
		"label", label, "button", TRUE, NULL);
}

EAccount *
e_composer_post_header_get_account (EComposerPostHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_POST_HEADER (header), NULL);

	return header->priv->account;
}

void
e_composer_post_header_set_account (EComposerPostHeader *header,
                                    EAccount *account)
{
	GList *folders = NULL;

	g_return_if_fail (E_IS_COMPOSER_POST_HEADER (header));

	if (account != NULL) {
		g_return_if_fail (E_IS_ACCOUNT (account));
		g_object_ref (account);
	}

	if (!header->priv->custom)
		folders = e_composer_post_header_get_folders (header);

	if (header->priv->account != NULL)
		g_object_unref (header->priv->account);

	header->priv->account = account;
	composer_post_header_set_base_url (header);

	/* Make folders relative to the new account. */
	if (!header->priv->custom) {
		e_composer_post_header_set_folders (header, folders);
		g_list_free_full (folders, g_free);
	}

	g_object_notify (G_OBJECT (header), "account");
}

GList *
e_composer_post_header_get_folders (EComposerPostHeader *header)
{
	GList *folders, *iter;
	gchar *base_url;

	g_return_val_if_fail (E_IS_COMPOSER_POST_HEADER (header), NULL);

	folders = composer_post_header_split_csv (
		e_composer_text_header_get_text (
		E_COMPOSER_TEXT_HEADER (header)));

	base_url = header->priv->base_url;
	if (base_url == NULL)
		return folders;

	for (iter = folders; iter != NULL; iter = iter->next) {
		/* Convert relative folder names to absolute. */
		/* XXX Should use CamelURL for this. */
		if (strstr (iter->data, ":/") == NULL) {
			gchar *abs_url;

			abs_url = g_strconcat (base_url, iter->data, NULL);
			g_free (iter->data);
			iter->data = abs_url;
		}
	}

	return folders;
}

void
e_composer_post_header_set_folders (EComposerPostHeader *header,
                                    GList *folders)
{
	GList *iter;
	gint ii = 0;
	gchar **strv;
	gchar *text;
	gboolean custom_save;

	g_return_if_fail (E_IS_COMPOSER_POST_HEADER (header));

	strv = g_new0 (gchar *, g_list_length (folders) + 1);

	for (iter = folders; iter != NULL; iter = iter->next)
		strv[ii++] = composer_post_header_folder_name_to_string (
			header, iter->data);

	text = g_strjoinv (", ", strv);
	custom_save = header->priv->custom;
	e_composer_text_header_set_text (
		E_COMPOSER_TEXT_HEADER (header), text);
	header->priv->custom = custom_save;
	g_free (text);

	g_strfreev (strv);
}

void
e_composer_post_header_set_folders_base (EComposerPostHeader *header,
                                         const gchar *base_url,
                                         const gchar *folders)
{
	GList *list, *iter;

	list = composer_post_header_split_csv (folders);
	for (iter = list; iter != NULL; iter = iter->next) {
		gchar *abs_url;

		/* FIXME This doesn't handle all folder names properly. */
		abs_url = g_strdup_printf (
			"%s/%s", base_url, (gchar *) iter->data);
		g_free (iter->data);
		iter->data = abs_url;
	}

	e_composer_post_header_set_folders (header, list);
	g_list_free_full (list, g_free);
}

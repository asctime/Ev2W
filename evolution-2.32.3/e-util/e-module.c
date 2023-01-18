/*
 * e-module.c
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

/**
 * SECTION: e-module
 * @short_description: a module loader
 * @include: e-util/e-module.h
 **/

#include "e-module.h"

#include <glib/gi18n.h>

/* This is the symbol we call when loading a module. */
#define LOAD_SYMBOL	"e_module_load"

/* This is the symbol we call when unloading a module. */
#define UNLOAD_SYMBOL	"e_module_unload"

#define E_MODULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MODULE, EModulePrivate))

struct _EModulePrivate {
	GModule *module;
	gchar *filename;

	void (*load) (GTypeModule *type_module);
	void (*unload) (GTypeModule *type_module);
};

enum {
	PROP_0,
	PROP_FILENAME
};

G_DEFINE_TYPE (
	EModule,
	e_module,
	G_TYPE_TYPE_MODULE)

static void
module_set_filename (EModule *module,
                     const gchar *filename)
{
	g_return_if_fail (module->priv->filename == NULL);

	module->priv->filename = g_strdup (filename);
}

static void
module_set_property (GObject *object,
                     guint property_id,
                     const GValue *value,
                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILENAME:
			module_set_filename (
				E_MODULE (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
module_get_property (GObject *object,
                     guint property_id,
                     GValue *value,
                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILENAME:
			g_value_set_string (
				value, e_module_get_filename (
				E_MODULE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
module_finalize (GObject *object)
{
	EModulePrivate *priv;

	priv = E_MODULE_GET_PRIVATE (object);

	g_free (priv->filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_module_parent_class)->finalize (object);
}

static gboolean
module_load (GTypeModule *type_module)
{
	EModulePrivate *priv;
	gpointer symbol;

	priv = E_MODULE_GET_PRIVATE (type_module);

	g_return_val_if_fail (priv->filename != NULL, FALSE);
	priv->module = g_module_open (priv->filename, 0);

	if (priv->module == NULL)
		goto fail;

	if (!g_module_symbol (priv->module, LOAD_SYMBOL, &symbol))
		goto fail;

	priv->load = symbol;

	if (!g_module_symbol (priv->module, UNLOAD_SYMBOL, &symbol))
		goto fail;

	priv->unload = symbol;

	priv->load (type_module);

	return TRUE;

fail:
	g_warning ("%s", g_module_error ());

	if (priv->module != NULL)
		g_module_close (priv->module);

	return FALSE;
}

static void
module_unload (GTypeModule *type_module)
{
	EModulePrivate *priv;

	priv = E_MODULE_GET_PRIVATE (type_module);

	priv->unload (type_module);

	g_module_close (priv->module);
	priv->module = NULL;

	priv->load = NULL;
	priv->unload = NULL;
}

static void
e_module_class_init (EModuleClass *class)
{
	GObjectClass *object_class;
	GTypeModuleClass *type_module_class;

	g_type_class_add_private (class, sizeof (EModulePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = module_set_property;
	object_class->get_property = module_get_property;
	object_class->finalize = module_finalize;

	type_module_class = G_TYPE_MODULE_CLASS (class);
	type_module_class->load = module_load;
	type_module_class->unload = module_unload;

	/**
	 * EModule:filename
	 *
	 * The filename of the module.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_FILENAME,
		g_param_spec_string (
			"filename",
			"Filename",
			"The filename of the module",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_module_init (EModule *module)
{
	module->priv = E_MODULE_GET_PRIVATE (module);
}

/**
 * e_module_new:
 * @filename: filename of the shared library module
 *
 * Creates a new #EModule that will load the specific shared library
 * when in use.
 *
 * Returns: a new #EModule for @filename
 **/
EModule *
e_module_new (const gchar *filename)
{
	g_return_val_if_fail (filename != NULL, NULL);

	return g_object_new (E_TYPE_MODULE, "filename", filename, NULL);
}

/**
 * e_module_get_filename:
 * @module: an #EModule
 *
 * Returns the filename of the shared library for @module.  The
 * string is owned by @module and should not be modified or freed.
 *
 * Returns: the filename for @module
 **/
const gchar *
e_module_get_filename (EModule *module)
{
	g_return_val_if_fail (E_IS_MODULE (module), NULL);

	return module->priv->filename;
}

/**
 * e_module_load_all_in_directory:
 * @dirname: pathname for a directory containing modules to load
 *
 * Loads all the modules in the specified directory into memory.  If
 * you want to unload them (enabling on-demand loading) you must call
 * g_type_module_unuse() on all the modules.  Free the returned list
 * with g_list_free().
 *
 * Returns: a list of #EModules loaded from @dirname
 **/
GList *
e_module_load_all_in_directory (const gchar *dirname)
{
	GDir *dir;
	const gchar *basename;
	GList *loaded_modules = NULL;
	GError *error = NULL;

	g_return_val_if_fail (dirname != NULL, NULL);

	if (!g_module_supported ())
		return NULL;

	dir = g_dir_open (dirname, 0, &error);
	if (dir == NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return NULL;
	}

	while ((basename = g_dir_read_name (dir)) != NULL) {
		EModule *module;
		gchar *filename;

		if (!g_str_has_suffix (basename, "." G_MODULE_SUFFIX))
			continue;

		filename = g_build_filename (dirname, basename, NULL);

		module = e_module_new (filename);

		if (!g_type_module_use (G_TYPE_MODULE (module))) {
			g_printerr ("Failed to load module: %s\n", filename);
			g_object_unref (module);
			g_free (filename);
			continue;
		}

		g_free (filename);

		loaded_modules = g_list_prepend (loaded_modules, module);
	}

	g_dir_close (dir);

	return loaded_modules;
}

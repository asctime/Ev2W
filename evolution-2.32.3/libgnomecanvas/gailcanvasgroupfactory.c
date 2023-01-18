/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include "gailcanvasgroupfactory.h"
#include "gailcanvasgroup.h"

static void gail_canvas_group_factory_class_init (GailCanvasGroupFactoryClass *klass);

static AtkObject * gail_canvas_group_factory_create_accessible (GObject *obj);

static GType gail_canvas_group_factory_get_accessible_type (void);

GType
gail_canvas_group_factory_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (GailCanvasGroupFactoryClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) gail_canvas_group_factory_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (GailCanvasGroupFactory), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };
    type = g_type_register_static (
                           ATK_TYPE_OBJECT_FACTORY,
                           "GailCanvasGroupFactory" , &tinfo, 0);
  }

  return type;
}

static void
gail_canvas_group_factory_class_init (GailCanvasGroupFactoryClass *klass)
{
  AtkObjectFactoryClass *class = ATK_OBJECT_FACTORY_CLASS (klass);

  class->create_accessible = gail_canvas_group_factory_create_accessible;
  class->get_accessible_type = gail_canvas_group_factory_get_accessible_type;
}

static AtkObject*
gail_canvas_group_factory_create_accessible (GObject   *obj)
{
  return gail_canvas_group_new (obj);
}

static GType
gail_canvas_group_factory_get_accessible_type (void)
{
  return GAIL_TYPE_CANVAS_GROUP;
}

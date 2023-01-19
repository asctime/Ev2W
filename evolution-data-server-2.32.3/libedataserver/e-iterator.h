/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef __E_ITERATOR_H__
#define __E_ITERATOR_H__

#include <stdio.h>
#include <time.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define E_TYPE_ITERATOR            (e_iterator_get_type ())
#define E_ITERATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_ITERATOR, EIterator))
#define E_ITERATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_ITERATOR, EIteratorClass))
#define E_IS_ITERATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_ITERATOR))
#define E_IS_ITERATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_ITERATOR))
#define E_ITERATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_ITERATOR, EIteratorClass))

typedef struct _EIterator EIterator;
typedef struct _EIteratorClass EIteratorClass;

struct _EIterator {
	GObject object;
};

struct _EIteratorClass {
	GObjectClass parent_class;

	/* Signals */
	void         (*invalidate) (EIterator  *iterator);

	/* Virtual functions */
	gconstpointer  (*get)        (EIterator  *iterator);
	void         (*reset)      (EIterator  *iterator);
	void         (*last)       (EIterator  *iterator);
	gboolean     (*next)       (EIterator  *iterator);
	gboolean     (*prev)       (EIterator  *iterator);
	void         (*remove)     (EIterator  *iterator);
	void         (*insert)     (EIterator  *iterator,
				    gconstpointer object,
				    gboolean	before);
	void         (*set)        (EIterator  *iterator,
				    gconstpointer object);
	gboolean     (*is_valid)   (EIterator  *iterator);
};

const void    *e_iterator_get        (EIterator  *iterator);
void           e_iterator_reset      (EIterator  *iterator);
void           e_iterator_last       (EIterator  *iterator);
gboolean       e_iterator_next       (EIterator  *iterator);
gboolean       e_iterator_prev       (EIterator  *iterator);
void           e_iterator_delete     (EIterator  *iterator);
void           e_iterator_insert     (EIterator  *iterator,
				      gconstpointer object,
				      gboolean    before);
void           e_iterator_set        (EIterator  *iterator,
				      gconstpointer object);
gboolean       e_iterator_is_valid   (EIterator  *iterator);

void           e_iterator_invalidate (EIterator  *iterator);

/* Standard Glib function */
GType          e_iterator_get_type   (void);

G_END_DECLS

#endif /* __E_ITERATOR_H__ */

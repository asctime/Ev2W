/*
 * Evolution calendar - Week day picker widget
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
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <libgnomecanvas/gnome-canvas-text.h>
#include <e-util/e-util.h>
#include "weekday-picker.h"



#define PADDING 2

/* Private part of the WeekdayPicker structure */
struct _WeekdayPickerPrivate {
	/* Selected days; see weekday_picker_set_days() */
	guint8 day_mask;

	/* Blocked days; these cannot be modified */
	guint8 blocked_day_mask;

	/* Day that defines the start of the week; 0 = Sunday, ..., 6 = Saturday */
	gint week_start_day;

	/* Current keyboard focus day */
	gint focus_day;

	/* Metrics */
	gint font_ascent, font_descent;
	gint max_letter_width;

	/* Components */
	GnomeCanvasItem *boxes[7];
	GnomeCanvasItem *labels[7];
};



/* Signal IDs */
enum {
	CHANGED,
	LAST_SIGNAL
};

static guint wp_signals[LAST_SIGNAL];

G_DEFINE_TYPE (WeekdayPicker, weekday_picker, GNOME_TYPE_CANVAS)

static gchar *
get_day_text (gint day_index)
{
	GDateWeekday weekday;

	/* Convert from tm_wday to GDateWeekday. */
	weekday = (day_index == 0) ? G_DATE_SUNDAY : day_index;

	return g_strdup (e_get_weekday_name (weekday, TRUE));
}

static void
colorize_items (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;
	GdkColor *outline, *focus_outline;
	GdkColor *fill, *sel_fill;
	GdkColor *text_fill, *sel_text_fill;
	GtkStateType state;
	GtkStyle *style;
	gint i;

	priv = wp->priv;

	state = gtk_widget_get_state (GTK_WIDGET (wp));
	style = gtk_widget_get_style (GTK_WIDGET (wp));

	outline = &style->fg[state];
	focus_outline = &style->bg[state];

	fill = &style->base[state];
	text_fill = &style->fg[state];

	sel_fill = &style->bg[GTK_STATE_SELECTED];
	sel_text_fill = &style->fg[GTK_STATE_SELECTED];

	for (i = 0; i < 7; i++) {
		gint day;
		GdkColor *f, *t, *o;

		day = i + priv->week_start_day;
		if (day >= 7)
			day -= 7;

		if (priv->day_mask & (0x1 << day)) {
			f = sel_fill;
			t = sel_text_fill;
		} else {
			f = fill;
			t = text_fill;
		}

		if (day == priv->focus_day)
			o = focus_outline;
		else
			o = outline;

		gnome_canvas_item_set (priv->boxes[i],
				       "fill_color_gdk", f,
				       "outline_color_gdk", o,
				       NULL);

		gnome_canvas_item_set (priv->labels[i],
				       "fill_color_gdk", t,
				       NULL);
	}
}

static void
configure_items (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;
	GtkAllocation allocation;
	gint width, height;
	gint box_width;
	gint i;

	priv = wp->priv;

	gtk_widget_get_allocation (GTK_WIDGET (wp), &allocation);

	width = allocation.width;
	height = allocation.height;

	box_width = (width - 1) / 7;

	for (i = 0; i < 7; i++) {
		gchar *c;
		gint day;

		day = i + priv->week_start_day;
		if (day >= 7)
			day -= 7;

		gnome_canvas_item_set (priv->boxes[i],
				       "x1", (double) (i * box_width),
				       "y1", (double) 0,
				       "x2", (double) ((i + 1) * box_width),
				       "y2", (double) (height - 1),
				       "width_pixels", 0,
				       NULL);

		c = get_day_text (day);
		gnome_canvas_item_set (priv->labels[i],
				       "text", c,
				       "x", (double) (i * box_width) + box_width / 2.0,
				       "y", (double) (1 + PADDING),
				       "anchor", GTK_ANCHOR_N,
				       NULL);
		g_free (c);
	}

	colorize_items (wp);
}

static void
weekday_picker_destroy (GtkObject *object)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (object));

	wp = WEEKDAY_PICKER (object);
	priv = wp->priv;

	g_free (priv);
	wp->priv = NULL;

	/* Chain up to parent's destroy() method. */
	GTK_OBJECT_CLASS (weekday_picker_parent_class)->destroy (object);
}

static void
weekday_picker_realize (GtkWidget *widget)
{
	WeekdayPicker *wp;

	wp = WEEKDAY_PICKER (widget);

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (weekday_picker_parent_class)->realize (widget);

	configure_items (wp);
}

static void
weekday_picker_size_request (GtkWidget *widget,
                             GtkRequisition *requisition)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;

	wp = WEEKDAY_PICKER (widget);
	priv = wp->priv;

	requisition->width = (priv->max_letter_width + 2 * PADDING + 1) * 7 + 1;
	requisition->height = (priv->font_ascent + priv->font_descent + 2 * PADDING + 2);
}

static void
weekday_picker_size_allocate (GtkWidget *widget,
                              GtkAllocation *allocation)
{
	GtkWidgetClass *widget_class;
	WeekdayPicker *wp;

	wp = WEEKDAY_PICKER (widget);

	/* Chain up to parent's size_allocate() method. */
	widget_class = GTK_WIDGET_CLASS (weekday_picker_parent_class);
	widget_class->size_allocate (widget, allocation);

	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (wp), 0, 0,
		allocation->width, allocation->height);

	configure_items (wp);
}

static void
weekday_picker_style_set (GtkWidget *widget,
                          GtkStyle *previous_style)
{
	GtkWidgetClass *widget_class;
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;
	gint max_width;
	gint i;
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;

	wp = WEEKDAY_PICKER (widget);
	priv = wp->priv;

	/* Set up Pango prerequisites */
	font_desc = gtk_widget_get_style (widget)->font_desc;
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (
		pango_context, font_desc,
		pango_context_get_language (pango_context));
	layout = pango_layout_new (pango_context);

	priv->font_ascent =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics));
	priv->font_descent =
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	max_width = 0;

	for (i = 0; i < 7; i++) {
		gchar *c;
		gint w;

		c = get_day_text (i);
		pango_layout_set_text (layout, c, strlen (c));
		pango_layout_get_pixel_size (layout, &w, NULL);
		g_free (c);

		if (w > max_width)
			max_width = w;
	}

	priv->max_letter_width = max_width;

	configure_items (wp);
	g_object_unref (layout);
	pango_font_metrics_unref (font_metrics);

	/* Chain up to parent's style_set() method. */
	widget_class = GTK_WIDGET_CLASS (weekday_picker_parent_class);
	widget_class->style_set (widget, previous_style);
}

static gboolean
weekday_picker_focus (GtkWidget *widget,
                      GtkDirectionType direction)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (IS_WEEKDAY_PICKER (widget), FALSE);
	wp = WEEKDAY_PICKER (widget);
	priv = wp->priv;

	if (!gtk_widget_get_can_focus (widget))
		return FALSE;

	if (gtk_widget_has_focus (widget)) {
		priv->focus_day = -1;
		colorize_items (wp);
		return FALSE;
	}

	priv->focus_day = priv->week_start_day;
	gnome_canvas_item_grab_focus (priv->boxes[priv->focus_day]);
	colorize_items (wp);

	return TRUE;
}
static void
weekday_picker_class_init (WeekdayPickerClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = weekday_picker_destroy;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = weekday_picker_realize;
	widget_class->size_request = weekday_picker_size_request;
	widget_class->size_allocate = weekday_picker_size_allocate;
	widget_class->style_set = weekday_picker_style_set;
	widget_class->focus = weekday_picker_focus;

	class->changed = NULL;

	wp_signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (WeekdayPickerClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
day_clicked (WeekdayPicker *wp, gint index)
{
	WeekdayPickerPrivate *priv = wp->priv;
	guint8 day_mask;

	if (priv->blocked_day_mask & (0x1 << index))
		return;

	if (priv->day_mask & (0x1 << index))
		day_mask = priv->day_mask & ~(0x1 << index);
	else
		day_mask = priv->day_mask | (0x1 << index);

	weekday_picker_set_days (wp, day_mask);
}

static gint
handle_key_press_event (WeekdayPicker *wp, GdkEvent *event)
{
	WeekdayPickerPrivate *priv = wp->priv;
	guint keyval = event->key.keyval;

	if (priv->focus_day == -1)
		priv->focus_day = priv->week_start_day;

	switch (keyval) {
		case GDK_Up:
		case GDK_Right:
			priv->focus_day += 1;
			break;
		case GDK_Down:
		case GDK_Left:
			priv->focus_day -= 1;
			break;
		case GDK_space:
		case GDK_Return:
			day_clicked (wp, priv->focus_day);
			return TRUE;
		default:
			return FALSE;
	}

	if (priv->focus_day > 6)
		priv->focus_day = 0;
	if (priv->focus_day < 0)
		priv->focus_day = 6;

	colorize_items (wp);
	gnome_canvas_item_grab_focus (priv->boxes[priv->focus_day]);
	return TRUE;
}

/* Event handler for the day items */
static gint
day_event_cb (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;
	gint i;

	wp = WEEKDAY_PICKER (data);
	priv = wp->priv;

	if (event->type == GDK_KEY_PRESS)
		return handle_key_press_event(wp, event);

	if (!(event->type == GDK_BUTTON_PRESS && event->button.button == 1))
		return FALSE;

	/* Find which box was clicked */

	for (i = 0; i < 7; i++)
		if (priv->boxes[i] == item || priv->labels[i] == item)
			break;

	g_return_val_if_fail (i != 7, TRUE);

	i += priv->week_start_day;
	if (i >= 7)
		i -= 7;

	priv->focus_day = i;
	gnome_canvas_item_grab_focus (priv->boxes[i]);
	day_clicked (wp, i);
	return TRUE;
}

/* Creates the canvas items for the weekday picker.  The items are empty until
 * they are configured elsewhere.
 */
static void
create_items (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;
	GnomeCanvasGroup *parent;
	gint i;

	priv = wp->priv;

	parent = gnome_canvas_root (GNOME_CANVAS (wp));

	for (i = 0; i < 7; i++) {
		priv->boxes[i] = gnome_canvas_item_new (parent,
							GNOME_TYPE_CANVAS_RECT,
							NULL);
		g_signal_connect (priv->boxes[i], "event", G_CALLBACK (day_event_cb), wp);

		priv->labels[i] = gnome_canvas_item_new (parent,
							 GNOME_TYPE_CANVAS_TEXT,
							 NULL);
		g_signal_connect (priv->labels[i], "event", G_CALLBACK (day_event_cb), wp);
	}
}

static void
weekday_picker_init (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	priv = g_new0 (WeekdayPickerPrivate, 1);

	wp->priv = priv;

	create_items (wp);
	priv->focus_day = -1;
}

/**
 * weekday_picker_new:
 * @void:
 *
 * Creates a new weekday picker widget.
 *
 * Return value: A newly-created weekday picker.
 **/
GtkWidget *
weekday_picker_new (void)
{
	return g_object_new (TYPE_WEEKDAY_PICKER, NULL);
}

/**
 * weekday_picker_set_days:
 * @wp: A weekday picker.
 * @day_mask: Bitmask with the days to be selected.
 *
 * Sets the days that are selected in a weekday picker.  In the @day_mask,
 * Sunday is bit 0, Monday is bit 1, etc.
 **/
void
weekday_picker_set_days (WeekdayPicker *wp, guint8 day_mask)
{
	WeekdayPickerPrivate *priv;

	g_return_if_fail (wp != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (wp));

	priv = wp->priv;

	priv->day_mask = day_mask;
	colorize_items (wp);

	g_signal_emit (GTK_OBJECT (wp), wp_signals[CHANGED], 0);
}

/**
 * weekday_picker_get_days:
 * @wp: A weekday picker.
 *
 * Queries the days that are selected in a weekday picker.
 *
 * Return value: Bit mask of selected days.  Sunday is bit 0, Monday is bit 1,
 * etc.
 **/
guint8
weekday_picker_get_days (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	g_return_val_if_fail (wp != NULL, 0);
	g_return_val_if_fail (IS_WEEKDAY_PICKER (wp), 0);

	priv = wp->priv;
	return priv->day_mask;
}

/**
 * weekday_picker_set_blocked_days:
 * @wp: A weekday picker.
 * @blocked_day_mask: Bitmask with the days to be blocked.
 *
 * Sets the days that the weekday picker will prevent from being modified by the
 * user.  The @blocked_day_mask is specified in the same way as in
 * weekday_picker_set_days().
 **/
void
weekday_picker_set_blocked_days (WeekdayPicker *wp, guint8 blocked_day_mask)
{
	WeekdayPickerPrivate *priv;

	g_return_if_fail (wp != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (wp));

	priv = wp->priv;
	priv->blocked_day_mask = blocked_day_mask;
}

/**
 * weekday_picker_get_blocked_days:
 * @wp: A weekday picker.
 *
 * Queries the set of days that the weekday picker prevents from being modified
 * by the user.
 *
 * Return value: Bit mask of blocked days, with the same format as that returned
 * by weekday_picker_get_days().
 **/
guint
weekday_picker_get_blocked_days (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	g_return_val_if_fail (wp != NULL, 0);
	g_return_val_if_fail (IS_WEEKDAY_PICKER (wp), 0);

	priv = wp->priv;
	return priv->blocked_day_mask;
}

/**
 * weekday_picker_set_week_start_day:
 * @wp: A weekday picker.
 * @week_start_day: Index of the day that defines the start of the week; 0 is
 * Sunday, 1 is Monday, etc.
 *
 * Sets the day that defines the start of the week for a weekday picker.
 **/
void
weekday_picker_set_week_start_day (WeekdayPicker *wp, gint week_start_day)
{
	WeekdayPickerPrivate *priv;

	g_return_if_fail (wp != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (wp));
	g_return_if_fail (week_start_day >= 0 && week_start_day < 7);

	priv = wp->priv;
	priv->week_start_day = week_start_day;

	configure_items (wp);
}

/**
 * weekday_picker_get_week_start_day:
 * @wp: A weekday picker.
 *
 * Queries the day that defines the start of the week in a weekday picker.
 *
 * Return value: Index of the day that defines the start of the week.  See
 * weekday_picker_set_week_start_day() to see how this is represented.
 **/
gint
weekday_picker_get_week_start_day (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	g_return_val_if_fail (wp != NULL, -1);
	g_return_val_if_fail (IS_WEEKDAY_PICKER (wp), -1);

	priv = wp->priv;
	return priv->week_start_day;
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 * Author: Michael Zucchi <notzed@ximian.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camel-debug.h"

gint camel_verbose_debug;

static GHashTable *debug_table = NULL;

/**
 * camel_debug_init:
 * @void:
 *
 * Init camel debug.
 *
 * CAMEL_DEBUG is set to a comma separated list of modules to debug.
 * The modules can contain module-specific specifiers after a ':', or
 * just act as a wildcard for the module or even specifier.  e.g. 'imap'
 * for imap debug, or 'imap:folder' for imap folder debug.  Additionaly,
 * ':folder' can be used for a wildcard for any folder operations.
 **/
void
camel_debug_init (void)
{
	gchar *d;

	d = g_strdup(getenv("CAMEL_DEBUG"));
	if (d) {
		gchar *p;

		debug_table = g_hash_table_new(g_str_hash, g_str_equal);
		p = d;
		while (*p) {
			while (*p && *p != ',')
				p++;
			if (*p)
				*p++ = 0;
			g_hash_table_insert(debug_table, d, d);
			d = p;
		}

		if (g_hash_table_lookup(debug_table, "all"))
			camel_verbose_debug = 1;
	}
}

/**
 * camel_debug:
 * @mode:
 *
 * Check to see if a debug mode is activated.  @mode takes one of two forms,
 * a fully qualified 'module:target', or a wildcard 'module' name.  It
 * returns a boolean to indicate if the module or module and target is
 * currently activated for debug output.
 *
 * Returns:
 **/
gboolean camel_debug(const gchar *mode)
{
	if (camel_verbose_debug)
		return TRUE;

	if (debug_table) {
		gchar *colon;
		gchar *fallback;

		if (g_hash_table_lookup(debug_table, mode))
			return TRUE;

		/* Check for fully qualified debug */
		colon = strchr(mode, ':');
		if (colon) {
			fallback = g_alloca(strlen(mode)+1);
			strcpy(fallback, mode);
			colon = (colon-mode) + fallback;
			/* Now check 'module[:*]' */
			*colon = 0;
			if (g_hash_table_lookup(debug_table, fallback))
				return TRUE;
			/* Now check ':subsystem' */
			*colon = ':';
			if (g_hash_table_lookup(debug_table, colon))
				return TRUE;
		}
	}

	return FALSE;
}

static GStaticMutex debug_lock = G_STATIC_MUTEX_INIT;
/**
 * camel_debug_start:
 * @mode:
 *
 * Start debug output for a given mode, used to make sure debug output
 * is output atomically and not interspersed with unrelated stuff.
 *
 * Returns: Returns true if mode is set, and in which case, you must
 * call debug_end when finished any screen output.
 **/
gboolean
camel_debug_start(const gchar *mode)
{
	if (camel_debug(mode)) {
		g_static_mutex_lock (&debug_lock);
		printf ("Thread %p >\n", g_thread_self());
		return TRUE;
	}

	return FALSE;
}

/**
 * camel_debug_end:
 *
 * Call this when you're done with your debug output.  If and only if
 * you called camel_debug_start, and if it returns TRUE.
 **/
void
camel_debug_end(void)
{
	printf ("< %p >\n", g_thread_self());
	g_static_mutex_unlock (&debug_lock);
}

#if 0
#include <sys/debugreg.h>

static unsigned
i386_length_and_rw_bits (gint len, enum target_hw_bp_type type)
{
  unsigned rw;

  switch (type)
    {
      case hw_execute:
        rw = DR_RW_EXECUTE;
        break;
      case hw_write:
        rw = DR_RW_WRITE;
        break;
      case hw_read:      /* x86 doesn't support data-read watchpoints */
      case hw_access:
        rw = DR_RW_READ;
        break;
#if 0
      case hw_io_access: /* not yet supported */
        rw = DR_RW_IORW;
        break;
#endif
      default:
        internal_error (__FILE__, __LINE__, "\
Invalid hw breakpoint type %d in i386_length_and_rw_bits.\n", (gint)type);
    }

  switch (len)
    {
      case 1:
        return (DR_LEN_1 | rw);
      case 2:
        return (DR_LEN_2 | rw);
      case 4:
        return (DR_LEN_4 | rw);
      case 8:
        if (TARGET_HAS_DR_LEN_8)
          return (DR_LEN_8 | rw);
      default:
        internal_error (__FILE__, __LINE__, "\
Invalid hw breakpoint length %d in i386_length_and_rw_bits.\n", len);
    }
}

#define I386_DR_SET_RW_LEN(i,rwlen) \
  do { \
    dr_control_mirror &= ~(0x0f << (DR_CONTROL_SHIFT+DR_CONTROL_SIZE*(i)));   \
    dr_control_mirror |= ((rwlen) << (DR_CONTROL_SHIFT+DR_CONTROL_SIZE*(i))); \
  } while (0)

#define I386_DR_LOCAL_ENABLE(i) \
  dr_control_mirror |= (1 << (DR_LOCAL_ENABLE_SHIFT + DR_ENABLE_SIZE * (i)))

#define set_dr(regnum, val) \
		__asm__("movl %0,%%db" #regnum  \
			: /* no output */ \
			:"r" (val))

#define get_dr(regnum, val) \
		__asm__("movl %%db" #regnum ", %0"  \
			:"=r" (val))

/* fine idea, but it doesn't work, crashes in get_dr :-/ */
void
camel_debug_hwatch(gint wp, gpointer addr)
{
     guint32 control, rw;

     g_assert(wp <= DR_LASTADDR);
     g_assert(sizeof(addr) == 4);

     get_dr(7, control);
     /* set watch mode + size */
     rw = DR_RW_WRITE | DR_LEN_4;
     control &= ~(((1<<DR_CONTROL_SIZE)-1) << (DR_CONTROL_SHIFT+DR_CONTROL_SIZE * wp));
     control |= rw << (DR_CONTROL_SHIFT + DR_CONTROL_SIZE*wp);
     /* set watch enable */
     control |=  ( 1<< (DR_LOCAL_ENABLE_SHIFT + DR_ENABLE_SIZE * wp));
     control |= DR_LOCAL_SLOWDOWN;
     control &= ~DR_CONTROL_RESERVED;

     switch (wp) {
     case 0:
	     set_dr(0, addr);
	     break;
     case 1:
	     set_dr(1, addr);
	     break;
     case 2:
	     set_dr(2, addr);
	     break;
     case 3:
	     set_dr(3, addr);
	     break;
     }
     set_dr(7, control);
}

#endif

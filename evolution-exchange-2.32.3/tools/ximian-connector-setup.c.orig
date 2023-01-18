/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2003, 2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libedataserverui/e-passwords.h>

#include "exchange-autoconfig-wizard.h"

#ifdef G_OS_WIN32
#include <libedataserver/e-data-server-util.h>
# define WIN32_LEAN_AND_MEAN
# ifdef DATADIR
#  undef DATADIR
# endif
# ifdef _WIN32_WINNT
#  undef _WIN32_WINNT
# endif
# define _WIN32_WINNT 0x0601
# include <windows.h>
# include <conio.h>
# include <io.h>
# ifndef PROCESS_DEP_ENABLE
#  define PROCESS_DEP_ENABLE 0x00000001
# endif
# ifndef PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION
#  define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x00000002
# endif
#endif


gint
main (gint argc, gchar **argv)
{
	GError *error = NULL;

#ifdef G_OS_WIN32
	/* Reduce risks */
	{
		typedef BOOL (WINAPI *t_SetDllDirectoryA) (LPCSTR lpPathName);
		t_SetDllDirectoryA p_SetDllDirectoryA;

		p_SetDllDirectoryA = GetProcAddress (GetModuleHandle ("kernel32.dll"), "SetDllDirectoryA");
		if (p_SetDllDirectoryA)
			(*p_SetDllDirectoryA) ("");
	}
#ifndef _WIN64
	{
		typedef BOOL (WINAPI *t_SetProcessDEPPolicy) (DWORD dwFlags);
		t_SetProcessDEPPolicy p_SetProcessDEPPolicy;

		p_SetProcessDEPPolicy = GetProcAddress (GetModuleHandle ("kernel32.dll"), "SetProcessDEPPolicy");
		if (p_SetProcessDEPPolicy)
			(*p_SetProcessDEPPolicy) (PROCESS_DEP_ENABLE|PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
	}
#endif
	{
		gchar *localedir;

		/* We assume evolution-exchange is installed in the
		 * same run-time prefix as evolution-data-server.
		 */
		localedir = e_util_replace_prefix (PREFIX, e_util_get_cp_prefix (), CONNECTOR_LOCALEDIR);
		bindtextdomain (GETTEXT_PACKAGE, localedir);
	}
#else
	bindtextdomain (GETTEXT_PACKAGE, CONNECTOR_LOCALEDIR);
#endif
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();
	g_thread_init (NULL);
	gtk_init_with_args (&argc, &argv, NULL, NULL, (gchar *)GETTEXT_PACKAGE, &error);

	if (error != NULL) {
		g_printerr ("Failed initialize application, %s\n", error->message);
		g_error_free (error);
		return (1);
	}

	e_passwords_init ();

	exchange_autoconfig_assistant_run ();

	e_passwords_shutdown ();

	return 0;
}

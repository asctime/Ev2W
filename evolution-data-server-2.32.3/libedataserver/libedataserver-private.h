/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * libedataserver-private.h: For e-d-s (not just libedataserver)
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors:
 *   Tor Lillqvist <tml@novell.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _LIBEDATASERVER_PRIVATE_H_
#define _LIBEDATASERVER_PRIVATE_H_

#ifdef G_OS_WIN32

const gchar *_libedataserver_get_extensiondir (void) G_GNUC_CONST;
const gchar *_libedataserver_get_imagesdir (void) G_GNUC_CONST;
const gchar *_libedataserver_get_ui_uidir (void) G_GNUC_CONST;

#undef E_DATA_SERVER_EXTENSIONDIR
#define E_DATA_SERVER_EXTENSIONDIR _libedataserver_get_extensiondir ()

#undef E_DATA_SERVER_IMAGESDIR
#define E_DATA_SERVER_IMAGESDIR _libedataserver_get_imagesdir ()

#undef E_DATA_SERVER_UI_UIDIR
#define E_DATA_SERVER_UI_UIDIR _libedataserver_get_ui_uidir ()

#endif	/* G_OS_WIN32 */

#endif	/* _LIBEDATASERVER_PRIVATE_H_ */

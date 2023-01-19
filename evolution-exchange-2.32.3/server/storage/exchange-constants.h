/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef __EXCHANGE_CONSTANTS_H__
#define __EXCHANGE_CONSTANTS_H__

enum {
	UNSUPPORTED_MODE = 0,
        OFFLINE_MODE,
        ONLINE_MODE
};

typedef enum {
	EXCHANGE_CALENDAR_FOLDER,
	EXCHANGE_TASKS_FOLDER,
	EXCHANGE_CONTACTS_FOLDER
}FolderType;

/* This flag indicates that its other user's folder. We encode this flag
   with the FolderType to identify the same. We are doing this to
   avoid ABI/API break. */
#define FORIEGN_FOLDER_FLAG 0x0100

#endif

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef E2K_CAL_UTILS_H
#define E2K_CAL_UTILS_H

#include <libical/ical.h>
#include <e2k-types.h>

G_BEGIN_DECLS

gchar                *e2k_timestamp_from_icaltime (struct icaltimetype itt);
struct icaltimetype  e2k_timestamp_to_icaltime   (const gchar *timestamp);

G_END_DECLS

#endif

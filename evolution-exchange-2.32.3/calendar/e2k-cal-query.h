/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef E2K_CAL_QUERY_H
#define E2K_CAL_QUERY_H

#include <e2k-restriction.h>
#include "e-cal-backend-exchange.h"

G_BEGIN_DECLS

E2kRestriction * e2k_cal_query_to_restriction (ECalBackendExchange *cbex,
					       const gchar          *sexp);

G_END_DECLS

#endif

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2001-2004 Novell, Inc.
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

#include <string.h>

#include "e2k-cal-query.h"
#include <libedata-cal/e-cal-backend-sexp.h>
#include <libedataserver/e-sexp.h>

#include <e2k-propnames.h>
#include <e2k-utils.h>

/* E-Sexp functions */

static E2kRestriction **
rns_array (ESExp *esexp, gint argc, ESExpResult **argv)
{
	E2kRestriction **rns;
	gint i;

	rns = g_new (E2kRestriction *, argc);
	for (i = 0; i < argc; i++) {
		if (argv[i]->type != ESEXP_RES_UNDEFINED) {
			while (i--)
				e2k_restriction_unref (rns[i]);
			g_free (rns);
			e_sexp_fatal_error (esexp, "bad expression list");
			return NULL;
		}

		rns[i] = (E2kRestriction *)argv[i]->value.string;
	}

	return rns;
}

static ESExpResult *
func_and (ESExp *esexp, gint argc, ESExpResult **argv, gpointer data)
{
	ESExpResult *result;
	E2kRestriction **rns;

	rns = rns_array (esexp, argc, argv);

	result = e_sexp_result_new (esexp, ESEXP_RES_UNDEFINED);
	result->value.string = (gchar *)
		e2k_restriction_and (argc, rns, TRUE);
	g_free (rns);

	return result;
}

static ESExpResult *
func_or (ESExp *esexp, gint argc, ESExpResult **argv, gpointer data)
{
	ESExpResult *result;
	E2kRestriction **rns;

	rns = rns_array (esexp, argc, argv);

	result = e_sexp_result_new (esexp, ESEXP_RES_UNDEFINED);
	result->value.string = (gchar *)
		e2k_restriction_or (argc, rns, TRUE);
	g_free (rns);

	return result;
}

static ESExpResult *
func_not (ESExp *esexp, gint argc, ESExpResult **argv, gpointer data)
{
	ESExpResult *result;

	if (argc != 1 || (argv[0]->type != ESEXP_RES_UNDEFINED)) {
		e_sexp_fatal_error (esexp, "'not' expects an expression");
		return NULL;
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_UNDEFINED);
	result->value.string = (gchar *)
		e2k_restriction_not ((E2kRestriction *)argv[0]->value.string,
				     TRUE);

	return result;
}

/* (occur-in-time-range? START END)
 *
 * START - time_t, start of the time range
 * END - time_t, end of the time range
 *
 * Returns a boolean indicating whether the component has any occurrences in the
 * specified time range.
 */
static ESExpResult *
func_occur_in_time_range (ESExp *esexp, gint argc, ESExpResult **argv, gpointer user_data)
{
	ECalBackend *backend = user_data;
	gchar *start, *end;
	ESExpResult *result;

	/* check argument types */
	if (argc != 2) {
		e_sexp_fatal_error (esexp, "occur-in-time-range? expects 2 arguments");
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_TIME) {
		e_sexp_fatal_error (esexp, "occur-in-time-range? expects argument 1 "
				    "to be a time_t");
		return NULL;
	}

	if (argv[1]->type != ESEXP_RES_TIME) {
		e_sexp_fatal_error (esexp, "occur-in-time-range? expects argument 2 "
				    "to be a time_t");
		return NULL;
	}

	start = e2k_make_timestamp (argv[0]->value.time);
	end = e2k_make_timestamp (argv[1]->value.time);

	result = e_sexp_result_new (esexp, ESEXP_RES_UNDEFINED);
	switch (e_cal_backend_get_kind (backend)) {
	case ICAL_VEVENT_COMPONENT:
		result->value.string = (gchar *)
			e2k_restriction_andv (
				e2k_restriction_prop_date (
					E2K_PR_CALENDAR_DTSTART,
					E2K_RELOP_GE, start),
				e2k_restriction_prop_date (
					E2K_PR_CALENDAR_DTEND,
					E2K_RELOP_LE, end),
				NULL);
		break;

	case ICAL_VTODO_COMPONENT:
		result->value.string = (gchar *)
			e2k_restriction_andv (
				e2k_restriction_prop_date (
					E2K_PR_MAPI_COMMON_START,
					E2K_RELOP_GE, start),
				e2k_restriction_prop_date (
					E2K_PR_MAPI_COMMON_END,
					E2K_RELOP_LE, end),
				NULL);
		break;

	default:
		break;
	}

	g_free (start);
	g_free (end);

	return result;
}

/* (contains? FIELD STR)
 *
 * FIELD - string, name of field to match (any, comment, description, summary)
 * STR - string, match string
 *
 * Returns a boolean indicating whether the specified field contains the
 * specified string.
 */
static ESExpResult *
func_contains (ESExp *esexp, gint argc, ESExpResult **argv, gpointer user_data)
{
	ESExpResult *result;
	E2kRestriction *rn;
	const gchar *field;
	const gchar *str;

	/* check argument types */
	if (argc != 2) {
		e_sexp_fatal_error (esexp, "contains? expects 2 arguments");
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_STRING) {
		e_sexp_fatal_error (esexp, "contains? expects argument 1 "
				    "to be a string");
		return NULL;
	}
	field = argv[0]->value.string;

	if (argv[1]->type != ESEXP_RES_STRING) {
		e_sexp_fatal_error (esexp, "contains? expects argument 2 to be a string");
		return NULL;
	}
	str = argv[1]->value.string;

	if (!g_ascii_strcasecmp (field, "summary")) {
		rn = e2k_restriction_content (E2K_PR_HTTPMAIL_SUBJECT,
					      E2K_FL_SUBSTRING,
					      str);
	} else if (!g_ascii_strcasecmp (field, "description") ||
		   !g_ascii_strcasecmp (field, "comment")) {
		/* We can't search comment, so we just pretend it's
		 * also the description.
		 */
		rn = e2k_restriction_content (E2K_PR_HTTPMAIL_TEXT_DESCRIPTION,
					      E2K_FL_SUBSTRING,
					      str);
	} else if (!g_ascii_strcasecmp (field, "any")) {
		rn = e2k_restriction_orv (
			e2k_restriction_content (E2K_PR_HTTPMAIL_SUBJECT,
						 E2K_FL_SUBSTRING,
						 str),
			e2k_restriction_content (E2K_PR_HTTPMAIL_TEXT_DESCRIPTION,
						 E2K_FL_SUBSTRING,
						 str),
			NULL);
	} else {
		e_sexp_fatal_error (esexp, "bad field name in contains?");
		return NULL;
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_UNDEFINED);
	result->value.string = (gchar *)rn;

	return result;
}

/* (has-alarms?)
 *
 * A boolean value for components that have/dont have alarms.
 *
 * Returns: a boolean indicating whether the component has alarms or not.
 */
static ESExpResult *
func_has_alarms (ESExp *esexp, gint argc, ESExpResult **argv, gpointer user_data)
{
	ESExpResult *result;

	/* check argument types */
	if (argc != 0) {
		e_sexp_fatal_error (esexp, "has-alarms? expects 0 arguments");
		return NULL;
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_UNDEFINED);
	result->value.string = (gchar *)
		e2k_restriction_prop_bool (E2K_PR_MAPI_REMINDER_SET,
					   E2K_RELOP_EQ, TRUE);
	return result;
}

/* (has-categories? STR+)
 * (has-categories? #f)
 *
 * STR - At least one string specifying a category
 * Or you can specify a single #f (boolean false) value for components
 * that have no categories assigned to them ("unfiled").
 *
 * Returns a boolean indicating whether the component has all the specified
 * categories.
 */
static ESExpResult *
func_has_categories (ESExp *esexp, gint argc, ESExpResult **argv, gpointer user_data)
{
	ESExpResult *result;

	/* FIXME: implement */

	result = e_sexp_result_new (esexp, ESEXP_RES_UNDEFINED);
	result->value.string = NULL;
	return result;
}

/* (is-completed?)
 *
 * Returns a boolean indicating whether the component is completed (i.e. has
 * a COMPLETED property. This is really only useful for TODO components.
 */
static ESExpResult *
func_is_completed (ESExp *esexp, gint argc, ESExpResult **argv, gpointer user_data)
{
	ECalBackend *backend = user_data;
	ESExpResult *result;

	if (e_cal_backend_get_kind (backend) != ICAL_VTODO_COMPONENT) {
		e_sexp_fatal_error (esexp, "completed-before? is only meaningful for task folders");
		return NULL;
	}

	/* check argument types */
	if (argc != 0) {
		e_sexp_fatal_error (esexp, "is-completed? expects 0 arguments");
		return NULL;
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_UNDEFINED);
	result->value.string = (gchar *)
		e2k_restriction_prop_bool (E2K_PR_OUTLOOK_TASK_IS_DONE,
					   E2K_RELOP_EQ, TRUE);
	return result;
}

/* (completed-before? TIME)
 *
 * TIME - time_t
 *
 * Returns a boolean indicating whether the component was completed on
 * or before the given time (i.e. it checks the COMPLETED property).
 * This is really only useful for TODO components.
 */
static ESExpResult *
func_completed_before (ESExp *esexp, gint argc, ESExpResult **argv, gpointer user_data)
{
	ECalBackend *backend = user_data;
	ESExpResult *result;
	gchar *before_time;

	if (e_cal_backend_get_kind (backend) != ICAL_VTODO_COMPONENT) {
		e_sexp_fatal_error (esexp, "completed-before? is only meaningful for task folders");
		return NULL;
	}

	/* check argument types */
	if (argc != 1) {
		e_sexp_fatal_error (esexp, "completed-before? expects 1 argument");
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_TIME) {
		e_sexp_fatal_error (esexp, "completed-before? expects argument 1 "
				    "to be a time_t");
		return NULL;
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_UNDEFINED);

	before_time = e2k_make_timestamp (argv[0]->value.time);
	result->value.string = (gchar *)
		e2k_restriction_prop_date (E2K_PR_OUTLOOK_TASK_DONE_DT,
					   E2K_RELOP_LT, before_time);
	g_free (before_time);

	return result;
}

static struct {
	const gchar *name;
	ESExpFunc *func;
} functions[] = {
	{ "and", func_and },
	{ "or", func_or },
	{ "not", func_not },

	/* Time-related functions */
	{ "time-now", e_cal_backend_sexp_func_time_now },
	{ "make-time", e_cal_backend_sexp_func_make_time },
	{ "time-add-day", e_cal_backend_sexp_func_time_add_day },
	{ "time-day-begin", e_cal_backend_sexp_func_time_day_begin },
	{ "time-day-end", e_cal_backend_sexp_func_time_day_end },

	/* Component-related functions */
	{ "occur-in-time-range?", func_occur_in_time_range },
	{ "contains?", func_contains },
	{ "has-alarms?", func_has_alarms },
	{ "has-categories?", func_has_categories },
	{ "is-completed?", func_is_completed },
	{ "completed-before?", func_completed_before }
};

E2kRestriction *
e2k_cal_query_to_restriction (ECalBackendExchange *cbex,
			      const gchar *sexp)
{
	E2kRestriction *rn;
	ESExp *esexp;
	ESExpResult *result;
	gint i;

	g_return_val_if_fail (E_IS_CAL_BACKEND_EXCHANGE (cbex), NULL);
	g_return_val_if_fail (sexp != NULL, NULL);

	esexp = e_sexp_new ();
	for (i = 0; i < G_N_ELEMENTS (functions); i++)
		e_sexp_add_function (esexp, 0, (gchar *) functions[i].name, functions[i].func, NULL);

	e_sexp_input_text (esexp, sexp, strlen (sexp));
	if (e_sexp_parse (esexp) == -1) {
		e_sexp_unref (esexp);
		return NULL;
	}

	result = e_sexp_eval (esexp);
	if (result && result->type == ESEXP_RES_UNDEFINED)
		rn = (E2kRestriction *)result->value.string;
	else
		rn = NULL;

	e_sexp_result_free (esexp, result);
	e_sexp_unref (esexp);

	return rn;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <stdlib.h>
#include <libecal/e-cal.h>

gint
main (gint argc, gchar **argv)
{
	ECal *ecal;

	g_type_init ();

	if (argc < 2) {
		printf ("usage: test-recur <uid>\n");
		exit (0);
	}

	ecal = e_cal_new_from_uri (argv[1], E_CAL_SOURCE_TYPE_EVENT);

	if (!e_cal_open (ecal, TRUE, NULL)) {
		printf ("failed to open calendar\n");
		exit(0);
	}

	g_object_unref (ecal);

	return 0;
}

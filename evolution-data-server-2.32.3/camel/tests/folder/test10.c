/* threaded folder testing */

#include <string.h>

#include "camel-test.h"
#include "camel-test-provider.h"
#include "folders.h"
#include "messages.h"
#include "session.h"

#define MAX_LOOP (10000)
#define MAX_THREADS (5)

#define d(x)

static const gchar *local_drivers[] = { "local" };
static const gchar *local_providers[] = {
	"mbox",
	"mh",
	"maildir"
};

static gchar *path;
static CamelSession *session;
static gint testid;

static gpointer
worker(gpointer d)
{
	gint i;
	CamelStore *store;
	CamelFolder *folder;

	for (i=0;i<MAX_LOOP;i++) {
		store = camel_session_get_store(session, path, NULL);
		folder = camel_store_get_folder(store, "testbox", CAMEL_STORE_FOLDER_CREATE, NULL);
		if (testid == 0) {
			g_object_unref (folder);
			g_object_unref (store);
		} else {
			g_object_unref (store);
			g_object_unref (folder);
		}
	}

	return NULL;
}

gint
main(gint argc, gchar **argv)
{
	gint i, j;
	GThread *threads[MAX_THREADS];

	camel_test_init(argc, argv);
	camel_test_provider_init(1, local_drivers);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_test_session_new ("/tmp/camel-test");

	for (testid=0;testid<2;testid++) {
		if (testid == 0)
			camel_test_start("store and folder bag torture test, stacked references");
		else
			camel_test_start("store and folder bag torture test, unstacked references");

		for (j = 0; j < G_N_ELEMENTS (local_providers); j++) {

			camel_test_push("provider %s", local_providers[j]);
			path = g_strdup_printf("%s:///tmp/camel-test/%s", local_providers[j], local_providers[j]);

			for (i = 0; i < MAX_THREADS; i++) {
				GError *error = NULL;

				threads[i] = g_thread_create (worker, NULL, TRUE, &error);
				if (error) {
					fprintf (stderr, "%s: Failed to create a thread: %s\n", G_STRFUNC, error->message);
					g_error_free (error);
				}
			}

			for (i = 0; i < MAX_THREADS; i++) {
				if (threads[i])
					g_thread_join (threads[i]);
			}

			test_free(path);

			camel_test_pull();
		}

		camel_test_end();
	}

	g_object_unref (session);

	return 0;
}

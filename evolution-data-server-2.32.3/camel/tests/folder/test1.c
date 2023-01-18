/* store testing */

#include "camel-test.h"
#include "camel-test-provider.h"
#include "folders.h"
#include "session.h"

static const gchar *local_drivers[] = {
	"local"
};

static const gchar *local_providers[] = {
	"mbox",
	"mh",
	"maildir"
};

gint main(gint argc, gchar **argv)
{
	CamelSession *session;
	gint i;
	gchar *path;

	camel_test_init(argc, argv);
	camel_test_provider_init(1, local_drivers);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_test_session_new ("/tmp/camel-test");

	/* todo: cross-check everything with folder_info checks as well */
	/* todo: subscriptions? */
	/* todo: work out how to do imap/pop/nntp tests */
	for (i = 0; i < G_N_ELEMENTS (local_providers); i++) {
		path = g_strdup_printf("%s:///tmp/camel-test/%s", local_providers[i], local_providers[i]);

		test_folder_basic(session, path, TRUE, FALSE);

		g_free(path);
	}

	g_object_unref (session);

	return 0;
}

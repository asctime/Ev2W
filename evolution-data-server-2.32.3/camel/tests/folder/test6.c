/* folder testing */

#include "camel-test.h"
#include "camel-test-provider.h"
#include "folders.h"
#include "session.h"

static const gchar *imap_drivers[] = { "imap4" };
static const gchar *remote_providers[] = {
	"IMAP_TEST_URL",
};

gint main(gint argc, gchar **argv)
{
	CamelSession *session;
	gint i;
	gchar *path;

	camel_test_init(argc, argv);
	camel_test_provider_init(1, imap_drivers);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_test_session_new ("/tmp/camel-test");

	for (i = 0; i < G_N_ELEMENTS (remote_providers); i++) {
		path = getenv(remote_providers[i]);

		if (path == NULL) {
			printf("Aborted (ignored).\n");
			printf("Set '%s', to re-run test.\n", remote_providers[i]);
			/* tells make check to ignore us in the total count */
			_exit(77);
		}
		/*camel_test_nonfatal("The IMAP code is just rooted");*/
		test_folder_message_ops(session, path, FALSE, "testbox");
		test_folder_message_ops(session, path, FALSE, "INBOX");
		/*camel_test_fatal();*/
	}

	check_unref(session, 1);

	return 0;
}

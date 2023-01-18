/* folder testing */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "camel-test.h"
#include "camel-test-provider.h"
#include "messages.h"
#include "folders.h"
#include "session.h"

static const gchar *local_drivers[] = { "local" };

static const gchar *stores[] = {
	"mbox:///tmp/camel-test/mbox",
	"mh:///tmp/camel-test/mh",
	"maildir:///tmp/camel-test/maildir"
};

gint main(gint argc, gchar **argv)
{
	CamelSession *session;
	gint i;

	camel_test_init(argc, argv);
	camel_test_provider_init(1, local_drivers);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_test_session_new ("/tmp/camel-test");

	/* we iterate over all stores we want to test, with indexing or indexing turned on or off */
	for (i = 0; i < G_N_ELEMENTS (stores); i++) {
		gchar *name = stores[i];

		test_folder_message_ops(session, name, TRUE, "testbox");
	}

	/* create a pseudo-spool file, and check that */
	creat("/tmp/camel-test/testbox", 0600);
	test_folder_message_ops(session, "spool:///tmp/camel-test/testbox", TRUE, "INBOX");

	check_unref(session, 1);

	return 0;
}

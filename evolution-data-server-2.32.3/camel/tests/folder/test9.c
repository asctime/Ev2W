/* folder/index testing */

#include <string.h>

#include "camel-test.h"
#include "camel-test-provider.h"
#include "messages.h"
#include "folders.h"
#include "session.h"

static const gchar *local_drivers[] = { "local" };

struct {
	const gchar *name;
	CamelFolder *folder;
} mailboxes[] = {
	{ "INBOX", NULL },
	{ "folder1", NULL },
	{ "folder2", NULL },
	{ "folder3", NULL },
	{ "folder4", NULL },
};

struct {
	const gchar *name, *match, *action;
} rules[] = {
	{ "empty1", "(match-all (header-contains \"Frobnitz\"))", "(copy-to \"folder1\")" },
	{ "empty2", "(header-contains \"Frobnitz\")", "(copy-to \"folder2\")" },
	{ "count11", "(and (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"subject\"))", "(move-to \"folder3\")" },
	{ "empty3", "(and (header-contains \"subject\" \"Test1\") (header-contains \"subject\" \"subject\"))", "(move-to \"folder4\")" },
	{ "count1", "(body-contains \"data50\")", "(copy-to \"folder1\")" },
	{ "stop", "(body-contains \"data2\")", "(stop)" },
	{ "notreached1", "(body-contains \"data2\")", "(move-to \"folder2\")" },
	{ "count1", "(body-contains \"data3\")", "(move-to \"folder2\")" },
	{ "ustrcasecmp", "(header-matches \"Subject\" \"Test0 message100 subject\")", "(copy-to \"folder2\")" },
};

/* broken match rules */
struct {
	const gchar *name, *match, *action;
} brokens[] = {
	{ "count1", "(body-contains data50)", "(copy-to \"folder1\")" }, /* non string argument */
	{ "count1", "(body-contains-stuff \"data3\")", "(move-to-folder \"folder2\")" }, /* invalid function */
	{ "count1", "(or (body-contains \"data3\") (foo))", "(move-to-folder \"folder2\")" }, /* invalid function */
	{ "count1", "(or (body-contains \"data3\") (foo)", "(move-to-folder \"folder2\")" }, /* missing ) */
	{ "count1", "(and body-contains \"data3\") (foo)", "(move-to-folder \"folder2\")" }, /* missing ( */
	{ "count1", "body-contains \"data3\")", "(move-to-folder \"folder2\")" }, /* missing ( */
	{ "count1", "body-contains \"data3\"", "(move-to-folder \"folder2\")" }, /* missing ( ) */
	{ "count1", "(body-contains \"data3\" ())", "(move-to-folder \"folder2\")" }, /* extra () */
	{ "count1", "()", "(move-to-folder \"folder2\")" }, /* invalid () */
	{ "count1", "", "(move-to-folder \"folder2\")" }, /* empty */
};

/* broken action rules */
struct {
	const gchar *name, *match, *action;
} brokena[] = {
	{ "a", "(body-contains \"data2\")", "(body-contains \"help\")" }, /* rule in action */
	{ "a", "(body-contains \"data2\")", "(move-to-folder-name \"folder2\")" }, /* unknown function */
	{ "a", "(body-contains \"data2\")", "(or (move-to-folder \"folder2\")" }, /* missing ) */
	{ "a", "(body-contains \"data2\")", "(or move-to-folder \"folder2\"))" }, /* missing ( */
	{ "a", "(body-contains \"data2\")", "move-to-folder \"folder2\")" }, /* missing ( */
	{ "a", "(body-contains \"data2\")", "(move-to-folder \"folder2\" ())" }, /* invalid () */
	{ "a", "(body-contains \"data2\")", "()" }, /* invalid () */
	{ "a", "(body-contains \"data2\")", "" }, /* empty */
};

static CamelFolder *
get_folder (CamelFilterDriver *d,
            const gchar *uri,
            gpointer data,
            GError **error)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (mailboxes); i++)
		if (!strcmp(mailboxes[i].name, uri)) {
			return g_object_ref (mailboxes[i].folder);
		}
	return NULL;
}

gint main(gint argc, gchar **argv)
{
	CamelSession *session;
	CamelStore *store;
	CamelFolder *folder;
	CamelMimeMessage *msg;
	gint i, j;
	CamelStream *mbox;
	CamelFilterDriver *driver;
	GError *error = NULL;

	camel_test_init(argc, argv);
	camel_test_provider_init(1, local_drivers);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	camel_test_start("Simple filtering of mbox");

	session = camel_test_session_new ("/tmp/camel-test");

	/* todo: cross-check everything with folder_info checks as well */
	/* todo: work out how to do imap/pop/nntp tests */

	push("getting store");
	store = camel_session_get_store(session, "mbox:///tmp/camel-test/mbox", &error);
	check_msg(error == NULL, "getting store: %s", error->message);
	check(store != NULL);
	g_clear_error (&error);
	pull();

	push("Creating output folders");
	for (i = 0; i < G_N_ELEMENTS (mailboxes); i++) {
		push("creating %s", mailboxes[i].name);
		mailboxes[i].folder = folder = camel_store_get_folder(store, mailboxes[i].name, CAMEL_STORE_FOLDER_CREATE, &error);
		check_msg(error == NULL, "%s", error->message);
		check(folder != NULL);

		/* we need an empty folder for this to work */
		test_folder_counts(folder, 0, 0);
		g_clear_error (&error);
		pull();
	}
	pull();

	/* append a bunch of messages with specific content */
	push("creating 100 test message mbox");
	mbox = camel_stream_fs_new_with_name("/tmp/camel-test/inbox", O_WRONLY|O_CREAT|O_EXCL, 0600, NULL);
	for (j=0;j<100;j++) {
		gchar *content, *subject;

		push("creating test message");
		msg = test_message_create_simple();
		content = g_strdup_printf("data%d content\n", j);
		test_message_set_content_simple((CamelMimePart *)msg, 0, "text/plain",
						content, strlen(content));
		test_free(content);
		subject = g_strdup_printf("Test%d message%d subject", j, 100-j);
		camel_mime_message_set_subject(msg, subject);

		camel_mime_message_set_date(msg, j*60*24, 0);
		pull();

		camel_stream_printf(mbox, "From \n");
		check(camel_data_wrapper_write_to_stream((CamelDataWrapper *)msg, mbox, NULL) != -1);
#if 0
		push("appending simple message %d", j);
		camel_folder_append_message(folder, msg, NULL, ex);
		check_msg(error == NULL, "%s", error->message);
		g_clear_error (&error);
		pull();
#endif
		test_free(subject);

		check_unref(msg, 1);
	}
	check(camel_stream_close(mbox, NULL) != -1);
	check_unref(mbox, 1);
	pull();

	push("Building filters");
	driver = camel_filter_driver_new(session);
	camel_filter_driver_set_folder_func(driver, get_folder, NULL);
	for (i = 0; i < G_N_ELEMENTS (rules); i++) {
		camel_filter_driver_add_rule(driver, rules[i].name, rules[i].match, rules[i].action);
	}
	pull();

	push("Executing filters");
	camel_filter_driver_set_default_folder(driver, mailboxes[0].folder);
	camel_filter_driver_filter_mbox(driver, "/tmp/camel-test/inbox", NULL, &error);
	check_msg(error == NULL, "%s", error->message);

	/* now need to check the folder counts/etc */

	check_unref(driver, 1);
	g_clear_error (&error);
	pull();

	/* this tests that invalid rules are caught */
	push("Testing broken match rules");
	for (i = 0; i < G_N_ELEMENTS (brokens); i++) {
		push("rule %s", brokens[i].match);
		driver = camel_filter_driver_new(session);
		camel_filter_driver_set_folder_func(driver, get_folder, NULL);
		camel_filter_driver_add_rule(driver, brokens[i].name, brokens[i].match, brokens[i].action);
		camel_filter_driver_filter_mbox(driver, "/tmp/camel-test/inbox", NULL, &error);
		check(error != NULL);
		check_unref(driver, 1);
		g_clear_error (&error);
		pull();
	}
	pull();

	push("Testing broken action rules");
	for (i = 0; i < G_N_ELEMENTS (brokena); i++) {
		push("rule %s", brokena[i].action);
		driver = camel_filter_driver_new(session);
		camel_filter_driver_set_folder_func(driver, get_folder, NULL);
		camel_filter_driver_add_rule(driver, brokena[i].name, brokena[i].match, brokena[i].action);
		camel_filter_driver_filter_mbox(driver, "/tmp/camel-test/inbox", NULL, &error);
		check(error != NULL);
		check_unref(driver, 1);
		g_clear_error (&error);
		pull();
	}
	pull();

	for (i = 0; i < G_N_ELEMENTS (mailboxes); i++) {
		check_unref(mailboxes[i].folder, 1);
	}

	check_unref(store, 1);

	check_unref(session, 1);

	camel_test_end();

	return 0;
}

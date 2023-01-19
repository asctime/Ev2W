/* threaded folder testing */

#include <string.h>

#include "camel-test.h"
#include "camel-test-provider.h"
#include "folders.h"
#include "messages.h"
#include "session.h"

#define MAX_MESSAGES (100)
#define MAX_THREADS (10)

#define d(x)

static const gchar *local_drivers[] = { "local" };

static const gchar *local_providers[] = {
	"mbox",
	"mh",
	"maildir"
};

static void
test_add_message(CamelFolder *folder, gint j)
{
	CamelMimeMessage *msg;
	gchar *content;
	gchar *subject;
	GError *error = NULL;

	push("creating message %d\n", j);
	msg = test_message_create_simple();
	content = g_strdup_printf("Test message %08x contents\n\n", j);
	test_message_set_content_simple((CamelMimePart *)msg, 0, "text/plain",
							content, strlen(content));
	test_free(content);
	subject = g_strdup_printf("Test message %08x subject", j);
	camel_mime_message_set_subject(msg, subject);
	pull();

	push("appending simple message %d", j);
	camel_folder_append_message(folder, msg, NULL, NULL, &error);
	check_msg(error == NULL, "%s", error->message);
	g_clear_error (&error);
	pull();

	check_unref(msg, 1);
}

struct _threadinfo {
	gint id;
	CamelFolder *folder;
};

static gpointer
worker(gpointer d)
{
	struct _threadinfo *info = d;
	gint i, j, id = info->id;
	gchar *sub, *content;
	GPtrArray *res;
	CamelMimeMessage *msg;
	GError *error = NULL;

	/* we add a message, search for it, twiddle some flags, delete it */
	/* and flat out */
	for (i=0;i<MAX_MESSAGES;i++) {
		d(printf ("Thread %p message %i\n", g_thread_self (), i));
		test_add_message(info->folder, id+i);

		sub = g_strdup_printf("(match-all (header-contains \"subject\" \"message %08x subject\"))", id+i);

		push("searching for message %d\n\tusing: %s", id+i, sub);
		res = camel_folder_search_by_expression(info->folder, sub, &error);
		check_msg(error == NULL, "%s", error->message);
		check_msg(res->len == 1, "res->len = %d", res->len);
		g_clear_error (&error);
		pull();

		push("getting message '%s'", res->pdata[0]);
		msg = camel_folder_get_message(info->folder, (gchar *)res->pdata[0], &error);
		check_msg(error == NULL, "%s", error->message);
		g_clear_error (&error);
		pull();

		content = g_strdup_printf("Test message %08x contents\n\n", id+i);
		push("comparing content '%s': '%s'", res->pdata[0], content);
		test_message_compare_content(camel_medium_get_content((CamelMedium *)msg), content, strlen(content));
		test_free(content);
		pull();

		push("deleting message, cleanup");
		j=(100.0*rand()/(RAND_MAX+1.0));
		if (j<=70) {
			camel_folder_delete_message(info->folder, res->pdata[0]);
		}

		camel_folder_search_free(info->folder, res);
		res = NULL;
		test_free(sub);

		check_unref(msg, 1);
		pull();

		/* about 1-in 100 calls will expunge */
		j=(200.0*rand()/(RAND_MAX+1.0));
		if (j<=2) {
			d(printf("Forcing an expuge\n"));
			push("expunging folder");
			camel_folder_expunge(info->folder, &error);
			check_msg(error == NULL, "%s", error->message);
			pull();
		}
	}

	return info;
}

gint main(gint argc, gchar **argv)
{
	CamelSession *session;
	gint i, j, index;
	gchar *path;
	CamelStore *store;
	GThread *threads[MAX_THREADS];
	struct _threadinfo *info;
	CamelFolder *folder;
	GPtrArray *uids;
	GError *error = NULL;

	camel_test_init(argc, argv);
	camel_test_provider_init(1, local_drivers);

	/* clear out any camel-test data */
	system("/bin/rm -rf /tmp/camel-test");

	session = camel_test_session_new ("/tmp/camel-test");

	for (j = 0; j < G_N_ELEMENTS (local_providers); j++) {
		for (index=0;index<2;index++) {
			path = g_strdup_printf("method %s %s", local_providers[j], index?"indexed":"nonindexed");
			camel_test_start(path);
			test_free(path);

			push("trying %s index %d", local_providers[j], index);
			path = g_strdup_printf("%s:///tmp/camel-test/%s", local_providers[j], local_providers[j]);
			store = camel_session_get_store(session, path, &error);
			check_msg(error == NULL, "%s", error->message);
			g_clear_error (&error);
			test_free(path);

			if (index == 0)
				folder = camel_store_get_folder(store, "testbox", CAMEL_STORE_FOLDER_CREATE, &error);
			else
				folder = camel_store_get_folder(store, "testbox",
								CAMEL_STORE_FOLDER_CREATE|CAMEL_STORE_FOLDER_BODY_INDEX, &error);
			check_msg(error == NULL, "%s", error->message);
			g_clear_error (&error);

			for (i = 0; i < MAX_THREADS; i++) {
				GError *error = NULL;

				info = g_malloc(sizeof(*info));
				info->id = i*MAX_MESSAGES;
				info->folder = folder;

				threads[i] = g_thread_create (worker, info, TRUE, &error);
				if (error) {
					fprintf (stderr, "%s: Failed to create a thread: %s\n", G_STRFUNC, error->message);
					g_error_free (error);
				}
			}

			for (i = 0; i < MAX_THREADS; i++) {
				if (threads[i]) {
					info = g_thread_join (threads[i]);
					g_free (info);
				}
			}
			pull();

			push("deleting remaining messages");
			uids = camel_folder_get_uids(folder);
			for (i=0;i<uids->len;i++) {
				camel_folder_delete_message(folder, uids->pdata[i]);
			}
			camel_folder_free_uids(folder, uids);

			camel_folder_expunge(folder, &error);
			check_msg(error == NULL, "%s", error->message);

			check_unref(folder, 1);

			camel_store_delete_folder(store, "testbox", &error);
			check_msg(error == NULL, "%s", error->message);
			g_clear_error (&error);

			check_unref(store, 1);

			pull();

			camel_test_end();
		}
	}

	g_object_unref (session);

	return 0;
}

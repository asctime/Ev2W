#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camel-test.h"

const gchar *base = "http://a/b/c/d;p?q#f";

struct {
	const gchar *url_string, *result;
} tests[] = {
	{ "g:h", "g:h" },
	{ "g", "http://a/b/c/g" },
	{ "./g", "http://a/b/c/g" },
	{ "g/", "http://a/b/c/g/" },
	{ "/g", "http://a/g" },
	{ "//g", "http://g" },
	{ "?y", "http://a/b/c/d;p?y" },
	{ "g?y", "http://a/b/c/g?y" },
	{ "g?y/./x", "http://a/b/c/g?y/./x" },
	{ "#s", "http://a/b/c/d;p?q#s" },
	{ "g#s", "http://a/b/c/g#s" },
	{ "g#s/./x", "http://a/b/c/g#s/./x" },
	{ "g?y#s", "http://a/b/c/g?y#s" },
	{ ";x", "http://a/b/c/d;x" },
	{ "g;x", "http://a/b/c/g;x" },
	{ "g;x?y#s", "http://a/b/c/g;x?y#s" },
	{ ".", "http://a/b/c/" },
	{ "./", "http://a/b/c/" },
	{ "..", "http://a/b/" },
	{ "../", "http://a/b/" },
	{ "../g", "http://a/b/g" },
	{ "../..", "http://a/" },
	{ "../../", "http://a/" },
	{ "../../g", "http://a/g" },
	{ "", "http://a/b/c/d;p?q#f" },
	{ "../../../g", "http://a/../g" },
	{ "../../../../g", "http://a/../../g" },
	{ "/./g", "http://a/./g" },
	{ "/../g", "http://a/../g" },
	{ "g.", "http://a/b/c/g." },
	{ ".g", "http://a/b/c/.g" },
	{ "g..", "http://a/b/c/g.." },
	{ "..g", "http://a/b/c/..g" },
	{ "./../g", "http://a/b/g" },
	{ "./g/.", "http://a/b/c/g/" },
	{ "g/./h", "http://a/b/c/g/h" },
	{ "g/../h", "http://a/b/c/h" },
	{ "http:g", "http:g" },
	{ "http:", "http:" },

	/* (not from rfc 1808) */
	{ "sendmail:", "sendmail:" },
	{ "mbox:/var/mail/user", "mbox:/var/mail/user" },
	{ "pop://user@host", "pop://user@host" },
	{ "pop://user@host:99", "pop://user@host:99" },
	{ "pop://user:password@host", "pop://user:password@host" },
	{ "pop://user:password@host:99", "pop://user:password@host:99" },
	{ "pop://user;auth=APOP@host", "pop://user;auth=APOP@host" },
	{ "pop://user@host/;keep_on_server", "pop://user@host/;keep_on_server" },
	{ "pop://user@host/;keep_on_server=1", "pop://user@host/;keep_on_server=1" },
	{ "pop://us%65r@host", "pop://user@host" },
	{ "pop://us%40r@host", "pop://us%40r@host" },
	{ "pop://us%3ar@host", "pop://us%3ar@host" },
	{ "pop://us%2fr@host", "pop://us%2fr@host" }
};

gint
main (gint argc, gchar **argv)
{
	CamelURL *base_url, *url;
	gchar *url_string;
	gint i;
	GError *error = NULL;

	camel_test_init (argc, argv);

	camel_test_start ("URL parsing");

	camel_test_push ("base URL parsing");
	base_url = camel_url_new (base, &error);
	if (!base_url) {
		camel_test_fail (
			"Could not parse %s: %s\n",
			base, error->message);
	}
	camel_test_pull ();

	camel_test_push ("base URL unparsing");
	url_string = camel_url_to_string (base_url, 0);
	if (strcmp (url_string, base) != 0) {
		camel_test_fail ("URL <%s> unparses to <%s>\n",
				 base, url_string);
	}
	camel_test_pull ();
	g_free (url_string);

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		camel_test_push ("<%s> + <%s> = <%s>?", base, tests[i].url_string, tests[i].result);
		url = camel_url_new_with_base (base_url, tests[i].url_string);
		if (!url) {
			camel_test_fail ("could not parse");
			camel_test_pull ();
			continue;
		}

		url_string = camel_url_to_string (url, 0);
		if (strcmp (url_string, tests[i].result) != 0)
			camel_test_fail ("got <%s>!", url_string);
		g_free (url_string);
		camel_test_pull ();
	}

	camel_test_end ();

	return 0;
}

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_TEST_SESSION \
	(camel_test_session_get_type ())
#define CAMEL_TEST_SESSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_TEST_SESSION, CamelTestSession))
#define CAMEL_TEST_SESSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_TEST_SESSION, CamelTestSessionClass))
#define CAMEL_IS_TEST_SESSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_TEST_SESSION))
#define CAMEL_IS_TEST_SESSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_TEST_SESSION))
#define CAMEL_TEST_SESSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_TEST_SESSION, CamelTestSessionClass))

G_BEGIN_DECLS

typedef struct _CamelTestSession CamelTestSession;
typedef struct _CamelTestSessionClass CamelTestSessionClass;

struct _CamelTestSession {
	CamelSession parent;
};

struct _CamelTestSessionClass {
	CamelSessionClass parent_class;
};

GType camel_test_session_get_type (void);
CamelSession *camel_test_session_new (const gchar *path);

G_END_DECLS

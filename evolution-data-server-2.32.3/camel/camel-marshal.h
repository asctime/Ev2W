
#ifndef __camel_marshal_MARSHAL_H__
#define __camel_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* NONE:STRING,POINTER (camel-marshal.list:1) */
extern void camel_marshal_VOID__STRING_POINTER (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);
#define camel_marshal_NONE__STRING_POINTER	camel_marshal_VOID__STRING_POINTER

G_END_DECLS

#endif /* __camel_marshal_MARSHAL_H__ */


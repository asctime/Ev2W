
#ifndef __e_data_server_ui_marshal_MARSHAL_H__
#define __e_data_server_ui_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* BOOLEAN:OBJECT,BOXED (e-data-server-ui-marshal.list:1) */
extern void e_data_server_ui_marshal_BOOLEAN__OBJECT_BOXED (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);

/* BOOLEAN:BOXED,OBJECT,FLAGS,UINT (e-data-server-ui-marshal.list:2) */
extern void e_data_server_ui_marshal_BOOLEAN__BOXED_OBJECT_FLAGS_UINT (GClosure     *closure,
                                                                       GValue       *return_value,
                                                                       guint         n_param_values,
                                                                       const GValue *param_values,
                                                                       gpointer      invocation_hint,
                                                                       gpointer      marshal_data);

G_END_DECLS

#endif /* __e_data_server_ui_marshal_MARSHAL_H__ */


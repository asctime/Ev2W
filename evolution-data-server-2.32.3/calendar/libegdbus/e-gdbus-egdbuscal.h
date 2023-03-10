/* File:       e-gdbus-egdbuscal.h
 *
 * GType name: EGdbusCal
 * D-Bus name: org.gnome.evolution.dataserver.calendar.Cal
 *
 * Generated by GDBus Binding Tool 0.1. DO NOT EDIT.
 */

#ifndef __E_GDBUS_E_GDBUS_CAL_H__
#define __E_GDBUS_E_GDBUS_CAL_H__

#include <gio/gio.h>

#include "e-gdbus-typemappers.h"
G_BEGIN_DECLS

#define E_GDBUS_TYPE_CAL         (e_gdbus_cal_get_type ())
#define E_GDBUS_CAL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), E_GDBUS_TYPE_CAL, EGdbusCal))
#define E_GDBUS_IS_CAL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_GDBUS_TYPE_CAL))
#define E_GDBUS_CAL_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE((o), E_GDBUS_TYPE_CAL, EGdbusCalIface))

/**
 * EGdbusCal:
 *
 * Opaque type representing a proxy or an exported object.
 */
typedef struct _EGdbusCal EGdbusCal; /* Dummy typedef */
/**
 * EGdbusCalIface:
 * @parent_iface: The parent interface.
 * @auth_required: Handler for the #EGdbusCal::auth-required signal.
 * @backend_error: Handler for the #EGdbusCal::backend-error signal.
 * @readonly: Handler for the #EGdbusCal::readonly signal.
 * @mode: Handler for the #EGdbusCal::mode signal.
 * @handle_get_uri: Handler for the #EGdbusCal::handle-get-uri signal.
 * @handle_get_cache_dir: Handler for the #EGdbusCal::handle-get-cache-dir signal.
 * @handle_open: Handler for the #EGdbusCal::handle-open signal.
 * @handle_refresh: Handler for the #EGdbusCal::handle-refresh signal.
 * @handle_close: Handler for the #EGdbusCal::handle-close signal.
 * @handle_remove: Handler for the #EGdbusCal::handle-remove signal.
 * @handle_is_read_only: Handler for the #EGdbusCal::handle-is-read-only signal.
 * @handle_get_cal_address: Handler for the #EGdbusCal::handle-get-cal-address signal.
 * @handle_get_alarm_email_address: Handler for the #EGdbusCal::handle-get-alarm-email-address signal.
 * @handle_get_ldap_attribute: Handler for the #EGdbusCal::handle-get-ldap-attribute signal.
 * @handle_get_scheduling_information: Handler for the #EGdbusCal::handle-get-scheduling-information signal.
 * @handle_set_mode: Handler for the #EGdbusCal::handle-set-mode signal.
 * @handle_get_default_object: Handler for the #EGdbusCal::handle-get-default-object signal.
 * @handle_get_object: Handler for the #EGdbusCal::handle-get-object signal.
 * @handle_get_object_list: Handler for the #EGdbusCal::handle-get-object-list signal.
 * @handle_get_changes: Handler for the #EGdbusCal::handle-get-changes signal.
 * @handle_get_free_busy: Handler for the #EGdbusCal::handle-get-free-busy signal.
 * @handle_discard_alarm: Handler for the #EGdbusCal::handle-discard-alarm signal.
 * @handle_create_object: Handler for the #EGdbusCal::handle-create-object signal.
 * @handle_modify_object: Handler for the #EGdbusCal::handle-modify-object signal.
 * @handle_remove_object: Handler for the #EGdbusCal::handle-remove-object signal.
 * @handle_receive_objects: Handler for the #EGdbusCal::handle-receive-objects signal.
 * @handle_send_objects: Handler for the #EGdbusCal::handle-send-objects signal.
 * @handle_get_attachment_list: Handler for the #EGdbusCal::handle-get-attachment-list signal.
 * @handle_get_query: Handler for the #EGdbusCal::handle-get-query signal.
 * @handle_get_timezone: Handler for the #EGdbusCal::handle-get-timezone signal.
 * @handle_add_timezone: Handler for the #EGdbusCal::handle-add-timezone signal.
 * @handle_set_default_timezone: Handler for the #EGdbusCal::handle-set-default-timezone signal.
 *
 * Virtual table.
 */
typedef struct _EGdbusCalIface EGdbusCalIface;

GType e_gdbus_cal_get_type (void) G_GNUC_CONST;

/* ---------------------------------------------------------------------- */

typedef struct _EGdbusCalProxy EGdbusCalProxy;
typedef struct _EGdbusCalProxyClass EGdbusCalProxyClass;

/**
 * EGdbusCalProxyPrivate:
 *
 * The #EGdbusCalProxyPrivate structure contains only private data.
 */
typedef struct _EGdbusCalProxyPrivate EGdbusCalProxyPrivate;

/**
 * EGdbusCalProxy:
 *
 * The #EGdbusCalProxy structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _EGdbusCalProxy
{
  GDBusProxy parent_instance;
  EGdbusCalProxyPrivate *priv;
};

/**
 * EGdbusCalProxyClass:
 *
 * Class structure for #EGdbusCalProxy.
 */
struct _EGdbusCalProxyClass
{
  GDBusProxyClass parent_class;
};

#define E_GDBUS_TYPE_CAL_PROXY (e_gdbus_cal_proxy_get_type ())
GType e_gdbus_cal_proxy_get_type (void) G_GNUC_CONST;

void e_gdbus_cal_proxy_new (GDBusConnection     *connection,
                   GDBusProxyFlags      flags,
                   const gchar         *name,
                   const gchar         *object_path,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data);
EGdbusCal *e_gdbus_cal_proxy_new_finish (GAsyncResult  *res,
                        GError       **error);
EGdbusCal *e_gdbus_cal_proxy_new_sync (GDBusConnection     *connection,
                       GDBusProxyFlags      flags,
                       const gchar         *name,
                       const gchar         *object_path,
                       GCancellable        *cancellable,
                       GError             **error);

void e_gdbus_cal_proxy_new_for_bus (GBusType             bus_type,
                           GDBusProxyFlags      flags,
                           const gchar         *name,
                           const gchar         *object_path,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data);
EGdbusCal *e_gdbus_cal_proxy_new_for_bus_finish (GAsyncResult  *res,
                                 GError       **error);
EGdbusCal *e_gdbus_cal_proxy_new_for_bus_sync (GBusType             bus_type,
                               GDBusProxyFlags      flags,
                               const gchar         *name,
                               const gchar         *object_path,
                               GCancellable        *cancellable,
                               GError             **error);

/* ---------------------------------------------------------------------- */

typedef struct _EGdbusCalStub EGdbusCalStub;
typedef struct _EGdbusCalStubClass EGdbusCalStubClass;

/**
 * EGdbusCalStubPrivate:
 *
 * The #EGdbusCalStubPrivate structure contains only private data.
 */
typedef struct _EGdbusCalStubPrivate EGdbusCalStubPrivate;

/**
 * EGdbusCalStub:
 *
 * The #EGdbusCalStub structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _EGdbusCalStub
{
  GObject parent_instance;
  EGdbusCalStubPrivate *priv;
};

/**
 * EGdbusCalStubClass:
 *
 * Class structure for #EGdbusCalStub.
 */
struct _EGdbusCalStubClass
{
  GObjectClass parent_class;
};

#define E_GDBUS_TYPE_CAL_STUB (e_gdbus_cal_stub_get_type ())
GType e_gdbus_cal_stub_get_type (void) G_GNUC_CONST;

EGdbusCal *e_gdbus_cal_stub_new (void);

guint e_gdbus_cal_register_object (EGdbusCal *object,
                    GDBusConnection *connection,
                    const gchar *object_path,
                    GError **error);

void e_gdbus_cal_drain_notify (EGdbusCal *object);

const GDBusInterfaceInfo *e_gdbus_cal_interface_info (void) G_GNUC_CONST;

struct _EGdbusCalIface
{
  GTypeInterface parent_iface;

  /* Signal handlers for receiving D-Bus signals: */
  void (*auth_required) (
        EGdbusCal *object);
  void (*backend_error) (
        EGdbusCal *object,
        const gchar *arg_error);
  void (*readonly) (
        EGdbusCal *object,
        gboolean arg_is_readonly);
  void (*mode) (
        EGdbusCal *object,
        gint arg_mode);

  /* Signal handlers for handling D-Bus method calls: */
  gboolean (*handle_get_uri) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_get_cache_dir) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_open) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        gboolean in_only_if_exists,
        const gchar *in_username,
        const gchar *in_password);
  gboolean (*handle_refresh) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_close) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_remove) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_is_read_only) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_get_cal_address) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_get_alarm_email_address) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_get_ldap_attribute) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_get_scheduling_information) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_set_mode) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        guint in_mode);
  gboolean (*handle_get_default_object) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_get_object) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_uid,
        const gchar *in_rid);
  gboolean (*handle_get_object_list) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_sexp);
  gboolean (*handle_get_changes) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_change_id);
  gboolean (*handle_get_free_busy) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar * const *in_user_list,
        guint in_start,
        guint in_end);
  gboolean (*handle_discard_alarm) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_uid,
        const gchar *in_auid);
  gboolean (*handle_create_object) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_calobj);
  gboolean (*handle_modify_object) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_calobj,
        guint in_mod);
  gboolean (*handle_remove_object) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_uid,
        const gchar *in_rid,
        guint in_mod);
  gboolean (*handle_receive_objects) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_calobj);
  gboolean (*handle_send_objects) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_calobj);
  gboolean (*handle_get_attachment_list) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_uid,
        const gchar *in_rid);
  gboolean (*handle_get_query) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_sexp);
  gboolean (*handle_get_timezone) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_tzid);
  gboolean (*handle_add_timezone) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_tz);
  gboolean (*handle_set_default_timezone) (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *in_tz);
};

/* C Bindings for properties */

/* D-Bus Methods */
void e_gdbus_cal_call_get_uri (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_uri_finish (
        EGdbusCal *proxy,
        gchar **out_str_uri_copy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_uri_sync (
        EGdbusCal *proxy,
        gchar **out_str_uri_copy,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_cache_dir (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_cache_dir_finish (
        EGdbusCal *proxy,
        gchar **out_dirname,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_cache_dir_sync (
        EGdbusCal *proxy,
        gchar **out_dirname,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_open (
        EGdbusCal *proxy,
        gboolean in_only_if_exists,
        const gchar *in_username,
        const gchar *in_password,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_open_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_open_sync (
        EGdbusCal *proxy,
        gboolean in_only_if_exists,
        const gchar *in_username,
        const gchar *in_password,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_refresh (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_refresh_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_refresh_sync (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_close (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_close_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_close_sync (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_remove (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_remove_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_remove_sync (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_is_read_only (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_is_read_only_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_is_read_only_sync (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_cal_address (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_cal_address_finish (
        EGdbusCal *proxy,
        gchar **out_address,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_cal_address_sync (
        EGdbusCal *proxy,
        gchar **out_address,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_alarm_email_address (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_alarm_email_address_finish (
        EGdbusCal *proxy,
        gchar **out_address,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_alarm_email_address_sync (
        EGdbusCal *proxy,
        gchar **out_address,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_ldap_attribute (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_ldap_attribute_finish (
        EGdbusCal *proxy,
        gchar **out_address,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_ldap_attribute_sync (
        EGdbusCal *proxy,
        gchar **out_address,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_scheduling_information (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_scheduling_information_finish (
        EGdbusCal *proxy,
        gchar **out_capabilities,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_scheduling_information_sync (
        EGdbusCal *proxy,
        gchar **out_capabilities,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_set_mode (
        EGdbusCal *proxy,
        guint in_mode,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_set_mode_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_set_mode_sync (
        EGdbusCal *proxy,
        guint in_mode,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_default_object (
        EGdbusCal *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_default_object_finish (
        EGdbusCal *proxy,
        gchar **out_object,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_default_object_sync (
        EGdbusCal *proxy,
        gchar **out_object,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_object (
        EGdbusCal *proxy,
        const gchar *in_uid,
        const gchar *in_rid,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_object_finish (
        EGdbusCal *proxy,
        gchar **out_object,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_object_sync (
        EGdbusCal *proxy,
        const gchar *in_uid,
        const gchar *in_rid,
        gchar **out_object,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_object_list (
        EGdbusCal *proxy,
        const gchar *in_sexp,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_object_list_finish (
        EGdbusCal *proxy,
        gchar ***out_objects,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_object_list_sync (
        EGdbusCal *proxy,
        const gchar *in_sexp,
        gchar ***out_objects,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_changes (
        EGdbusCal *proxy,
        const gchar *in_change_id,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_changes_finish (
        EGdbusCal *proxy,
        gchar ***out_additions,
        gchar ***out_modifications,
        gchar ***out_removals,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_changes_sync (
        EGdbusCal *proxy,
        const gchar *in_change_id,
        gchar ***out_additions,
        gchar ***out_modifications,
        gchar ***out_removals,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_free_busy (
        EGdbusCal *proxy,
        const gchar * const *in_user_list,
        guint in_start,
        guint in_end,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_free_busy_finish (
        EGdbusCal *proxy,
        gchar ***out_freebusy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_free_busy_sync (
        EGdbusCal *proxy,
        const gchar * const *in_user_list,
        guint in_start,
        guint in_end,
        gchar ***out_freebusy,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_discard_alarm (
        EGdbusCal *proxy,
        const gchar *in_uid,
        const gchar *in_auid,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_discard_alarm_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_discard_alarm_sync (
        EGdbusCal *proxy,
        const gchar *in_uid,
        const gchar *in_auid,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_create_object (
        EGdbusCal *proxy,
        const gchar *in_calobj,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_create_object_finish (
        EGdbusCal *proxy,
        gchar **out_uid,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_create_object_sync (
        EGdbusCal *proxy,
        const gchar *in_calobj,
        gchar **out_uid,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_modify_object (
        EGdbusCal *proxy,
        const gchar *in_calobj,
        guint in_mod,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_modify_object_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_modify_object_sync (
        EGdbusCal *proxy,
        const gchar *in_calobj,
        guint in_mod,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_remove_object (
        EGdbusCal *proxy,
        const gchar *in_uid,
        const gchar *in_rid,
        guint in_mod,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_remove_object_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_remove_object_sync (
        EGdbusCal *proxy,
        const gchar *in_uid,
        const gchar *in_rid,
        guint in_mod,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_receive_objects (
        EGdbusCal *proxy,
        const gchar *in_calobj,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_receive_objects_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_receive_objects_sync (
        EGdbusCal *proxy,
        const gchar *in_calobj,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_send_objects (
        EGdbusCal *proxy,
        const gchar *in_calobj,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_send_objects_finish (
        EGdbusCal *proxy,
        gchar ***out_users,
        gchar **out_calobj,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_send_objects_sync (
        EGdbusCal *proxy,
        const gchar *in_calobj,
        gchar ***out_users,
        gchar **out_calobj,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_attachment_list (
        EGdbusCal *proxy,
        const gchar *in_uid,
        const gchar *in_rid,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_attachment_list_finish (
        EGdbusCal *proxy,
        gchar ***out_attachments,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_attachment_list_sync (
        EGdbusCal *proxy,
        const gchar *in_uid,
        const gchar *in_rid,
        gchar ***out_attachments,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_query (
        EGdbusCal *proxy,
        const gchar *in_sexp,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_query_finish (
        EGdbusCal *proxy,
        gchar **out_query,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_query_sync (
        EGdbusCal *proxy,
        const gchar *in_sexp,
        gchar **out_query,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_get_timezone (
        EGdbusCal *proxy,
        const gchar *in_tzid,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_get_timezone_finish (
        EGdbusCal *proxy,
        gchar **out_object,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_get_timezone_sync (
        EGdbusCal *proxy,
        const gchar *in_tzid,
        gchar **out_object,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_add_timezone (
        EGdbusCal *proxy,
        const gchar *in_tz,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_add_timezone_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_add_timezone_sync (
        EGdbusCal *proxy,
        const gchar *in_tz,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_cal_call_set_default_timezone (
        EGdbusCal *proxy,
        const gchar *in_tz,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_cal_call_set_default_timezone_finish (
        EGdbusCal *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_cal_call_set_default_timezone_sync (
        EGdbusCal *proxy,
        const gchar *in_tz,
        GCancellable *cancellable,
        GError **error);

/* D-Bus Methods Completion Helpers */
void e_gdbus_cal_complete_get_uri (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_str_uri_copy);

void e_gdbus_cal_complete_get_cache_dir (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_dirname);

void e_gdbus_cal_complete_open (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_refresh (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_close (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_remove (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_is_read_only (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_get_cal_address (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_address);

void e_gdbus_cal_complete_get_alarm_email_address (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_address);

void e_gdbus_cal_complete_get_ldap_attribute (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_address);

void e_gdbus_cal_complete_get_scheduling_information (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_capabilities);

void e_gdbus_cal_complete_set_mode (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_get_default_object (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_object);

void e_gdbus_cal_complete_get_object (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_object);

void e_gdbus_cal_complete_get_object_list (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar * const *out_objects);

void e_gdbus_cal_complete_get_changes (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar * const *out_additions,
        const gchar * const *out_modifications,
        const gchar * const *out_removals);

void e_gdbus_cal_complete_get_free_busy (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar * const *out_freebusy);

void e_gdbus_cal_complete_discard_alarm (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_create_object (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_uid);

void e_gdbus_cal_complete_modify_object (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_remove_object (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_receive_objects (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_send_objects (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar * const *out_users,
        const gchar *out_calobj);

void e_gdbus_cal_complete_get_attachment_list (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar * const *out_attachments);

void e_gdbus_cal_complete_get_query (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_query);

void e_gdbus_cal_complete_get_timezone (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation,
        const gchar *out_object);

void e_gdbus_cal_complete_add_timezone (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_cal_complete_set_default_timezone (
        EGdbusCal *object,
        GDBusMethodInvocation *invocation);

/* D-Bus Signal Emission Helpers */
void e_gdbus_cal_emit_auth_required (
        EGdbusCal *object);

void e_gdbus_cal_emit_backend_error (
        EGdbusCal *object,
        const gchar *arg_error);

void e_gdbus_cal_emit_readonly (
        EGdbusCal *object,
        gboolean arg_is_readonly);

void e_gdbus_cal_emit_mode (
        EGdbusCal *object,
        gint arg_mode);

G_END_DECLS

#endif /* __E_GDBUS_E_GDBUS_CAL_H__ */

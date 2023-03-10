/* File:       e-gdbus-egdbusbookview.h
 *
 * GType name: EGdbusBookView
 * D-Bus name: org.gnome.evolution.dataserver.addressbook.BookView
 *
 * Generated by GDBus Binding Tool 0.1. DO NOT EDIT.
 */

#ifndef __E_GDBUS_E_GDBUS_BOOK_VIEW_H__
#define __E_GDBUS_E_GDBUS_BOOK_VIEW_H__

#include <gio/gio.h>

#include "e-gdbus-typemappers.h"
G_BEGIN_DECLS

#define E_GDBUS_TYPE_BOOK_VIEW         (e_gdbus_book_view_get_type ())
#define E_GDBUS_BOOK_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), E_GDBUS_TYPE_BOOK_VIEW, EGdbusBookView))
#define E_GDBUS_IS_BOOK_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_GDBUS_TYPE_BOOK_VIEW))
#define E_GDBUS_BOOK_VIEW_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE((o), E_GDBUS_TYPE_BOOK_VIEW, EGdbusBookViewIface))

/**
 * EGdbusBookView:
 *
 * Opaque type representing a proxy or an exported object.
 */
typedef struct _EGdbusBookView EGdbusBookView; /* Dummy typedef */
/**
 * EGdbusBookViewIface:
 * @parent_iface: The parent interface.
 * @contacts_added: Handler for the #EGdbusBookView::contacts-added signal.
 * @contacts_changed: Handler for the #EGdbusBookView::contacts-changed signal.
 * @contacts_removed: Handler for the #EGdbusBookView::contacts-removed signal.
 * @status_message: Handler for the #EGdbusBookView::status-message signal.
 * @complete: Handler for the #EGdbusBookView::complete signal.
 * @handle_start: Handler for the #EGdbusBookView::handle-start signal.
 * @handle_stop: Handler for the #EGdbusBookView::handle-stop signal.
 * @handle_dispose: Handler for the #EGdbusBookView::handle-dispose signal.
 *
 * Virtual table.
 */
typedef struct _EGdbusBookViewIface EGdbusBookViewIface;

GType e_gdbus_book_view_get_type (void) G_GNUC_CONST;

/* ---------------------------------------------------------------------- */

typedef struct _EGdbusBookViewProxy EGdbusBookViewProxy;
typedef struct _EGdbusBookViewProxyClass EGdbusBookViewProxyClass;

/**
 * EGdbusBookViewProxyPrivate:
 *
 * The #EGdbusBookViewProxyPrivate structure contains only private data.
 */
typedef struct _EGdbusBookViewProxyPrivate EGdbusBookViewProxyPrivate;

/**
 * EGdbusBookViewProxy:
 *
 * The #EGdbusBookViewProxy structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _EGdbusBookViewProxy
{
  GDBusProxy parent_instance;
  EGdbusBookViewProxyPrivate *priv;
};

/**
 * EGdbusBookViewProxyClass:
 *
 * Class structure for #EGdbusBookViewProxy.
 */
struct _EGdbusBookViewProxyClass
{
  GDBusProxyClass parent_class;
};

#define E_GDBUS_TYPE_BOOK_VIEW_PROXY (e_gdbus_book_view_proxy_get_type ())
GType e_gdbus_book_view_proxy_get_type (void) G_GNUC_CONST;

void e_gdbus_book_view_proxy_new (GDBusConnection     *connection,
                   GDBusProxyFlags      flags,
                   const gchar         *name,
                   const gchar         *object_path,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data);
EGdbusBookView *e_gdbus_book_view_proxy_new_finish (GAsyncResult  *res,
                        GError       **error);
EGdbusBookView *e_gdbus_book_view_proxy_new_sync (GDBusConnection     *connection,
                       GDBusProxyFlags      flags,
                       const gchar         *name,
                       const gchar         *object_path,
                       GCancellable        *cancellable,
                       GError             **error);

void e_gdbus_book_view_proxy_new_for_bus (GBusType             bus_type,
                           GDBusProxyFlags      flags,
                           const gchar         *name,
                           const gchar         *object_path,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data);
EGdbusBookView *e_gdbus_book_view_proxy_new_for_bus_finish (GAsyncResult  *res,
                                 GError       **error);
EGdbusBookView *e_gdbus_book_view_proxy_new_for_bus_sync (GBusType             bus_type,
                               GDBusProxyFlags      flags,
                               const gchar         *name,
                               const gchar         *object_path,
                               GCancellable        *cancellable,
                               GError             **error);

/* ---------------------------------------------------------------------- */

typedef struct _EGdbusBookViewStub EGdbusBookViewStub;
typedef struct _EGdbusBookViewStubClass EGdbusBookViewStubClass;

/**
 * EGdbusBookViewStubPrivate:
 *
 * The #EGdbusBookViewStubPrivate structure contains only private data.
 */
typedef struct _EGdbusBookViewStubPrivate EGdbusBookViewStubPrivate;

/**
 * EGdbusBookViewStub:
 *
 * The #EGdbusBookViewStub structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _EGdbusBookViewStub
{
  GObject parent_instance;
  EGdbusBookViewStubPrivate *priv;
};

/**
 * EGdbusBookViewStubClass:
 *
 * Class structure for #EGdbusBookViewStub.
 */
struct _EGdbusBookViewStubClass
{
  GObjectClass parent_class;
};

#define E_GDBUS_TYPE_BOOK_VIEW_STUB (e_gdbus_book_view_stub_get_type ())
GType e_gdbus_book_view_stub_get_type (void) G_GNUC_CONST;

EGdbusBookView *e_gdbus_book_view_stub_new (void);

guint e_gdbus_book_view_register_object (EGdbusBookView *object,
                    GDBusConnection *connection,
                    const gchar *object_path,
                    GError **error);

void e_gdbus_book_view_drain_notify (EGdbusBookView *object);

const GDBusInterfaceInfo *e_gdbus_book_view_interface_info (void) G_GNUC_CONST;

struct _EGdbusBookViewIface
{
  GTypeInterface parent_iface;

  /* Signal handlers for receiving D-Bus signals: */
  void (*contacts_added) (
        EGdbusBookView *object,
        const gchar * const *arg_vcards);
  void (*contacts_changed) (
        EGdbusBookView *object,
        const gchar * const *arg_vcards);
  void (*contacts_removed) (
        EGdbusBookView *object,
        const gchar * const *arg_ids);
  void (*status_message) (
        EGdbusBookView *object,
        const gchar *arg_message);
  void (*complete) (
        EGdbusBookView *object,
        guint arg_status,
        const gchar *arg_message);

  /* Signal handlers for handling D-Bus method calls: */
  gboolean (*handle_start) (
        EGdbusBookView *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_stop) (
        EGdbusBookView *object,
        GDBusMethodInvocation *invocation);
  gboolean (*handle_dispose) (
        EGdbusBookView *object,
        GDBusMethodInvocation *invocation);
};

/* C Bindings for properties */

/* D-Bus Methods */
void e_gdbus_book_view_call_start (
        EGdbusBookView *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_book_view_call_start_finish (
        EGdbusBookView *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_book_view_call_start_sync (
        EGdbusBookView *proxy,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_book_view_call_stop (
        EGdbusBookView *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_book_view_call_stop_finish (
        EGdbusBookView *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_book_view_call_stop_sync (
        EGdbusBookView *proxy,
        GCancellable *cancellable,
        GError **error);

void e_gdbus_book_view_call_dispose (
        EGdbusBookView *proxy,
        GCancellable *cancellable,
        GAsyncReadyCallback callback,
        gpointer user_data);

gboolean e_gdbus_book_view_call_dispose_finish (
        EGdbusBookView *proxy,
        GAsyncResult *res,
        GError **error);

gboolean e_gdbus_book_view_call_dispose_sync (
        EGdbusBookView *proxy,
        GCancellable *cancellable,
        GError **error);

/* D-Bus Methods Completion Helpers */
void e_gdbus_book_view_complete_start (
        EGdbusBookView *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_book_view_complete_stop (
        EGdbusBookView *object,
        GDBusMethodInvocation *invocation);

void e_gdbus_book_view_complete_dispose (
        EGdbusBookView *object,
        GDBusMethodInvocation *invocation);

/* D-Bus Signal Emission Helpers */
void e_gdbus_book_view_emit_contacts_added (
        EGdbusBookView *object,
        const gchar * const *arg_vcards);

void e_gdbus_book_view_emit_contacts_changed (
        EGdbusBookView *object,
        const gchar * const *arg_vcards);

void e_gdbus_book_view_emit_contacts_removed (
        EGdbusBookView *object,
        const gchar * const *arg_ids);

void e_gdbus_book_view_emit_status_message (
        EGdbusBookView *object,
        const gchar *arg_message);

void e_gdbus_book_view_emit_complete (
        EGdbusBookView *object,
        guint arg_status,
        const gchar *arg_message);

G_END_DECLS

#endif /* __E_GDBUS_E_GDBUS_BOOK_VIEW_H__ */

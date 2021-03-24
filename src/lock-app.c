#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include "data.h"
#include "get-builder.h"
#include "gui-common.h"
#include "message-dialogs.h"
#include "otpclient.h"
#include "lock-app.h"

typedef struct lock_data_t {
    gint dialog_ret_code;
    gboolean retry;
    GtkWidget *pwd_entry;
    GtkBuilder *builder;
    AppData *app_data;
} LockData;

static void run_lock_dialog (GtkDialog *dlg,
                             gint       response_id,
                             gpointer   user_data);

static void
lock_app (GtkWidget *w __attribute__((unused)),
          gpointer user_data)
{
    AppData *app_data = (AppData *)user_data;
    LockData *lock_data = g_new0 (LockData, 1);

    lock_data->app_data = app_data;

    app_data->app_locked = TRUE;

    g_signal_emit_by_name (app_data->tree_view, "hide-all-otps");

    lock_data->builder = get_builder_from_partial_path (g_strconcat (UI_PARTIAL_PATH, "pwd_dialogs.ui", NULL));

    GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object (lock_data->builder, "unlock_pwd_diag_id"));
    lock_data->pwd_entry = GTK_WIDGET(gtk_builder_get_object (lock_data->builder, "unlock_entry_id"));

    g_signal_connect (lock_data->pwd_entry, "activate", G_CALLBACK (send_ok_cb), NULL);

    gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(app_data->main_window));

    gtk_widget_show (dialog);

    lock_data->retry = FALSE;
    do {
        app_data->loop = g_main_loop_new (NULL, FALSE);
        g_signal_connect (dialog, "response", G_CALLBACK(run_lock_dialog), lock_data);
        g_main_loop_run (app_data->loop);
        g_main_loop_unref (app_data->loop);
        app_data->loop = NULL;
    } while (lock_data->dialog_ret_code == GTK_RESPONSE_OK && lock_data->retry == TRUE);
}


static void
run_lock_dialog (GtkDialog *dlg,
                 gint       response_id,
                 gpointer   user_data)
{
    LockData *lock_data = (LockData *)user_data;
    if (response_id == GTK_RESPONSE_OK) {
        if (g_strcmp0 (lock_data->app_data->db_data->key, gtk_editable_get_text (GTK_EDITABLE(lock_data->pwd_entry))) != 0) {
            show_message_dialog (GTK_WIDGET(dlg), "The password is wrong, please try again.", GTK_MESSAGE_ERROR);
            gtk_editable_set_text (GTK_EDITABLE(lock_data->pwd_entry), "");
            lock_data->retry = TRUE;
        } else {
            lock_data->retry = FALSE;
            lock_data->app_data->app_locked = FALSE;
            lock_data->app_data->last_user_activity = g_date_time_new_now_local ();
            lock_data->app_data->source_id_last_activity = g_timeout_add_seconds (1, check_inactivity, lock_data->app_data);
            g_object_unref (lock_data->builder);
            g_free (lock_data);
            gtk_window_destroy (GTK_WINDOW(dlg));
        }
    } else {
        gtk_window_destroy (GTK_WINDOW(dlg));
        g_object_unref (lock_data->builder);
        GtkApplication *app = gtk_window_get_application (GTK_WINDOW (lock_data->app_data->main_window));
        destroy_cb (lock_data->app_data->main_window, lock_data->app_data);
        g_free (lock_data);
        g_application_quit (G_APPLICATION(app));
    }
}


static void
signal_triggered_cb (GDBusConnection *connection __attribute__((unused)),
                     const gchar *sender_name    __attribute__((unused)),
                     const gchar *object_path    __attribute__((unused)),
                     const gchar *interface_name __attribute__((unused)),
                     const gchar *signal_name    __attribute__((unused)),
                     GVariant *parameters,
                     gpointer user_data)
{
    AppData *app_data = (AppData *)user_data;
    gboolean is_screen_locked;
    g_variant_get (parameters, "(b)", &is_screen_locked);
    if (is_screen_locked == TRUE && app_data->app_locked == FALSE && app_data->auto_lock == TRUE) {
        lock_app (NULL, app_data);
    }
}


gboolean
check_inactivity (gpointer user_data)
{
    AppData *app_data = (AppData *)user_data;
    if (app_data->inactivity_timeout > 0 && app_data->app_locked == FALSE) {
        GDateTime *now = g_date_time_new_now_local ();
        GTimeSpan diff = g_date_time_difference (now, app_data->last_user_activity);
        if (diff >= (G_USEC_PER_SEC * (GTimeSpan)app_data->inactivity_timeout)) {
            g_signal_emit_by_name (app_data->main_window, "lock-app");
            g_date_time_unref (now);
            return FALSE;
        }
        g_date_time_unref (now);
    }
    return TRUE;
}


void
setup_dbus_listener (AppData *app_data)
{
    g_signal_new ("lock-app", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    g_signal_connect (app_data->main_window, "lock-app", G_CALLBACK(lock_app), app_data);

    gtk_widget_class_add_binding_signal (GTK_WIDGET_CLASS(app_data->main_window), GDK_KEY_l, GDK_CONTROL_MASK, "lock-app", NULL);

    //const gchar *services[] = { "org.cinnamon.ScreenSaver", "org.freedesktop.ScreenSaver", "org.gnome.ScreenSaver", "com.canonical.Unity" };
    const gchar *paths[] = { "/org/cinnamon/ScreenSaver", "/org/freedesktop/ScreenSaver", "/org/gnome/ScreenSaver", "/com/canonical/Unity/Session" };
    const gchar *interfaces[] = { "org.cinnamon.ScreenSaver", "org.freedesktop.ScreenSaver", "org.gnome.ScreenSaver", "com.canonical.Unity.Session" };
    const gchar *signal_names[] = { "ActiveChanged", "ActiveChanged", "ActiveChanged", "Locked" };

    app_data->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

    for (guint i = 0; i < DBUS_SERVICES; i++) {
        app_data->subscription_ids[i] = g_dbus_connection_signal_subscribe (app_data->connection, interfaces[i], interfaces[i], signal_names[i], paths[i],
                                                                            NULL, G_DBUS_SIGNAL_FLAGS_NONE, signal_triggered_cb, app_data, NULL);
    }
}
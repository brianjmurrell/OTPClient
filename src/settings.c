#include <gtk/gtk.h>
#include "otpclient.h"
#include "message-dialogs.h"
#include "get-builder.h"

typedef struct SettingsData_t {
    gchar *cfg_file_path;
    GKeyFile *kf;
    GtkWidget *sno_switch;
    GtkWidget *dn_switch;
    GtkWidget *sc_cb;
    GtkWidget *al_switch;
    GtkWidget *inactivity_cb;
    AppData *app_data;
} SettingsData;

static void run_settings_diag (GtkDialog *dlg __attribute__((unused)),
                               gint       response_id,
                               gpointer   user_data);


void
settings_dialog_cb (GSimpleAction *simple    __attribute__((unused)),
                    GVariant      *parameter __attribute__((unused)),
                    gpointer       user_data)
{
    AppData *app_data = (AppData *)user_data;
    SettingsData *settings_data = g_new0 (SettingsData, 1);

    settings_data->cfg_file_path;
#ifndef USE_FLATPAK_APP_FOLDER
    settings_data->cfg_file_path = g_build_filename (g_get_user_config_dir (), "otpclient.cfg", NULL);
#else
    settings_data->cfg_file_path = g_build_filename (g_get_user_data_dir (), "otpclient.cfg", NULL);
#endif
    GError *err = NULL;
    settings_data->kf = g_key_file_new ();
    if (!g_key_file_load_from_file (settings_data->kf, settings_data->cfg_file_path, G_KEY_FILE_NONE, &err)) {
        gchar *msg = g_strconcat ("Couldn't get data from config file: ", err->message, NULL);
        show_message_dialog (app_data->main_window, msg, GTK_MESSAGE_ERROR);
        g_free (msg);
        g_free (settings_data->cfg_file_path);
        g_key_file_free (settings_data->kf);
        return;
    }

    // if key is not found, g_key_file_get_boolean returns FALSE and g_key_file_get_integer returns 0.
    // Therefore, having these values as default is exactly what we want. So no need to check whether or not the key is missing.
    app_data->show_next_otp = g_key_file_get_boolean (settings_data->kf, "config", "show_next_otp", NULL);
    app_data->disable_notifications = g_key_file_get_boolean (settings_data->kf, "config", "notifications", NULL);
    app_data->search_column = g_key_file_get_integer (settings_data->kf, "config", "search_column", NULL);
    app_data->auto_lock = g_key_file_get_boolean (settings_data->kf, "config", "auto_lock", NULL);
    app_data->inactivity_timeout = g_key_file_get_integer (settings_data->kf, "config", "inactivity_timeout", NULL);

    GtkBuilder *builder = get_builder_from_partial_path(UI_PARTIAL_PATH);
    GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object (builder, "settings_diag_id"));
    settings_data->sno_switch = GTK_WIDGET(gtk_builder_get_object (builder, "nextotp_switch_id"));
    settings_data->dn_switch = GTK_WIDGET(gtk_builder_get_object (builder, "notif_switch_id"));
    settings_data->sc_cb = GTK_WIDGET(gtk_builder_get_object (builder, "search_by_cb_id"));
    settings_data->al_switch = GTK_WIDGET(gtk_builder_get_object (builder, "autolock_switch_id"));
    settings_data->inactivity_cb = GTK_WIDGET(gtk_builder_get_object (builder, "autolock_inactive_cb_id"));

    gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(app_data->main_window));

    gtk_switch_set_active (GTK_SWITCH(settings_data->sno_switch), app_data->show_next_otp);
    gtk_switch_set_active (GTK_SWITCH(settings_data->dn_switch), app_data->disable_notifications);
    gtk_switch_set_active (GTK_SWITCH(settings_data->al_switch), app_data->auto_lock);
    gchar *active_id_string = g_strdup_printf ("%d", app_data->search_column);
    gtk_combo_box_set_active_id (GTK_COMBO_BOX(settings_data->sc_cb), active_id_string);
    g_free (active_id_string);
    active_id_string = g_strdup_printf ("%d", app_data->inactivity_timeout);
    gtk_combo_box_set_active_id (GTK_COMBO_BOX(settings_data->inactivity_cb), active_id_string);
    g_free (active_id_string);

    gtk_widget_show (dialog);

    app_data->loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (dialog, "response", G_CALLBACK(run_settings_diag), settings_data);
    g_main_loop_run (app_data->loop);
    g_main_loop_unref (app_data->loop);
    app_data->loop = NULL;

    g_free (settings_data->cfg_file_path);
    g_key_file_free (settings_data->kf);

    gtk_window_destroy (GTK_WINDOW(dialog));

    g_object_unref (builder);
}


static void
run_settings_diag (GtkDialog *dlg __attribute__((unused)),
                   gint       response_id,
                   gpointer   user_data)
{
    SettingsData *settings_data = (SettingsData *)user_data;
    if (response_id == GTK_RESPONSE_OK) {
        settings_data->app_data->show_next_otp = gtk_switch_get_active (GTK_SWITCH(settings_data->sno_switch));
        settings_data->app_data->disable_notifications = gtk_switch_get_active (GTK_SWITCH(settings_data->dn_switch));
        settings_data->app_data->search_column = (gint) g_ascii_strtoll (gtk_combo_box_get_active_id (GTK_COMBO_BOX(settings_data->sc_cb)), NULL, 10);
        settings_data->app_data->auto_lock = gtk_switch_get_active (GTK_SWITCH(settings_data->al_switch));
        settings_data->app_data->inactivity_timeout = (gint) g_ascii_strtoll (gtk_combo_box_get_active_id (GTK_COMBO_BOX(settings_data->inactivity_cb)), NULL, 10);
        g_key_file_set_boolean (settings_data->kf, "config", "show_next_otp", settings_data->app_data->show_next_otp);
        g_key_file_set_boolean (settings_data->kf, "config", "notifications", settings_data->app_data->disable_notifications);
        g_key_file_set_integer (settings_data->kf, "config", "search_column", settings_data->app_data->search_column);
        g_key_file_set_boolean (settings_data->kf, "config", "auto_lock", settings_data->app_data->auto_lock);
        g_key_file_set_integer (settings_data->kf, "config", "inactivity_timeout", settings_data->app_data->inactivity_timeout);
        g_key_file_save_to_file (settings_data->kf, settings_data->cfg_file_path, NULL);
        gtk_tree_view_set_search_column (GTK_TREE_VIEW(settings_data->app_data->tree_view), settings_data->app_data->search_column + 1);
    }
    if (settings_data->app_data->loop) {
        g_main_loop_quit (settings_data->app_data->loop);
    }
}
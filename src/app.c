#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <jansson.h>
#include "app.h"
#include "data-structs.h"
#include "common/common.h"

static void     create_main_window            (AppData *app_data);

static gboolean migrate_db_path_from_v2_to_v3 (AppData *app_data);

static void     destroy_cb                    (AppData *app_data);


void
activate (GtkApplication    *app,
          gpointer           user_data __attribute__((unused)))
{
    g_autofree gchar *init_msg = init_libs (get_max_file_size_from_memlock());
    if (init_msg != NULL) {
        g_printerr ("ERROR: couldn't initialize GCrypt: %s\n", init_msg);
        g_application_quit (G_APPLICATION(app));
        return;
    }

    AppData *app_data = g_new0 (AppData, 1);
    app_data->db_data = g_new0 (DatabaseData, 1);

    // TODO: remove this function after some time (e.g. v3.1)
    if (!migrate_db_path_from_v2_to_v3 (app_data)) {
        g_free (app_data->db_data);
        g_free (app_data);
        g_application_quit (G_APPLICATION(app));
        return;
    }

    create_main_window (app_data);
    if (app_data->main_window == NULL) {
        g_free (app_data->db_data);
        g_free (app_data);
        g_application_quit (G_APPLICATION(app));
        return;
    }

    app_data->db_data->db_path = g_settings_get_string (app_data->settings, "db-path");
    if (g_utf8_strlen (app_data->db_data->db_path, -1) < 2) {
        g_free (app_data->db_data->db_path);
        // TODO first run: restore or create db? If new, select folder AND ask for pwd 2 time, if restore select file and goto next step
    }

    // TODO: ask for dec pwd and load db. Do the same for restore db of previous step

    g_signal_connect (app_data->main_window, "destroy", G_CALLBACK(destroy_cb), app_data);

    gtk_widget_show (app_data->main_window);
}


static gboolean
migrate_db_path_from_v2_to_v3 (AppData *app_data)
{
    gchar *flatpak_cfg_file_path = g_build_filename (g_get_user_data_dir (), "otpclient.cfg", NULL);
    gchar *distro_cfg_file_path = g_build_filename (g_get_user_config_dir (), "otpclient.cfg", NULL);
    g_autofree gchar *cfg_file_path = NULL;
    if (g_file_test (flatpak_cfg_file_path, G_FILE_TEST_EXISTS)) {
        cfg_file_path = g_strdup (flatpak_cfg_file_path);
        g_free (flatpak_cfg_file_path);
    }
    if (g_file_test (distro_cfg_file_path, G_FILE_TEST_EXISTS)) {
        cfg_file_path = g_strdup (distro_cfg_file_path);
        g_free (distro_cfg_file_path);
    }
    if (cfg_file_path != NULL) {
        GKeyFile *kf = g_key_file_new ();
        if (!g_key_file_load_from_file (kf, cfg_file_path, G_KEY_FILE_NONE, NULL)) {
            g_printerr ("ERROR: couldn't load the config file\n");
            g_key_file_free (kf);
            return FALSE;
        }
        g_autofree gchar *db_path = g_key_file_get_string (kf, "config", "db_path", NULL);
        if (!g_settings_set_string (app_data->settings, "db-path", db_path)) {
            g_printerr ("ERROR: couldn't write the key db-path");
            g_key_file_free (kf);
            return FALSE;
        }
        g_unlink (cfg_file_path);
    }
    // either no cfg file is present (therefore it's either a first run or db has been already migrated) or migration succeeded
    return TRUE;
}


static void
create_main_window (AppData *app_data)
{
    g_autoptr(GtkBuilder) builder = NULL;
    builder = gtk_builder_new_from_resource ("/com/github/paolostivanin/OTPClient/main_window.ui");

    app_data->main_window = GTK_WIDGET(gtk_builder_get_object (builder, "mainwin"));
    if (app_data->main_window == NULL) {
        g_printerr ("ERROR: couldn't create the main window\n");
        return;
    }

    app_data->settings = g_settings_new (APP_ID);
    gint width = g_settings_get_int (app_data->settings, "window-width");
    gint height = g_settings_get_int (app_data->settings, "window-height");

    if (width > -1 && height > -1) {
        gtk_window_set_default_size (GTK_WINDOW(app_data->main_window), width, height);
    }
}


static void
destroy_cb (AppData *app_data)
{
    gint width = -1, height = -1;
    gtk_window_get_default_size (GTK_WINDOW(app_data->main_window), &width, &height);
    if (width > 0 && height > 0) {
        g_settings_set_int (app_data->settings, "window-width", width);
        g_settings_set_int (app_data->settings, "window-height", height);
    }
    g_free (app_data->db_data->db_path);
}
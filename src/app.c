#include <gtk/gtk.h>
#include <gcrypt.h>
#include <jansson.h>
#include "otpclient.h"
#include "gui-common.h"
#include "gquarks.h"
#include "imports.h"
#include "common/exports.h"
#include "message-dialogs.h"
#include "password-cb.h"
#include "get-builder.h"
#include "liststore-misc.h"
#include "lock-app.h"
#include "common/common.h"
#include "version.h"

#ifndef USE_FLATPAK_APP_FOLDER
static gchar     *get_db_path               (AppData            *app_data);

static void       fc_dialog_response        (GtkNativeDialog *native,
                                             int              response,
                                             gpointer         user_data);

typedef struct s_get_db_path_data {
    GMainLoop  *loop;
    GtkFileChooser *chooser;
    gchar *db_path;
    GError *err;
    GKeyFile *kf;
    gchar *cfg_file_path;
} GetDBPathData;
#endif

static GKeyFile  *get_kf_ptr                (void);

static void       get_wh_data               (gint               *width,
                                             gint               *height,
                                             AppData            *app_data);

static gboolean   get_warn_data             (void);

static void       set_warn_data             (GtkDialog          *dlg,
                                             gint                response_id,
                                             gpointer            user_data);

static void       create_main_window        (GtkBuilder         *builder,
                                             gint                width,
                                             gint                height,
                                             AppData            *app_data);

static gboolean   set_action_group          (GtkBuilder         *builder,
                                             AppData            *app_data);

static void       get_window_size_cb        (GtkWidget          *window,
                                             GtkAllocation      *allocation,
                                             gpointer            user_data);

static void       toggle_delete_button_cb   (GtkWidget          *main_window,
                                             gpointer            user_data);

static void       del_data_cb               (GtkToggleButton    *btn,
                                             gpointer            user_data);

static void       change_password_cb        (GSimpleAction      *simple,
                                             GVariant           *parameter,
                                             gpointer            user_data);

static void       save_sort_order           (GtkTreeView        *tree_view);

static void       save_window_size          (gint                width,
                                             gint                height);

static void       store_data                (const gchar        *param1_name,
                                             gint                param1_value,
                                             const gchar        *param2_name,
                                             gint                param2_value);

static gboolean   key_pressed_cb            (GtkEventControllerKey *controller,
                                             guint                  keyval,
                                             guint                  keycode,
                                             GdkModifierType        state,
                                             gpointer               user_data);

static gboolean   show_memlock_warn_dialog  (gint32              max_file_size,
                                             GtkBuilder         *builder);

static void       set_open_db_action        (GtkWidget          *btn,
                                             gpointer            user_data);

static void       cleanup_destroy_diag_cb   (GtkDialog          *dlg,
                                             gint                response_id,
                                             gpointer            user_data);


void
activate (GtkApplication    *app,
          gpointer           user_data __attribute__((unused)))
{
    gint32 max_file_size = get_max_file_size_from_memlock ();

    AppData *app_data = g_new0 (AppData, 1);

    app_data->app_locked = FALSE;

    gint width = 0, height = 0;
    app_data->show_next_otp = FALSE; // next otp not shown by default
    app_data->disable_notifications = FALSE; // notifications enabled by default
    app_data->search_column = 0; // account
    app_data->auto_lock = FALSE; // disabled by default
    app_data->inactivity_timeout = 0; // never
    // open_db_file_action is set only on first startup and not when the db is deleted but the cfg file is there, therefore we need a default action
    app_data->open_db_file_action = GTK_FILE_CHOOSER_ACTION_SAVE;
    get_wh_data (&width, &height, app_data);

    app_data->db_data = g_new0 (DatabaseData, 1);

    g_autoptr(GtkBuilder) main_builder = NULL;
    main_builder = get_builder_from_partial_path (g_strconcat (UI_PARTIAL_PATH, "main.ui", NULL));

    create_main_window (main_builder, width, height, app_data);
    if (app_data->main_window == NULL) {
        g_printerr ("Couldn't locate the ui file, exiting...\n");
        g_free (app_data->db_data);
        g_application_quit (G_APPLICATION(app));
        return;
    }
    gtk_application_add_window (GTK_APPLICATION(app), GTK_WINDOW(app_data->main_window));
    g_signal_connect (app_data->main_window, "size-allocate", G_CALLBACK(get_window_size_cb), NULL);

    gchar *init_msg = init_libs (max_file_size);
    if (init_msg != NULL) {
        show_message_dialog (app_data->main_window, init_msg, GTK_MESSAGE_ERROR);
        g_free (init_msg);
        g_free (app_data->db_data);
        g_application_quit (G_APPLICATION(app));
        return;
    }

#ifdef USE_FLATPAK_APP_FOLDER
    app_data->db_data->db_path = g_build_filename (g_get_user_data_dir (), "otpclient-db.enc", NULL);
    // on the first run the cfg file is not created in the flatpak version because we use a non-changeable db path
    gchar *cfg_file_path = g_build_filename (g_get_user_data_dir (), "otpclient.cfg", NULL);
    if (!g_file_test (cfg_file_path, G_FILE_TEST_EXISTS)) {
        g_file_set_contents (cfg_file_path, "[config]", -1, NULL);
    }
    g_free (cfg_file_path);
#else
    g_autoptr(GtkBuilder) misc_diags_builder = NULL;
    misc_diags_builder = get_builder_from_partial_path (g_strconcat (UI_PARTIAL_PATH, "misc_diags.ui", NULL));
    if (!g_file_test (g_build_filename (g_get_user_config_dir (), "otpclient.cfg", NULL), G_FILE_TEST_EXISTS)) {
        app_data->diag_rcdb = GTK_WIDGET(gtk_builder_get_object (misc_diags_builder, "dialog_rcdb_id"));
        GtkWidget *restore_btn = GTK_WIDGET(gtk_builder_get_object (misc_diags_builder, "diag_rc_restoredb_btn_id"));
        GtkWidget *create_btn = GTK_WIDGET(gtk_builder_get_object (misc_diags_builder, "diag_rc_createdb_btn_id"));
        gtk_window_set_transient_for (GTK_WINDOW(app_data->diag_rcdb), GTK_WINDOW(app_data->main_window));
        gtk_widget_show (app_data->diag_rcdb);

        g_signal_connect (restore_btn, "clicked", G_CALLBACK(set_open_db_action), app_data);
        g_signal_connect (create_btn, "clicked", G_CALLBACK(set_open_db_action), app_data);
        app_data->loop = g_main_loop_new (NULL, FALSE);
        g_signal_connect (app_data->diag_rcdb, "response", G_CALLBACK(cleanup_destroy_diag_cb), app_data);
        g_main_loop_run (app_data->loop);
        g_main_loop_unref (app_data->loop);
        app_data->loop = NULL;
        if (app_data->quit_app == TRUE) {
            g_application_quit (G_APPLICATION(app));
            return;
        }
    }

    app_data->db_data->db_path = get_db_path (app_data);
    if (app_data->db_data->db_path == NULL) {
        g_free (app_data->db_data);
        g_free (app_data);
        g_application_quit (G_APPLICATION(app));
        return;
    }
#endif

    if (max_file_size < (96 * 1024) && get_warn_data () == TRUE) {
        if (show_memlock_warn_dialog (max_file_size, misc_diags_builder) == TRUE) {
            g_free (app_data->db_data);
            g_free (app_data);
            g_application_quit (G_APPLICATION(app));
            return;
        }
    }

    app_data->db_data->max_file_size_from_memlock = max_file_size;
    app_data->db_data->objects_hash = NULL;
    app_data->db_data->data_to_add = NULL;
    // subtract 3 seconds from the current time. Needed for "last_hotp" to be set on the first run
    app_data->db_data->last_hotp_update = g_date_time_add_seconds (g_date_time_new_now_local (), -(G_TIME_SPAN_SECOND * HOTP_RATE_LIMIT_IN_SEC));

    retry:
    app_data->db_data->key = prompt_for_password (app_data, NULL, NULL, FALSE);
    if (app_data->db_data->key == NULL) {
        g_free (app_data->db_data);
        g_free (app_data);
        g_application_quit (G_APPLICATION(app));
        return;
    }

    GError *err = NULL;
    load_db (app_data->db_data, &err);
    if (err != NULL && !g_error_matches (err, missing_file_gquark (), MISSING_FILE_CODE)) {
        show_message_dialog (app_data->main_window, err->message, GTK_MESSAGE_ERROR);
        gcry_free (app_data->db_data->key);
        if (g_error_matches (err, memlock_error_gquark (), MEMLOCK_ERRCODE)) {
            g_free (app_data->db_data);
            g_free (app_data);
            g_clear_error (&err);
            g_application_quit (G_APPLICATION(app));
            return;
        }
        g_clear_error (&err);
        goto retry;
    }

    if (g_error_matches (err, missing_file_gquark(), MISSING_FILE_CODE)) {
        const gchar *msg = "This is the first time you run OTPClient, so you need to <b>add</b> or <b>import</b> some tokens.\n"
        "- to <b>add</b> tokens, please click the + button on the <b>top left</b>.\n"
        "- to <b>import</b> existing tokens, please click the menu button <b>on the top right</b>.\n"
        "\nIf you need more info, please visit the <a href=\"https://github.com/paolostivanin/OTPClient/wiki\">project's wiki</a>";
        show_message_dialog (app_data->main_window, msg, GTK_MESSAGE_INFO);
        GError *tmp_err = NULL;
        update_and_reload_db (app_data, app_data->db_data, FALSE, &tmp_err);
        g_clear_error (&tmp_err);
    }

    app_data->clipboard = gdk_display_get_clipboard (gdk_display_get_default ());

    create_treeview (app_data);

    app_data->notification = g_notification_new ("OTPClient");
    g_notification_set_priority (app_data->notification, G_NOTIFICATION_PRIORITY_NORMAL);
    GIcon *icon = g_themed_icon_new ("com.github.paolostivanin.OTPClient");
    g_notification_set_icon (app_data->notification, icon);
    g_notification_set_body (app_data->notification, "OTP value has been copied to the clipboard");
    g_object_unref (icon);

    GtkToggleButton *del_toggle_btn = GTK_TOGGLE_BUTTON(gtk_builder_get_object (main_builder, "del_toggle_btn_id"));

    g_signal_new ("toggle-delete-button", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    GtkEventController *controller = gtk_event_controller_key_new ();
    g_signal_connect (controller, "key-pressed", G_CALLBACK(key_pressed_cb), app_data->main_window);
    gtk_widget_add_controller (app_data->main_window, controller);

    gtk_widget_class_add_binding_signal (GTK_WIDGET_GET_CLASS(app_data->main_window), GDK_KEY_d, GDK_CONTROL_MASK, "toggle-delete-button", NULL);
    g_signal_connect (app_data->main_window, "toggle-delete-button", G_CALLBACK(toggle_delete_button_cb), del_toggle_btn);
    g_signal_connect (del_toggle_btn, "toggled", G_CALLBACK(del_data_cb), app_data);
    g_signal_connect (app_data->main_window, "destroy", G_CALLBACK(destroy_cb), app_data);

    app_data->source_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 500, traverse_liststore, app_data, NULL);

    setup_dbus_listener (app_data);

    // set last user activity to now, so we have a starting point for the autolock feature
    app_data->last_user_activity = g_date_time_new_now_local ();
    app_data->source_id_last_activity = g_timeout_add_seconds (1, check_inactivity, app_data);
}


static void
cleanup_destroy_diag_cb (GtkDialog *dlg,
                         gint       response_id,
                         gpointer   user_data)
{
    AppData *app_data = (AppData *)user_data;
    if (response_id == GTK_RESPONSE_CANCEL) {
        gtk_window_destroy (GTK_WINDOW(dlg));
        g_free (app_data->db_data);
        g_free (app_data);
    }
    if (response_id == GTK_RESPONSE_OK) {
        gtk_window_destroy (GTK_WINDOW(dlg));
        app_data->quit_app = FALSE;
    }
    if (app_data->loop) {
        g_main_loop_quit (app_data->loop);
    }
}


static gboolean
show_memlock_warn_dialog (gint32      max_file_size,
                          GtkBuilder *builder)
{
    gchar *msg = g_strdup_printf ("Your OS's memlock limit (%d) may be too low for you.\n"
                                  "This could crash the program when importing data from 3rd party apps\n"
                                  "or when a certain amount of tokens is reached.\n"
                                  "Please have a look at the <a href=\"https://github.com/paolostivanin/OTPClient/wiki/Secure-Memory-Limitations\">secure memory wiki</a> page before\n"
                                  "using this software with the current settings.", max_file_size);
    GtkWidget *warn_diag = GTK_WIDGET(gtk_builder_get_object (builder, "warning_diag_id"));
    GtkLabel *warn_label = GTK_LABEL(gtk_builder_get_object (builder, "warning_diag_label_id"));
    GtkWidget *warn_chk_btn = GTK_WIDGET(gtk_builder_get_object (builder, "warning_diag_check_btn_id"));
    gtk_label_set_label (warn_label, msg);
    gtk_widget_show (warn_diag);

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);

    GQueue *queue = g_queue_new ();
    g_queue_push_head (queue, warn_chk_btn);
    g_queue_push_head (queue, loop);

    g_signal_connect (warn_diag, "response", G_CALLBACK(set_warn_data), queue);

    g_main_loop_run (loop);
    g_main_loop_unref (loop);
    loop = NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbad-function-cast"
    gboolean quit = GPOINTER_TO_INT(g_queue_pop_head (queue));
#pragma GCC diagnostic pop
    g_free (msg);
    g_queue_free (queue);

    return quit;
}


static gboolean
key_pressed_cb (GtkEventControllerKey *controller __attribute__((unused)),
                guint                  keyval,
                guint                  keycode    __attribute__((unused)),
                GdkModifierType        state      __attribute__((unused)),
                gpointer               user_data)
{
    if (keyval == GDK_KEY_q) {
        gtk_window_close (GTK_WINDOW(user_data));
    }
    return FALSE;
}


static GKeyFile *
get_kf_ptr ()
{
    GError *err = NULL;
    GKeyFile *kf = g_key_file_new ();
    gchar *cfg_file_path;
#ifndef USE_FLATPAK_APP_FOLDER
    cfg_file_path = g_build_filename (g_get_user_config_dir (), "otpclient.cfg", NULL);
#else
    cfg_file_path = g_build_filename (g_get_user_data_dir (), "otpclient.cfg", NULL);
#endif
    if (g_file_test (cfg_file_path, G_FILE_TEST_EXISTS)) {
        if (g_key_file_load_from_file (kf, cfg_file_path, G_KEY_FILE_NONE, &err)) {
            g_free (cfg_file_path);
            return kf;
        }
        g_printerr ("%s\n", err->message);
    }
    g_free (cfg_file_path);
    g_key_file_free (kf);
    return NULL;
}


static void
get_wh_data (gint     *width,
             gint     *height,
             AppData  *app_data)
{
    GKeyFile *kf = get_kf_ptr ();
    if (kf != NULL) {
        *width = g_key_file_get_integer (kf, "config", "window_width", NULL);
        *height = g_key_file_get_integer (kf, "config", "window_height", NULL);
        app_data->show_next_otp = g_key_file_get_boolean (kf, "config", "show_next_otp", NULL);
        app_data->disable_notifications = g_key_file_get_boolean (kf, "config", "notifications", NULL);
        app_data->search_column = g_key_file_get_integer (kf, "config", "search_column", NULL);
        app_data->auto_lock = g_key_file_get_boolean (kf, "config", "auto_lock", NULL);
        app_data->inactivity_timeout = g_key_file_get_integer (kf, "config", "inactivity_timeout", NULL);
        g_key_file_free (kf);
    }
}


static gboolean
get_warn_data ()
{
    GKeyFile *kf = get_kf_ptr ();
    gboolean show_warning = TRUE;
    GError *err = NULL;
    if (kf != NULL) {
        show_warning = g_key_file_get_boolean (kf, "config", "show_memlock_warning", &err);
        if (err != NULL && (err->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND || err->code == G_KEY_FILE_ERROR_INVALID_VALUE)) {
            // value is not present, so we want to show the warning
            show_warning = TRUE;
        }
        g_key_file_free (kf);
    }

    return show_warning;
}


static void
set_warn_data (GtkDialog *dlg,
               gint       response_id,
               gpointer   user_data)
{
    GQueue *queue = (GQueue *)user_data;
    GMainLoop *loop = g_queue_pop_head (queue);
    GtkWidget *warn_chk_btn = g_queue_pop_head (queue);

    g_queue_push_head (queue, GINT_TO_POINTER(response_id == GTK_RESPONSE_CLOSE ? TRUE : FALSE));

    gboolean show_warning = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(warn_chk_btn));
    GKeyFile *kf = get_kf_ptr ();
    GError *err = NULL;
    if (kf != NULL) {
        g_key_file_set_boolean (kf, "config", "show_memlock_warning", show_warning);
        gchar *cfg_file_path;
#ifndef USE_FLATPAK_APP_FOLDER
        cfg_file_path = g_build_filename (g_get_user_config_dir (), "otpclient.cfg", NULL);
#else
        cfg_file_path = g_build_filename (g_get_user_data_dir (), "otpclient.cfg", NULL);
#endif
        if (!g_key_file_save_to_file (kf, cfg_file_path, &err)) {
            g_printerr ("%s\n", err->message);
        }
        g_free (cfg_file_path);
        g_key_file_free (kf);
    }
    gtk_window_destroy (GTK_WINDOW(dlg));
    if (loop) {
        g_main_loop_quit (loop);
    }
}


static void
create_main_window (GtkBuilder      *builder,
                    gint             width,
                    gint             height,
                    AppData         *app_data)
{
    app_data->main_window = GTK_WIDGET(gtk_builder_get_object (builder, "appwindow_id"));
    gtk_window_set_icon_name (GTK_WINDOW(app_data->main_window), "otpclient");

    gtk_window_set_default_size (GTK_WINDOW(app_data->main_window), (width >= 150) ? width : 500, (height >= 150) ? height : 300);

    GtkWidget *header_bar =  GTK_WIDGET(gtk_builder_get_object (builder, "headerbar_id"));
    GtkWidget *title = gtk_label_new ("OTPClient");
    gtk_widget_add_css_class (title, "title");
    GtkWidget *subtitle = gtk_label_new (PROJECT_VER);
    gtk_widget_add_css_class (subtitle, "subtitle");
    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
    gtk_box_append (GTK_BOX(vbox), title);
    gtk_box_append (GTK_BOX(vbox), subtitle);
    gtk_header_bar_set_title_widget (GTK_HEADER_BAR(header_bar), vbox);

    set_action_group (builder, app_data);
}


static gboolean
set_action_group (GtkBuilder *builder,
                  AppData    *app_data)
{
    static GActionEntry settings_menu_entries[] = {
            { .name = ANDOTP_IMPORT_ACTION_NAME, .activate = select_file_cb },
            { .name = ANDOTP_IMPORT_PLAIN_ACTION_NAME, .activate = select_file_cb },
            { .name = AUTHPLUS_IMPORT_ACTION_NAME, .activate = select_file_cb },
            { .name = FREEOTPPLUS_IMPORT_ACTION_NAME, .activate = select_file_cb },
            { .name = AEGIS_IMPORT_ACTION_NAME, .activate = select_file_cb },
            { .name = ANDOTP_EXPORT_ACTION_NAME, .activate = export_data_cb },
            { .name = ANDOTP_EXPORT_PLAIN_ACTION_NAME, .activate = export_data_cb },
            { .name = FREEOTPPLUS_EXPORT_ACTION_NAME, .activate = export_data_cb },
            { .name = AEGIS_EXPORT_ACTION_NAME, .activate = export_data_cb },
            { .name = "change_pwd", .activate = change_password_cb },
            { .name = "edit_row", .activate = edit_selected_row_cb },
            { .name = "settings", .activate = settings_dialog_cb },
            { .name = "shortcuts", .activate = shortcuts_window_cb },
            { .name = "reset_sort", .activate = reset_column_sorting_cb }
    };

    static GActionEntry add_menu_entries[] = {
            { .name = "webcam", .activate = webcam_cb },
            { .name = "screenshot", .activate = screenshot_cb },
            { .name = "import_qr_file", .activate = add_qr_from_file },
            { .name = "import_qr_clipboard", .activate = add_qr_from_clipboard },
            { .name = "manual", .activate = add_data_dialog }
    };

    GtkWidget *settings_popover = GTK_WIDGET (gtk_builder_get_object (builder, "settings_popover_id"));
    GActionGroup *settings_actions = (GActionGroup *)g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (settings_actions), settings_menu_entries, G_N_ELEMENTS (settings_menu_entries), app_data);
    gtk_widget_insert_action_group (settings_popover, "settings_menu", settings_actions);

    GtkWidget *add_popover = GTK_WIDGET (gtk_builder_get_object (builder, "add_popover_id"));
    GActionGroup *add_actions = (GActionGroup *)g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (add_actions), add_menu_entries, G_N_ELEMENTS (add_menu_entries), app_data);
    gtk_widget_insert_action_group (add_popover, "add_menu", add_actions);

    return TRUE;
}


#ifndef USE_FLATPAK_APP_FOLDER
static gchar *
get_db_path (AppData *app_data)
{
    GetDBPathData *get_db_path_data = g_new0 (GetDBPathData, 1);
    get_db_path_data->db_path = NULL;
    get_db_path_data->err = NULL;
    get_db_path_data->kf = g_key_file_new ();
    get_db_path_data->cfg_file_path = g_build_filename (g_get_user_config_dir (), "otpclient.cfg", NULL);
    if (g_file_test (get_db_path_data->cfg_file_path, G_FILE_TEST_EXISTS)) {
        if (!g_key_file_load_from_file (get_db_path_data->kf, get_db_path_data->cfg_file_path, G_KEY_FILE_NONE, &get_db_path_data->err)) {
            show_message_dialog (app_data->main_window, get_db_path_data->err->message, GTK_MESSAGE_ERROR);
            g_key_file_free (get_db_path_data->kf);
            return NULL;
        }
        get_db_path_data->db_path = g_key_file_get_string (get_db_path_data->kf, "config", "db_path", &get_db_path_data->err);
        if (get_db_path_data->db_path == NULL) {
            goto new_db;
        }
        if (!g_file_test (get_db_path_data->db_path, G_FILE_TEST_EXISTS)) {
            gchar *msg = g_strconcat ("Database file/location:\n<b>", get_db_path_data->db_path, "</b>\ndoes not exist. A new database will be created.", NULL);
            show_message_dialog (app_data->main_window, msg, GTK_MESSAGE_ERROR);
            g_free (msg);
            goto new_db;
        }
        goto end;
    }
    new_db: ; // empty statement workaround
    GtkFileChooserNative *dialog = gtk_file_chooser_native_new ("Select database location",
                                                                GTK_WINDOW (app_data->main_window),
                                                                app_data->open_db_file_action,
                                                                "OK",
                                                                "Cancel");
    get_db_path_data->chooser = GTK_FILE_CHOOSER (dialog);
    gtk_file_chooser_set_select_multiple (get_db_path_data->chooser, FALSE);
    if (app_data->open_db_file_action == GTK_FILE_CHOOSER_ACTION_SAVE) {
        gtk_file_chooser_set_current_name (get_db_path_data->chooser, "NewDatabase.enc");
    }

    get_db_path_data->loop = g_main_loop_new (NULL, FALSE);

    gtk_native_dialog_show (GTK_NATIVE_DIALOG(dialog));
    g_signal_connect (dialog, "response", G_CALLBACK (fc_dialog_response), get_db_path_data);

    g_main_loop_run (get_db_path_data->loop);
    g_main_loop_unref (get_db_path_data->loop);
    get_db_path_data->loop = NULL;

    end:
    g_free (get_db_path_data->cfg_file_path);
    gchar *db_path = g_strdup (get_db_path_data->db_path);
    g_free (get_db_path_data->db_path);
    g_free (get_db_path_data);

    return db_path;
}


static void
fc_dialog_response (GtkNativeDialog *native,
                    int              response,
                    gpointer         user_data)
{
    GetDBPathData *get_db_path_data = (GetDBPathData *)user_data;
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file (get_db_path_data->chooser);
        get_db_path_data->db_path = g_file_get_path (file);
        g_object_unref (file);
        g_key_file_set_string (get_db_path_data->kf, "config", "db_path", get_db_path_data->db_path);
        g_key_file_save_to_file (get_db_path_data->kf, get_db_path_data->cfg_file_path, &get_db_path_data->err);
        if (get_db_path_data->err != NULL) {
            g_printerr ("%s\n", get_db_path_data->err->message);
            g_key_file_free (get_db_path_data->kf);
        }
    }
    g_object_unref (native);
    if (get_db_path_data->loop) {
        g_main_loop_quit (get_db_path_data->loop);
    }
}
#endif


static void
toggle_delete_button_cb (GtkWidget *main_window __attribute__((unused)),
                         gpointer   user_data)
{
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(user_data), !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(user_data)));
}


static void
del_data_cb (GtkToggleButton *btn,
             gpointer         user_data)
{
    AppData *app_data = (AppData *)user_data;

    GtkStyleContext *gsc = gtk_widget_get_style_context (GTK_WIDGET(btn));
    GtkTreeSelection *tree_selection = gtk_tree_view_get_selection (app_data->tree_view);

    if (gtk_toggle_button_get_active (btn)) {
        app_data->css_provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_data (app_data->css_provider, "#delbtn { background: #ff0033; }", -1);
        gtk_style_context_add_provider (gsc, GTK_STYLE_PROVIDER(app_data->css_provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
        const gchar *msg = "You just entered the deletion mode. You can now click on the row(s) you'd like to delete.\n"
            "Please note that once a row has been deleted, <b>it's impossible to recover the associated data.</b>";

        if (get_confirmation_from_dialog (app_data->main_window, msg)) {
            g_signal_handlers_disconnect_by_func (app_data->tree_view, row_selected_cb, app_data);
            // the following function emits the "changed" signal
            gtk_tree_selection_unselect_all (tree_selection);
            // clear all active otps before proceeding to the deletion phase
            g_signal_emit_by_name (app_data->tree_view, "hide-all-otps");
            g_signal_connect (app_data->tree_view, "row-activated", G_CALLBACK(delete_rows_cb), app_data);
        } else {
            gtk_toggle_button_set_active (btn, FALSE);
        }
    } else {
        gtk_style_context_remove_provider (gsc, GTK_STYLE_PROVIDER(app_data->css_provider));
        g_object_unref (app_data->css_provider);
        g_signal_handlers_disconnect_by_func (app_data->tree_view, delete_rows_cb, app_data);
        g_signal_connect (app_data->tree_view, "row-activated", G_CALLBACK(row_selected_cb), app_data);
    }
}


static void
change_password_cb (GSimpleAction *simple    __attribute__((unused)),
                    GVariant      *parameter __attribute__((unused)),
                    gpointer       user_data)
{
    AppData *app_data = (AppData *)user_data;
    gchar *tmp_key = secure_strdup (app_data->db_data->key);
    gchar *pwd = prompt_for_password (app_data, tmp_key, NULL, FALSE);
    if (pwd != NULL) {
        app_data->db_data->key = pwd;
        GError *err = NULL;
        update_and_reload_db (app_data, app_data->db_data, FALSE, &err);
        if (err != NULL) {
            show_message_dialog (app_data->main_window, err->message, GTK_MESSAGE_ERROR);
            GtkApplication *app = gtk_window_get_application (GTK_WINDOW(app_data->main_window));
            destroy_cb (app_data->main_window, app_data);
            g_application_quit (G_APPLICATION(app));
        }
        show_message_dialog (app_data->main_window, "Password successfully changed", GTK_MESSAGE_INFO);
    } else {
        gcry_free (tmp_key);
    }
}


static void
get_window_size_cb (GtkWidget      *window,
                    GtkAllocation  *allocation __attribute__((unused)),
                    gpointer        user_data  __attribute__((unused)))
{
    gint w, h;
    gtk_window_get_default_size (GTK_WINDOW(window), &w, &h);
    g_object_set_data (G_OBJECT(window), "width", GINT_TO_POINTER(w));
    g_object_set_data (G_OBJECT(window), "height", GINT_TO_POINTER(h));
}


void
destroy_cb (GtkWidget   *window,
            gpointer     user_data)
{
    AppData *app_data = (AppData *)user_data;
    save_sort_order (app_data->tree_view);
    g_source_remove (app_data->source_id);
    g_source_remove (app_data->source_id_last_activity);
    g_date_time_unref (app_data->last_user_activity);
    for (gint i = 0; i < DBUS_SERVICES; i++) {
        g_dbus_connection_signal_unsubscribe (app_data->connection, app_data->subscription_ids[i]);
    }
    g_dbus_connection_close (app_data->connection, NULL, NULL, NULL);
    gcry_free (app_data->db_data->key);
    g_free (app_data->db_data->db_path);
    g_slist_free_full (app_data->db_data->objects_hash, g_free);
    json_decref (app_data->db_data->json_data);
    g_free (app_data->db_data);
    gdk_clipboard_set_text (app_data->clipboard, "");
    g_application_withdraw_notification (G_APPLICATION(gtk_window_get_application (GTK_WINDOW(app_data->main_window))), NOTIFICATION_ID);
    g_object_unref (app_data->notification);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbad-function-cast"
    gint w = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(window), "width"));
    gint h = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(window), "height"));
#pragma GCC diagnostic pop
    save_window_size (w, h);
    g_free (app_data);
}


static void
save_sort_order (GtkTreeView *tree_view)
{
    gint id;
    GtkSortType order;
    gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE(GTK_LIST_STORE(gtk_tree_view_get_model (tree_view))), &id, &order);
    // store data only if it was changed
    if (id >= 0) {
        store_data ("column_id", id, "sort_order", order);
    }
}


static void
save_window_size (gint width,
                  gint height)
{
    store_data ("window_width", width, "window_height", height);
}


static void
store_data (const gchar *param1_name,
            gint         param1_value,
            const gchar *param2_name,
            gint         param2_value)
{
    GError *err = NULL;
    GKeyFile *kf = g_key_file_new ();
    gchar *cfg_file_path;
#ifndef USE_FLATPAK_APP_FOLDER
    cfg_file_path = g_build_filename (g_get_user_config_dir (), "otpclient.cfg", NULL);
#else
    cfg_file_path = g_build_filename (g_get_user_data_dir (), "otpclient.cfg", NULL);
#endif
    if (g_file_test (cfg_file_path, G_FILE_TEST_EXISTS)) {
        if (!g_key_file_load_from_file (kf, cfg_file_path, G_KEY_FILE_NONE, &err)) {
            g_printerr ("%s\n", err->message);
        } else {
            g_key_file_set_integer (kf, "config", param1_name, param1_value);
            g_key_file_set_integer (kf, "config", param2_name, param2_value);
            if (!g_key_file_save_to_file (kf, cfg_file_path, &err)) {
                g_printerr ("%s\n", err->message);
            }
        }
    }
    g_key_file_free (kf);
    g_free (cfg_file_path);
}


static void
set_open_db_action (GtkWidget *btn,
                    gpointer   user_data)
{
    AppData *app_data = (AppData *)user_data;
    app_data->open_db_file_action = g_strcmp0 (gtk_widget_get_name (btn), "diag_rc_restoredb_btn") == 0 ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE;
    gtk_dialog_response (GTK_DIALOG(app_data->diag_rcdb), GTK_RESPONSE_OK);
}

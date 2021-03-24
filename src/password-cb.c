#include <gtk/gtk.h>
#include <gcrypt.h>
#include "gui-common.h"
#include "message-dialogs.h"
#include "get-builder.h"
#include "otpclient.h"

typedef struct entry_widgets_t {
    GtkWidget *entry_old;
    GtkWidget *entry1;
    GtkWidget *entry2;
    gboolean retry;
    gboolean pwd_must_be_checked;
    gboolean file_exists;
    gchar *pwd;
    gchar *cur_pwd;
    const gchar *action_name;
    gint diag_ret_code;
    AppData *app_data;
} EntryWidgets;

static void run_dialog    (GtkDialog *dlg,
                           gint       response_id,
                           gpointer   user_data);

static void check_pwd_cb  (GtkWidget *entry,
                           gpointer   user_data);

static void password_cb   (GtkWidget *entry,
                           gpointer  *pwd);


gchar *
prompt_for_password (AppData        *app_data,
                     gchar          *current_key,
                     const gchar    *action_name,
                     gboolean        is_export_pwd)
{
    EntryWidgets *entry_widgets = g_new0 (EntryWidgets, 1);
    entry_widgets->app_data = app_data;
    entry_widgets->retry = FALSE;
    entry_widgets->action_name = action_name;

    GtkBuilder *builder = get_builder_from_partial_path (g_strconcat (UI_PARTIAL_PATH, "pwd_dialogs.ui", NULL));
    GtkWidget *dialog;

    entry_widgets->pwd_must_be_checked = TRUE;
    entry_widgets->file_exists = g_file_test (app_data->db_data->db_path, G_FILE_TEST_EXISTS);
    if ((entry_widgets->file_exists == TRUE || entry_widgets->action_name != NULL) && current_key == NULL && is_export_pwd == FALSE) {
        // decrypt dialog, just one field
        entry_widgets->pwd_must_be_checked = FALSE;
        dialog = GTK_WIDGET(gtk_builder_get_object (builder, "decpwd_diag_id"));
        gchar *text = NULL, *markup = NULL;
        if (entry_widgets->action_name == NULL){
            markup = g_markup_printf_escaped ("%s <span font_family=\"monospace\">%s</span>", "Enter the decryption password for\n", app_data->db_data->db_path);
        } else {
            text = g_strdup ("Enter the decryption password");
        }
        GtkLabel *label = GTK_LABEL(gtk_builder_get_object (builder, "decpwd_label_id"));
        if (markup != NULL) {
            gtk_label_set_markup (label, markup);
            g_free (markup);
        } else {
            gtk_label_set_text (label, text);
            g_free (text);
        }
        entry_widgets->entry1 = GTK_WIDGET(gtk_builder_get_object (builder,"decpwddiag_entry_id"));
        g_signal_connect (entry_widgets->entry1, "activate", G_CALLBACK (send_ok_cb), NULL);
    } else if ((entry_widgets->file_exists == FALSE && current_key == NULL) || is_export_pwd == TRUE) {
        // new db dialog, 2 fields
        dialog = GTK_WIDGET(gtk_builder_get_object (builder, "newdb_pwd_diag_id"));
        entry_widgets->entry1 = GTK_WIDGET(gtk_builder_get_object (builder,"newdb_pwd_diag_entry1_id"));
        entry_widgets->entry2 = GTK_WIDGET(gtk_builder_get_object (builder,"newdb_pwd_diag_entry2_id"));
        g_signal_connect (entry_widgets->entry2, "activate", G_CALLBACK (send_ok_cb), NULL);
    } else {
        // change pwd dialog, 3 fields
        if (current_key == NULL) {
            show_message_dialog (app_data->main_window, "ERROR: current_key cannot be NULL", GTK_MESSAGE_ERROR);
            g_free (entry_widgets);
            g_object_unref (builder);
            return NULL;
        }
        dialog = GTK_WIDGET(gtk_builder_get_object (builder, "changepwd_diag_id"));
        entry_widgets->cur_pwd = secure_strdup (current_key);
        entry_widgets->entry_old = GTK_WIDGET(gtk_builder_get_object (builder,"changepwd_diag_currententry_id"));
        entry_widgets->entry1 = GTK_WIDGET(gtk_builder_get_object (builder,"changepwd_diag_newentry1_id"));
        entry_widgets->entry2 = GTK_WIDGET(gtk_builder_get_object (builder,"changepwd_diag_newentry2_id"));
        g_signal_connect (entry_widgets->entry2, "activate", G_CALLBACK (send_ok_cb), NULL);
    }

    gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(app_data->main_window));

    do {
        app_data->loop = g_main_loop_new (NULL, FALSE);
        g_signal_connect (dialog, "response", G_CALLBACK(run_dialog), entry_widgets);
        g_main_loop_run (app_data->loop);
        g_main_loop_unref (app_data->loop);
        app_data->loop = NULL;
    } while (entry_widgets->diag_ret_code == GTK_RESPONSE_OK && entry_widgets->retry == TRUE);

    gchar *pwd = NULL;
    if (entry_widgets->pwd != NULL) {
        gcry_free (current_key);
        gsize len = strlen (entry_widgets->pwd) + 1;
        pwd = gcry_calloc_secure (len, 1);
        strncpy (pwd, entry_widgets->pwd, len);
        gcry_free (entry_widgets->pwd);
    }
    if (entry_widgets->cur_pwd != NULL) {
        gcry_free (entry_widgets->cur_pwd);
    }

    g_free (entry_widgets);

    gtk_window_destroy (GTK_WINDOW(dialog));

    g_object_unref (builder);

    return pwd;
}


static void
run_dialog (GtkDialog *dlg __attribute__((unused)),
            gint       response_id,
            gpointer   user_data)
{
    EntryWidgets *entry_widgets = (EntryWidgets *)user_data;
    entry_widgets->diag_ret_code = response_id;
    if (response_id == GTK_RESPONSE_OK) {
        if ((entry_widgets->file_exists == TRUE || entry_widgets->action_name != NULL) && entry_widgets->pwd_must_be_checked == FALSE) {
            password_cb (entry_widgets->entry1, (gpointer *)&entry_widgets->pwd);
            entry_widgets->diag_ret_code = GTK_RESPONSE_CLOSE;
        } else {
            check_pwd_cb (entry_widgets->entry1, (gpointer)entry_widgets);
        }
    }
    if (entry_widgets->app_data->loop) {
        g_main_loop_quit (entry_widgets->app_data->loop);
    }
}


static void
check_pwd_cb (GtkWidget   *entry,
              gpointer     user_data)
{
    EntryWidgets *entry_widgets = (EntryWidgets *) user_data;
    if (entry_widgets->cur_pwd != NULL && g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE(entry_widgets->entry_old)), entry_widgets->cur_pwd) != 0) {
        show_message_dialog (GTK_WIDGET(gtk_widget_get_root (entry)), "Old password doesn't match", GTK_MESSAGE_ERROR);
        entry_widgets->retry = TRUE;
        return;
    }
    if (gtk_entry_get_text_length (GTK_ENTRY(entry_widgets->entry1)) < 6) {
        show_message_dialog (GTK_WIDGET(gtk_widget_get_root (entry)), "Password must be at least 6 characters.", GTK_MESSAGE_ERROR);
        entry_widgets->retry = TRUE;
        return;
    }
    if (g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE(entry_widgets->entry1)), gtk_editable_get_text (GTK_EDITABLE(entry_widgets->entry2))) == 0) {
        password_cb (entry, (gpointer *)&entry_widgets->pwd);
        entry_widgets->retry = FALSE;
    } else {
        show_message_dialog (GTK_WIDGET(gtk_widget_get_root (entry)), "Passwords mismatch", GTK_MESSAGE_ERROR);
        entry_widgets->retry = TRUE;
    }
}


static void
password_cb (GtkWidget  *entry,
             gpointer   *pwd)
{
    const gchar *text = gtk_editable_get_text (GTK_EDITABLE(entry));
    gsize len = strlen (text) + 1;
    *pwd = gcry_calloc_secure (len, 1);
    strncpy (*pwd, text, len);
    GtkWidget *top_level = GTK_WIDGET(gtk_widget_get_root (entry));
    gtk_dialog_response (GTK_DIALOG (top_level), GTK_RESPONSE_CLOSE);
}

#include <gtk/gtk.h>
#include <zbar.h>
#include <gcrypt.h>
#include "gui-common.h"
#include "imports.h"
#include "parse-uri.h"
#include "message-dialogs.h"
#include "add-common.h"
#include "get-builder.h"


typedef struct config_data {
    GtkWidget *diag;
    gchar *otp_uri;
    gboolean qrcode_found;
    gboolean gtimeout_exit_value;
    guint counter;
    guint source_id;
    zbar_processor_t *proc;
    AppData *app_data;
} ConfigData;

static void     run_webcam_diag (GtkDialog *dlg,
                                 gint       response_id,
                                 gpointer   user_data);

static gboolean check_result    (gpointer        data);

static void     scan_qrcode     (zbar_image_t   *image,
                                 gconstpointer   user_data);


void
webcam_cb (GSimpleAction *simple    __attribute__((unused)),
           GVariant      *parameter __attribute__((unused)),
           gpointer       user_data)
{
    AppData *app_data = (AppData *)user_data;

    ConfigData *cfg_data = g_new0 (ConfigData, 1);
    cfg_data->app_data = app_data;

    GtkBuilder *builder = get_builder_from_partial_path (g_strconcat (UI_PARTIAL_PATH, "add_token_dialogs", NULL));
    cfg_data->diag = GTK_WIDGET(gtk_builder_get_object (builder, "diag_webcam_id"));

    gtk_window_set_transient_for (GTK_WINDOW(cfg_data->diag), GTK_WINDOW(app_data->main_window));

    cfg_data->qrcode_found = FALSE;
    cfg_data->gtimeout_exit_value = TRUE;
    cfg_data->counter = 0;

    cfg_data->proc = zbar_processor_create (1);
    zbar_processor_set_config (cfg_data->proc, ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
    if (zbar_processor_init (cfg_data->proc, "/dev/video0", 1)) {
        show_message_dialog (app_data->main_window, "Couldn't initialize the webcam", GTK_MESSAGE_ERROR);
        zbar_processor_destroy (cfg_data->proc);
        g_free (cfg_data);
        return;
    }
    zbar_processor_set_data_handler (cfg_data->proc, scan_qrcode, cfg_data);
    zbar_processor_set_visible (cfg_data->proc, 0);
    zbar_processor_set_active (cfg_data->proc, 1);

    cfg_data->source_id = g_timeout_add (1000, check_result, cfg_data);

    gtk_widget_show (cfg_data->diag);

    app_data->loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (cfg_data->diag, "response", G_CALLBACK(run_webcam_diag), cfg_data);
    g_main_loop_run (app_data->loop);
    g_main_loop_unref (app_data->loop);
    app_data->loop = NULL;

    g_free (cfg_data);

    g_object_unref (builder);
}


static void
run_webcam_diag (GtkDialog *dlg __attribute__((unused)),
                 gint       response_id,
                 gpointer   user_data)
{
    ConfigData *cfg_data = (ConfigData *)user_data;
    if (response_id == GTK_RESPONSE_CANCEL) {
        if (cfg_data->qrcode_found) {
            gchar *err_msg = add_data_to_db (cfg_data->otp_uri, cfg_data->app_data);
            if (err_msg != NULL) {
                show_message_dialog (cfg_data->app_data->main_window, err_msg, GTK_MESSAGE_ERROR);
                g_free (err_msg);
            } else {
                show_message_dialog (cfg_data->app_data->main_window, "QRCode successfully scanned", GTK_MESSAGE_INFO);
            }
            gcry_free (cfg_data->otp_uri);
        }
        zbar_processor_destroy (cfg_data->proc);
        if (cfg_data->gtimeout_exit_value) {
            // only remove if 'check_result' returned TRUE
            g_source_remove (cfg_data->source_id);
        }
        gtk_window_destroy (GTK_WINDOW(cfg_data->diag));
    }
    if (cfg_data->app_data->loop) {
        g_main_loop_quit (cfg_data->app_data->loop);
    }
}


static gboolean
check_result (gpointer data)
{
    ConfigData *cfg_data = (ConfigData *)data;
    if (cfg_data->qrcode_found || cfg_data->counter > 30) {
        gtk_dialog_response (GTK_DIALOG (cfg_data->diag), GTK_RESPONSE_CANCEL);
        cfg_data->gtimeout_exit_value = FALSE;
        return FALSE;
    }
    cfg_data->counter++;
    return TRUE;
}


static void
scan_qrcode (zbar_image_t   *image,
             gconstpointer   user_data)
{
    ConfigData *cfg_data = (ConfigData *)user_data;
    const zbar_symbol_t *symbol = zbar_image_first_symbol (image);
    for (; symbol; symbol = zbar_symbol_next (symbol)) {
        cfg_data->otp_uri = secure_strdup (zbar_symbol_get_data (symbol));
        cfg_data->qrcode_found = TRUE;
    }
}

#include <gtk/gtk.h>
#include <gcrypt.h>
#include <glib/gstdio.h>
#include "imports.h"
#include "qrcode-parser.h"
#include "message-dialogs.h"
#include "add-common.h"
#include "get-builder.h"

typedef struct s_gtimeout_data {
    GtkWidget *diag;
    GdkPixbuf *pb;
    gboolean image_available;
    gboolean gtimeout_exit_value;
    guint counter;
    guint source_id;
    AppData * app_data;
} GTimeoutCBData;

static void     add_qr_file_diag_cb      (GtkDialog          *dlg,
                                          gint                response_id,
                                          gpointer            user_data);

static void     add_qr_cboard_diag_cb    (GtkDialog          *dlg,
                                          gint                response_id,
                                          gpointer            user_data);

static gboolean check_result             (gpointer            data);

static void     get_data_from_cb         (GdkContentProvider *content,
                                          gpointer            user_data);

static void     parse_file_and_update_db (const gchar        *filename,
                                          AppData            *app_data);

static void     save_pb_and_parse_png    (GdkPixbuf          *pixbuf,
                                          AppData            *app_data);


void
add_qr_from_file (GSimpleAction *simple    __attribute__((unused)),
                  GVariant      *parameter __attribute__((unused)),
                  gpointer       user_data)
{
    // use this function to open a file with a qr code and parse it
    AppData *app_data = (AppData *)user_data;

    GtkFileChooserNative *dialog = gtk_file_chooser_native_new ("Open File",
                                                     GTK_WINDOW (app_data->main_window),
                                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                                     "Open",
                                                     "Cancel");

    GtkFileFilter *filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "QR Image (*.png)");
    gtk_file_filter_add_pattern (filter, "*.png");
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

    app_data->loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (dialog, "response", G_CALLBACK(add_qr_file_diag_cb), app_data);
    g_main_loop_run (app_data->loop);
    g_main_loop_unref (app_data->loop);
    app_data->loop = NULL;
    g_object_unref (dialog);
}


void
add_qr_from_clipboard (GSimpleAction *simple    __attribute__((unused)),
                       GVariant      *parameter __attribute__((unused)),
                       gpointer       user_data)
{
    // use this function to parse the QR code directly from the clipboard (when an image is copied)
    AppData *app_data = (AppData *)user_data;
    GTimeoutCBData *gt_cb_data = g_new0 (GTimeoutCBData, 1);
    gt_cb_data->image_available = FALSE;
    gt_cb_data->gtimeout_exit_value = TRUE;
    gt_cb_data->counter = 0;
    gt_cb_data->app_data = app_data;
    gt_cb_data->source_id = g_timeout_add (1000, check_result, gt_cb_data);

    GtkBuilder *builder = get_builder_from_partial_path (g_strconcat (UI_PARTIAL_PATH, "add_token_dialogs.ui", NULL));
    gt_cb_data->diag = GTK_WIDGET(gtk_builder_get_object (builder, "diag_qr_clipboard_id"));
    gtk_widget_show (gt_cb_data->diag);

    app_data->loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (gt_cb_data->diag, "response", G_CALLBACK(add_qr_cboard_diag_cb), gt_cb_data);
    g_main_loop_run (app_data->loop);
    g_main_loop_unref (app_data->loop);
    app_data->loop = NULL;
    g_object_unref (builder);
}


static void
add_qr_file_diag_cb (GtkDialog *dlg,
                     gint       response_id,
                     gpointer   user_data)
{
    AppData *app_data = (AppData *)user_data;
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GFile *fl = gtk_file_chooser_get_file (GTK_FILE_CHOOSER(dlg));
        gchar *abs_path = g_file_get_path (fl);
        parse_file_and_update_db (abs_path, app_data);
        g_object_unref (fl);
        g_free (abs_path);
    }
    if (app_data->loop) {
        g_main_loop_quit (app_data->loop);
    }
}


static void
add_qr_cboard_diag_cb (GtkDialog *dlg,
                       gint       response_id,
                       gpointer   user_data)
{
    GTimeoutCBData *gt_cb_data = (GTimeoutCBData *)user_data;
    if (response_id == GTK_RESPONSE_CANCEL) {
        if (gt_cb_data->image_available == TRUE) {
            save_pb_and_parse_png (gt_cb_data->pb, gt_cb_data->app_data);
        }
        if (gt_cb_data->gtimeout_exit_value == TRUE) {
            // only remove if 'check_result' returned TRUE
            g_source_remove (gt_cb_data->source_id);
        }
        gtk_window_destroy (GTK_WINDOW(dlg));
        g_free (gt_cb_data);
    }
    if (gt_cb_data->app_data->loop) {
        g_main_loop_quit (gt_cb_data->app_data->loop);
    }
}


static gboolean
check_result (gpointer data)
{
    GTimeoutCBData *gt_cb_data = (GTimeoutCBData *)data;
    GdkContentProvider *content = gdk_clipboard_get_content (gt_cb_data->app_data->clipboard);
    if (content != NULL) {
        get_data_from_cb (content, data);
    }
    if (gt_cb_data->counter > 30 || gt_cb_data->image_available == TRUE) {
        gtk_dialog_response (GTK_DIALOG (gt_cb_data->diag), GTK_RESPONSE_CANCEL);
        gt_cb_data->gtimeout_exit_value = FALSE;
        return FALSE;
    }
    gt_cb_data->counter++;
    return TRUE;
}


static void
get_data_from_cb (GdkContentProvider *content,
                  gpointer user_data)
{
    GTimeoutCBData *gt_cb_data = (GTimeoutCBData *)user_data;
    GError  *err = NULL;
    GValue value_pxb = G_VALUE_INIT;
    g_value_init (&value_pxb, GDK_TYPE_PIXBUF);

    if (!gdk_content_provider_get_value (content, &value_pxb, NULL)) {
        gt_cb_data->pb = g_value_get_object (&value_pxb);
        gt_cb_data->image_available = TRUE;
        g_value_unset (&value_pxb);
    } else {
        g_printerr ("Clipboard contains neither an URI nor an image: %s\n", err->message);
        // what now?
    }
}


static void
save_pb_and_parse_png (GdkPixbuf *pixbuf,
                       AppData   *app_data)
{
    GError  *err = NULL;
    if (pixbuf != NULL) {
        gchar *filename = g_build_filename (g_get_tmp_dir (), "qrcode_from_cb.png", NULL);
        gdk_pixbuf_save (pixbuf, filename, "png", &err, NULL);
        if (err != NULL) {
            gchar *msg = g_strconcat ("Couldn't save clipboard to png:\n", err->message, NULL);
            show_message_dialog (app_data->main_window, msg, GTK_MESSAGE_ERROR);
            g_free (msg);
        } else {
            parse_file_and_update_db (filename, app_data);
        }
        g_unlink (filename);
        g_free (filename);
    } else {
        show_message_dialog (app_data->main_window, "Couldn't get QR code image from clipboard", GTK_MESSAGE_ERROR);
    }
}


static void
parse_file_and_update_db (const gchar *filename,
                          AppData     *app_data)
{
    gchar *otpauth_uri = NULL;
    gchar *err_msg = parse_qrcode (filename, &otpauth_uri);
    if (err_msg != NULL) {
        show_message_dialog(app_data->main_window, err_msg, GTK_MESSAGE_ERROR);
        g_free(err_msg);
        return;
    }

    err_msg = add_data_to_db (otpauth_uri, app_data);
    if (err_msg != NULL) {
        show_message_dialog (app_data->main_window, err_msg, GTK_MESSAGE_ERROR);
        g_free (err_msg);
    } else {
        show_message_dialog (app_data->main_window, "QRCode successfully imported from the screenshot", GTK_MESSAGE_INFO);
    }
    gcry_free (otpauth_uri);
}

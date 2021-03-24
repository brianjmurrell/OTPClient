#include <gtk/gtk.h>

typedef struct ConfdiagData_t {
    GMainLoop *loop;
    gboolean confirm;
} ConfDiagData;

static void run_conf_diag (GtkDialog *dlg,
                           gint       response_id,
                           gpointer   user_data);


void
show_message_dialog (GtkWidget      *parent,
                     const gchar    *message,
                     GtkMessageType  message_type)
{
    static GtkWidget *dialog = NULL;

    dialog = gtk_message_dialog_new (parent == NULL ? NULL : GTK_WINDOW(parent),
                                     GTK_DIALOG_MODAL, message_type, GTK_BUTTONS_OK, "%s", message);
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG(dialog), message);

    g_signal_connect (dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
}


gboolean
get_confirmation_from_dialog (GtkWidget     *parent,
                              const gchar   *message)
{
    static GtkWidget *dialog = NULL;
    ConfDiagData *conf_diag_data = g_new0 (ConfDiagData, 1);

    dialog = gtk_dialog_new_with_buttons ("Confirm", GTK_WINDOW (parent), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                          "OK", GTK_RESPONSE_OK, "Cancel", GTK_RESPONSE_CANCEL,
                                           NULL);

    GtkStyleContext *gsc = gtk_widget_get_style_context (GTK_WIDGET(dialog));
    GtkCssProvider *css_pv = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (css_pv, "border-width: 5", -1);
    gtk_style_context_add_provider (gsc, GTK_STYLE_PROVIDER(css_pv), GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    GtkWidget *label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), message);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_box_append (GTK_BOX(content_area), label);

    gtk_widget_show (dialog);

    conf_diag_data->loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (dialog, "response", G_CALLBACK(run_conf_diag), conf_diag_data);
    g_main_loop_run (conf_diag_data->loop);
    g_main_loop_unref (conf_diag_data->loop);
    conf_diag_data->loop = NULL;

    gboolean r = conf_diag_data->confirm;

    gtk_window_destroy (GTK_WINDOW(dialog));

    gtk_style_context_remove_provider (gsc, GTK_STYLE_PROVIDER(css_pv));
    g_object_unref (css_pv);
    g_free (conf_diag_data);

    return r;
}


static void
run_conf_diag (GtkDialog *dlg __attribute__((unused)),
               gint       response_id,
               gpointer   user_data)
{
    ConfDiagData *conf_diag_data = (ConfDiagData *)user_data;
    response_id == GTK_RESPONSE_OK ? conf_diag_data->confirm = TRUE : FALSE;
    if (conf_diag_data->loop) {
        g_main_loop_quit (conf_diag_data->loop);
    }
}
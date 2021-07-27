#include <gtk/gtk.h>
#include "app.h"
#include "version.h"

gint
main (gint    argc,
      gchar **argv)
{
    GtkApplication *app = gtk_application_new (APP_ID, G_APPLICATION_FLAGS_NONE);
    g_set_application_name (PROJECT_NAME);

    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

    gint status = g_application_run (G_APPLICATION(app), argc, argv);

    g_object_unref (app);

    return status;
}

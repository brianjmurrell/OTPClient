#include <gtk/gtk.h>
#include <gcrypt.h>
#include <jansson.h>
#include "app.h"
#include "common/common.h"

void
activate (GtkApplication    *app,
          gpointer           user_data __attribute__((unused)))
{
    g_autofree gchar *init_msg = init_libs (get_max_file_size_from_memlock());
    if (init_msg != NULL) {
        g_printerr ("Error while initalizing Gcrypt: %s\n", init_msg);
        return;
    }
/* 2. create mainwin, so we can show a dialog
 * 3. check if cfg file exists
 *      -> if not, show first run dialog
 *          + ask user if he/she wants to create a new db OR restore an existing db
 *              - if new, select target folder AND ask for pwd 2 times
 *              - if restore, ask for dec pwd
 *      -> if yes, ask for dec pwd
 * 4. load db and show main win
 */
}

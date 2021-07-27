#pragma once

#include <gtk/gtk.h>
#include <jansson.h>

#define DBUS_SERVICES 4

G_BEGIN_DECLS

typedef struct db_data_t {
    gchar *db_path;

    gchar *key;

    json_t *json_data;

    GSList *objects_hash;

    GSList *data_to_add;

    gint32 max_file_size_from_memlock;

    gchar *last_hotp;
    GDateTime *last_hotp_update;
} DatabaseData;


typedef struct app_data_t {
    GtkWidget *main_window;

    GSettings *settings;

    GdkClipboard *clipboard;

    GNotification *notification;

    DatabaseData *db_data;
} AppData;

G_END_DECLS
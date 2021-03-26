#pragma once

G_BEGIN_DECLS

void activate (GtkApplication *app,
               gpointer        user_data);

void destroy_cb (GtkWidget      *window,
                 gpointer        user_data);

G_END_DECLS
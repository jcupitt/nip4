#ifndef __APP_H
#define __APP_H

#define APP_TYPE (app_get_type())
#define APP NIP4_APP

G_DECLARE_FINAL_TYPE(App, app, NIP4, APP, GtkApplication)

App *app_new(void);
GSettings *app_settings(App *app);

#endif /* __APP_H */

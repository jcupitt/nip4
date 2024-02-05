#ifndef __NIP4_APP_H
#define __NIP4_APP_H

#define APP_TYPE (nip4_app_get_type())
G_DECLARE_FINAL_TYPE(Nip4App, nip4_app, NIP4, APP, GtkApplication)

#define APP NIP4_APP

Nip4App *nip4_app_new(void);

#endif /* __NIP4_APP_H */

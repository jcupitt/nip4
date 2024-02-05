#ifndef __MAIN_WINDOW_H
#define __MAIN_WINDOW_H

#define MAIN_WINDOW_TYPE (main_window_get_type())

#define NIP4_MAIN_WINDOW MAIN_WINDOW

G_DECLARE_FINAL_TYPE(MainWindow, main_window,
	NIP4, MAIN_WINDOW, GtkApplicationWindow)

MainWindow *main_window_new(Nip4App *app);
GSettings *main_window_get_settings(MainWindow *win);
void main_window_set_gfile(MainWindow *win, GFile *gfile);

#endif /* __MAIN_WINDOW_H */

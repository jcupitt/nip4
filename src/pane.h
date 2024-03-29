// shim

typedef enum {
	PANE_HIDE_LEFT,
	PANE_HIDE_RIGHT
} PaneHandedness;

Pane *pane_new(PaneHandedness handedness);

void pane_set_position(Pane *pane, int position);
void pane_set_user_position(Pane *pane, int user_position);
void pane_set_open(Pane *pane, gboolean open);
void pane_set_state(Pane *pane, gboolean open, int user_position);
void pane_set_child(Pane *pane, Panechild *panechild);

gboolean pane_get_open(Pane *pane);
int pane_get_position(Pane *pane);

void pane_animate_closed(Pane *pane);
void pane_animate_open(Pane *pane);

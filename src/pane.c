// shim

#include "nip4.h"

Pane *
pane_new(PaneHandedness handedness)
{
	return NULL;
}

void
pane_set_position(Pane *pane, int position)
{
}

void
pane_set_user_position(Pane *pane, int user_position)
{
}

void
pane_set_open(Pane *pane, gboolean open)
{
}

void
pane_set_state(Pane *pane, gboolean open, int user_position)
{
}

void
pane_set_child(Pane *pane, Panechild *panechild)
{
}

void
pane_animate_closed(Pane *pane)
{
}

void
pane_animate_open(Pane *pane)
{
}

gboolean
pane_get_open(Pane *pane)
{
	return FALSE;
}

int
pane_get_position(Pane *pane)
{
	return 0;
}

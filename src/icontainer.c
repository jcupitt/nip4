// icontainer shim

#include "nip4.h"

void *
icontainer_map(iContainer *icontainer, icontainer_map_fn fn, void *a, void *b)
{
	return NULL;
}

void icontainer_reparent(iContainer *parent, iContainer *child, int pos)
{
}

void icontainer_current(iContainer *parent, iContainer *child)
{
}

void icontainer_pos_sort(iContainer *icontainer)
{
}

// shim for subcolumn

#include "nip4.h"

void *
subcolumn_map(Subcolumn *scol, row_map_fn fn, void *a, void *b)
{
	return icontainer_map(ICONTAINER(scol), (icontainer_map_fn) fn, a, b);
}

Subcolumn *
subcolumn_new(Rhs *rhs, Column *col)
{
	return NULL;
}

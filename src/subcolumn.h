// subcol shim

#define SUBCOLUMN(X) (X)

void *subcolumn_map(Subcolumn *scol, row_map_fn fn, void *a, void *b);
Subcolumn *subcolumn_new(Rhs *rhs, Column *col);

// shim until row.h lands

#define ROW(X) ((Row *) X)

struct _Row {
	Rhs *child_rhs;
	Expr *expr;
	Symbol *sym;
};

void *row_is_selected(Row *row);
void *row_deselect(Row *row);
void *row_select_ensure(Row *row);
void *row_select(Row *row);
void *row_select_extend(Row *row);
void *row_select_toggle(Row *row);
void row_select_modifier(Row *row, guint state);

void row_qualified_name(Row *row, VipsBuf *buf);

Row *row_new(Subcolumn *scol, Symbol *sym, PElement *root);

// shim for expr

struct _Expr {
	Compile *compile;
	Row *row;
};

gboolean expr_dirty(Expr *expt, int serial);

void *expr_error_set(Expr *expr);
void expr_error_clear(Expr *expr);
void expr_error_get(Expr *expr);

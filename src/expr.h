// shim for expr

struct _Expr {
};

int link_serial_new(void);
gboolean expr_dirty(Expr *expt, int serial);

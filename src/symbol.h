// shim for symbol.h

struct _Symbol {
	Expr *expr;
};

void symbol_recalculate_all_force( gboolean now );
void symbol_recalculate_all( void );

Symbol *symbol_new(Compile *compile, const char *name);
gboolean symbol_user_init(Symbol *sym);

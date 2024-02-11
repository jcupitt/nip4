// shim!

#define WORKSPACE(X) ((Workspace *) X)
#define IS_WORKSPACE(X) (true)

struct _Workspace {
	Symbol *sym;

	int compat_major;
	int compat_minor;
};

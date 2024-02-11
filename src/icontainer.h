// shim until icontainer comes

struct _iContainer {
	iObject parent_object;

	iContainer *parent;
	View *temp_view;
	int pos;
};

#define ICONTAINER(X) ((iContainer *) X)

typedef void *(*icontainer_map_fn)(iContainer *,
	void *, void *);
typedef void *(*icontainer_map3_fn)(iContainer *,
	void *, void *, void *);
typedef void *(*icontainer_map4_fn)(iContainer *,
	void *, void *, void *, void *);
typedef void *(*icontainer_map5_fn)(iContainer *,
	void *, void *, void *, void *, void *);

typedef gint (*icontainer_sort_fn)(iContainer *a, iContainer *b);

void *icontainer_map(iContainer *icontainer,
	icontainer_map_fn fn, void *a, void *b);

void icontainer_reparent(iContainer *parent, iContainer *child, int pos);

void icontainer_current(iContainer *parent, iContainer *child);


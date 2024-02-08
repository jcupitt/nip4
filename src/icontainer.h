// shim until icontainer comes

struct _iContainer {
	iObject parent_object;

	View *temp_view;
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

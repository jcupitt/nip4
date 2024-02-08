// shim until heapmodel comes in

struct _Heapmodel {
	gboolean modified;
	Row *row;
};

#define IS_HEAPMODEL(X) (true)
#define HEAPMODEL(X) ((Heapmodel *) X)

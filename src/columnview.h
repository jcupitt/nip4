// shim until columnview arrives

#define IS_COLUMNVIEW(X) (true)
#define COLUMNVIEW(X) ((Columnview *) X)

typedef struct _Columnview Columnview;

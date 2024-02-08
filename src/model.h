// shim until we get model.c in

struct _Model {
	gboolean display;
};

typedef enum {
	MODEL_SCROLL_TOP,
	MODEL_SCROLL_BOTTOM
} ModelScrollPosition;

void model_view_new(Model *, View *);

#define IS_MODEL(X) (true)
#define MODEL(X) ((Model *) X)

// shim!

#define WORKSPACEVIEW(X) ((Workspaceview *) X)
#define IS_WORKSPACEVIEW(X) (true)

void workspaceview_set_label(Workspaceview *wview, GtkWidget *tab_label);

#ifndef __WORKSPACEVIEWLABEL_H
#define __WORKSPACEVIEWLABEL_H

#define WORKSPACEVIEWLABEL_TYPE (workspaceviewlabel_get_type())

G_DECLARE_FINAL_TYPE(Workspaceviewlabel, workspaceviewlabel,
	NIP4, WORKSPACEVIEWLABEL, GtkWidget)

#define WORKSPACEVIEWLABEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), \
		WORKSPACEVIEWLABEL_TYPE, Workspaceviewlabel))

Workspaceviewlabel *workspaceviewlabel_new(Workspaceview *wview);
void workspaceviewlabel_refresh(Workspaceviewlabel *wviewlabel);

#endif /* __WORKSPACEVIEWLABEL_H */

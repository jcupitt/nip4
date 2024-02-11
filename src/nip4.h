/*

	Copyright (C) 1991-2003 The National Gallery
	Copyright (C) 2004-2023 libvips.org

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

#ifndef __NIP4_H
#define __NIP4_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#define APP_PATH "/org/libvips/nip4"

#include <gtk/gtk.h>
#include <vips/vips.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#ifdef ENABLE_NLS

#include <libintl.h>
#define _(String) gettext(String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif

#else /* NLS is disabled */

#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain, String) (String)
#define dcgettext(Domain, String, Type) (String)
#define bindtextdomain(Domain, Directory) (Domain)
#define bind_textdomain_codeset(Domain, Codeset) (Codeset)
#define ngettext(S, P, N) ((N) == 1 ? (S) : (P))

#endif /*ENABLE_NLS*/

/* The tile size for image rendering.
 */
#define TILE_SIZE (256)

/* Cache size -- enough for two 4k displays.
 */
#define MAX_TILES (2 * (4096 / TILE_SIZE) * (2048 / TILE_SIZE))

// smallish static strings
#define MAX_STRSIZE (256)

// number of temp workspaces, items in file menu, etc.
#define MAX_RECENT (10)

/* We use various gtk4 features (GtkInfoBar, GtkDialog) which are going away
 * in gtk5.
 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

// various forward typdefs
typedef struct _View View;
typedef struct _Model Model;
typedef struct _iContainer iContainer;
typedef struct _Heapmodel Heapmodel;
typedef struct _Symbol Symbol;
typedef struct _Row Row;
typedef struct _Expr Expr;
typedef struct _Filemodel Filemodel;
typedef struct _Workspacegroupview Workspacegroupview;
typedef struct _Workspacegroup Workspacegroup;
typedef struct _Workspaceview Workspaceview;
typedef struct _Workspace Workspace;

#include "util.h"
#include "main.h"
#include "watch.h"
#include "path.h"
#include "gtkutil.h"
#include "nip4marshal.h"
#include "iobject.h"
#include "vobject.h"
#include "model.h"
#include "columnview.h"
#include "heapmodel.h"
#include "icontainer.h"
#include "filemodel.h"
#include "view.h"
#include "expr.h"
#include "row.h"
#include "workspacegroupview.h"
#include "workspacegroup.h"
#include "workspaceview.h"
#include "workspace.h"
#include "symbol.h"
#include "link.h"
#include "app.h"
#include "mainwindow.h"

#endif /*__NIP4_H*/

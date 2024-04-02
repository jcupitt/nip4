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
#include <setjmp.h>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

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

/* Scope stack for parser.
 */
#define MAX_SSTACK (40)

/* XML namespace for save files ... note, not nip4! We can't change this.
 */
#define NAMESPACE "http://www.vips.ecs.soton.ac.uk/nip"

/* Max number of args we allow.
 */
#define MAX_SYSTEM (50)

/* Largest imagevec we can make.
 */
#define MAX_VEC (10000)

/* How much we decompile for error messages.
 */
#define MAX_ERROR_FRAG (100)

/* Biggest thing we print in trace.
 */
#define MAX_TRACE (1024)

/* Max chars we display of value.
 */
#define MAX_LINELENGTH (120)

/* We use various gtk4 features (GtkInfoBar, GtkDialog) which are going away
 * in gtk5.
 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

// various forward typdefs

typedef struct _BuiltinInfo BuiltinInfo;
typedef struct _Column Column;
typedef struct _Columnview Columnview;
typedef struct _Compile Compile;
typedef struct _Element Element;
typedef struct _Expr Expr;
typedef struct _Classmodel Classmodel;
typedef struct _Filemodel Filemodel;
typedef struct _HeapBlock HeapBlock;
typedef struct _Heap Heap;
typedef struct _Heapmodel Heapmodel;
typedef struct _iContainer iContainer;
typedef struct _Imageinfogroup Imageinfogroup;
typedef struct _Imageinfo Imageinfo;
typedef struct _iText iText;
typedef struct _iImage iImage;
typedef struct _LinkExpr LinkExpr;
typedef struct _Link Link;
typedef struct _Log Log;
typedef struct _Managedfile Managedfile;
typedef struct _Managedgobject Managedgobject;
typedef struct _Managedgvalue Managedgvalue;
typedef struct _Managed Managed;
typedef struct _Managedstring Managedstring;
typedef struct _Matrix Matrix;
typedef struct _Model Model;
typedef struct _Pane Pane;
typedef struct _Panechild Panechild;
typedef struct _ParseConst ParseConst;
typedef struct _ParseNode ParseNode;
typedef struct _Reduce Reduce;
typedef struct _Rhs Rhs;
typedef struct _Row Row;
typedef struct _Rowview Rowview;
typedef struct _Subcolumn Subcolumn;
typedef struct _Subcolumnview Subcolumnview;
typedef struct _Symbol Symbol;
typedef struct _Toolitem Toolitem;
typedef struct _Toolkitbrowser Toolkitbrowser;
typedef struct _Toolkitgroup Toolkitgroup;
typedef struct _Toolkit Toolkit;
typedef struct _Tool Tool;
typedef struct _Trace Trace;
typedef struct _View View;
typedef struct _Watchgroup Watchgroup;
typedef struct _Workspacedefs Workspacedefs;
typedef struct _Workspacegroupview Workspacegroupview;
typedef struct _Workspacegroup Workspacegroup;
typedef struct _Workspaceroot Workspaceroot;
typedef struct _Workspaceview Workspaceview;
typedef struct _Workspace Workspace;

#include "util.h"
#include "app.h"
#include "main.h"
#include "watch.h"
#include "path.h"
#include "parser.h"
#include "tree.h"
#include "gtkutil.h"
#include "predicate.h"
#include "nip4marshal.h"
#include "mainwindow.h"
#include "iobject.h"
#include "vobject.h"
#include "icontainer.h"
#include "model.h"
#include "view.h"
#include "prefs.h"
#include "filemodel.h"
#include "heap.h"
#include "managed.h"
#include "managedstring.h"
#include "managedfile.h"
#include "managedgobject.h"
#include "managedgvalue.h"
#include "imageinfo.h"
#include "secret.h"
#include "compile.h"
#include "action.h"
#include "progress.h"
#include "vipsobject.h"
#include "reduce.h"
#include "heapmodel.h"
#include "classmodel.h"
#include "column.h"
#include "expr.h"
#include "itext.h"
#include "log.h"
#include "trace.h"
#include "row.h"
#include "matrix.h"
#include "rhs.h"
#include "pane.h"
#include "class.h"
#include "tool.h"
#include "toolkit.h"
#include "toolkitgroup.h"
#include "toolkitbrowser.h"
#include "builtin.h"
#include "symbol.h"
#include "dump.h"
#include "predicate.h"
#include "link.h"
#include "workspace.h"
#include "subcolumn.h"
#include "workspaceroot.h"
#include "workspacegroup.h"
#include "columnview.h"
#include "prefcolumnview.h"
#include "prefworkspaceview.h"
#include "workspacegroupview.h"
#include "workspaceview.h"
#include "itextview.h"
#include "toolkitview.h"
#include "toolkitgroupview.h"
#include "subcolumnview.h"
#include "toolview.h"
#include "rhsview.h"

#endif /*__NIP4_H*/

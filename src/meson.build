marshal = gnome.genmarshal(
  'nip4marshal',
  prefix: 'nip4',
  sources: 'nip4marshal.list',
)

resources = gnome.compile_resources(
  'nip4-gresources',
  'gtk/nip4.gresources.xml',
  source_dir: 'gtk',
)

executable('nip4', [
    marshal,
    resources,
    'app.c',
    'expr.c',
    'filemodel.c',
    'gtkutil.c',
    'heapmodel.c',
    'parser.c',
    'icontainer.c',
    'iobject.c',
    'link.c',
    'main.c',
    'mainwindow.c',
    'model.c',
    'path.c',
    'symbol.c',
    'util.c',
    'view.c',
    'vobject.c',
    'column.c',
    'columnview.c',
    'workspaceview.c',
    'workspace.c',
    'workspacegroupview.c',
    'workspacegroup.c',
  ],
  dependencies: nip4_deps,
  win_subsystem: 'windows',
  install: true,
)

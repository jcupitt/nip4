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
    'util.c',
    'gtkutil.c',
    'main.c',
    'app.c',
    'iobject.c',
    'mainwindow.c',
  ],
  dependencies: nip4_deps,
  win_subsystem: 'windows',
  install: true,
)

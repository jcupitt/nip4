
kplot_sources = files (
  'array.c',
  'border.c',
  'bucket.c',
  'buffer.c',
  'colours.c',
  'draw.c',
  'grid.c',
  'hist.c',
  'kdata.c',
  'kplot.c',
  'label.c',
  'margin.c',
  'mean.c',
  'plotctx.c',
  'stddev.c',
  'tic.c',
  'vector.c',
  'reallocarray.c',
)

cairo_dep = dependency('cairo', version: '>=1.0')

# build a static library that nip4 can link to its executable
kplot_lib = static_library('kplot', kplot_sources,
    dependencies: cairo_dep,
    install: false,
)


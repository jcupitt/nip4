project( 'nip4', 'c',
  version: '0.1.0',
  license: 'GPL',
  meson_version: '>=0.56',
  default_options: [
    'c_std=c11',
    # turn off asserts etc. in release mode
    'b_ndebug=if-release'
  ]
)

application_id = 'org.libvips.nip4'

version_parts = meson.project_version().split('.')
version_major = version_parts[0]
version_minor = version_parts[1]
version_patch = version_parts[2]

prefix_dir = get_option('prefix')
lib_dir = prefix_dir / get_option('libdir')

pkg = import('pkgconfig')
gnome = import('gnome')

cc = meson.get_compiler('c')
config_h = configuration_data()

set_defines = [
  ['APPLICATION_ID', application_id],
  ['PACKAGE', meson.project_name()],
  ['VERSION', meson.project_version()],
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')
# so it can find config.h
add_project_arguments('-I.', language: 'c')

nip4_deps = [cc.find_library('m')]

gettext_domain = 'nip4@0@.@1@'.format(version_major, version_minor)
config_h.set_quoted('GETTEXT_PACKAGE', gettext_domain)
config_h.set_quoted('PREFIX', prefix_dir)
config_h.set_quoted('LIBDIR', lib_dir)

if cc.has_function('ngettext')
    config_h.set('ENABLE_NLS', 1)
    have_bind_textdomain_codeset = cc.has_function('bind_textdomain_codeset')
else
    libintl_dep = cc.find_library('intl', required: false)
    if libintl_dep.found()
        nip4_deps += libintl_dep
        config_h.set('ENABLE_NLS', 1)
        have_bind_textdomain_codeset = cc.has_function('bind_textdomain_codeset', prefix: '#include <libintl.h>', dependencies: libintl_dep)
    else
        have_bind_textdomain_codeset = false
    endif
endif
config_h.set('HAVE_BIND_TEXTDOMAIN_CODESET', have_bind_textdomain_codeset)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

# need vips_thread_execute()
nip4_deps += dependency('vips', version: '>=8.15')
nip4_deps += dependency('gtk4')
nip4_deps += dependency('libxml-2.0')

# Courtesy check that the compiler supports the cleanup attribute
glib_dep = dependency('glib-2.0')
if not cc.has_header_symbol('glib.h', 'g_autofree', dependencies : glib_dep)
  error('OpenSlide requires the GNU C "cleanup" attribute.')
endif

install_data(application_id + '.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas')

install_data(application_id + '.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '128x128' / 'apps')
install_data(application_id + '.desktop',
  install_dir: get_option('datadir') / 'applications')
install_data(application_id + '.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo')

meson.add_install_script('meson_post_install.py')

subdir('src')

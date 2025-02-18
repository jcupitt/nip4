variables_data = configuration_data()
variables_data.set('abs_top_srcdir', project_source_root)
variables_data.set('abs_top_builddir', project_build_root)

variables_sh = configure_file(
    input: 'variables.sh.in',
    output: '@BASENAME@',
    configuration: variables_data
)

test('test_all',
    files('test_all.sh'),
    workdir: meson.current_build_dir(),
    # typically 2m or so on a fast machine in debug mode
    timeout: 240,
)


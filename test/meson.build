should_pass_tests = [
  # test workspaces
  'big_and_small_disks.ws',
  'test_colour.ws',
  'test_conv.ws',
  'test_filter.ws',
  'test_fourier.ws',
  'test_histogram.ws',
  'test_image.ws',
  'test_math.ws',
  'test_matrix.ws',
  'test_stats.ws',
  'test_tasks.ws',
  'test_widgets.ws',

  # test the scripting language
  'test_snip.def',
]

should_fail_tests = [
  'test_fail.ws',
]

# all the demo workspaces, except frame and register, since they ise a
# non-free plugin
demos = [
  '1_point_mosaic/1pt_mosaic.ws',
  '2_point_mosaic/2pts_mosaic.ws',
  'businesscard/businesscard.ws',
  'clone/clone.ws',
  'logo/logo2.ws',
  'manual_balance/manual_balance.ws',
  'overlays_and_blending/overlay_blend.ws',
]

foreach name : should_pass_tests
  file = files('workspaces/' + name)
  test(name, nip4_batch, args: ['--test', file])
  test(name, nip4_batch, args: ['--test', file], env: ['VIPS_NOVECTOR=1'])
endforeach

foreach name : should_fail_tests
  file = files('workspaces/' + name)
  test(name, nip4_batch, should_fail: true, args: ['--test', file])
  test(name, nip4_batch, should_fail: true,
    args: ['--test', file],
    env: ['VIPS_NOVECTOR=1'],
  )
endforeach

foreach name : demos
  file = files(project_source_root + '/share/nip4/data/examples/' + name)
  test(name, nip4_batch, args: ['--test', file])
  # repeat, with the vector system disabled
  test(name, nip4_batch, args: ['--test', file], env: ['VIPS_NOVECTOR=1'])
endforeach


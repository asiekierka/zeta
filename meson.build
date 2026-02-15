project(
    'zeta', ['c'],
    version: '1.1.4',
    default_options: ['b_lto=true', 'buildtype=release', 'debug=false', 'optimization=3', 'c_std=gnu23,gnu2x', 'warning_level=3']
)
cc = meson.get_compiler('c')

add_project_arguments('-DZETA_MESON_BUILD', language: 'c')

conf_data = configuration_data()
conf_data.set('version', meson.project_version())

zeta_core_sources = [
  'src/cpu.c',
  'src/zzt.c',
  'src/zzt_ems.c',
  'src/ui.c',
  'src/audio_shared.c',
  'src/audio_stream.c',
  'src/util.c'
]

zeta_frontend_sources = [
  'src/render_software.c',
  'src/audio_writer.c',
  'src/gif_writer.c',
  'src/screenshot_writer.c'
]

zeta_posix_sources = [
  'src/asset_loader.c',
  'src/posix_vfs.c'
]

zeta_curses_sources = [
  'src/frontend_curses.c'
]

zeta_sdl2_sources = [
  'src/sdl2/frontend.c',
  'src/sdl2/render_opengl.c',
  'src/sdl2/render_software.c',
]

zeta_sdl3_sources = [
  'src/sdl3/frontend.c',
  'src/sdl3/render_opengl.c',
  'src/sdl3/render_software.c'
]

math_dep = cc.find_library('m', required: get_option('resampler') == 'bandlimited')
ncurses_dep = dependency('ncurses', required: get_option('frontend') == 'curses')
sdl2_dep = dependency('sdl2', required: get_option('frontend') == 'sdl2')
sdl3_dep = dependency('sdl3', required: get_option('frontend') == 'sdl3')

frontend = get_option('frontend')
if frontend == 'auto'
  if target_machine.system() == 'emscripten'
    frontend = 'wasm'
  elif sdl3_dep.found()
    frontend = 'sdl3'
  elif sdl2_dep.found()
    frontend = 'sdl2'
  else
    error('no compatible frontend supported - try installing SDL or specifying frontend manually')
  endif
endif

full_frontend = false
getopt_required = false
zeta_sources = zeta_core_sources
zeta_dependencies = []
zeta_link_args = []
zeta_filename = 'zeta'

if frontend == 'wasm'
  emscripten_externs = configure_file(input: 'src/emscripten_externs.js', output: '@PLAINNAME@', copy: true)
  add_project_arguments('-s', 'STRICT=1', '-DNO_MEMSET', '-DAVOID_MALLOC', '--no-entry', language: 'c')
  zeta_link_args += [
    '-s', 'STRICT=1', '--no-entry',
    '--closure', '1',
    '-s', 'ENVIRONMENT=web',
    '-s', 'EXPORTED_FUNCTIONS=["_malloc","_free","_audio_generate_init"]',
    '-s', 'EXPORTED_RUNTIME_METHODS=["AsciiToString", "HEAPU8", "HEAPU32"]',
    '-s', 'MODULARIZE=1', '-s', 'EXPORT_NAME="ZetaNative"',
    '-s', 'ALLOW_MEMORY_GROWTH=1',
    '-s', 'MALLOC="emmalloc"', '-s', 'FILESYSTEM=0',
    '-s', 'INITIAL_MEMORY=4194304', '-s', 'STACK_SIZE=262144',
    '-L@0@/src'.format(meson.current_source_dir()), '-lemscripten_glue.js',
    '--closure-args', '--js @0@'.format(emscripten_externs.full_path()),
    '-Wl,-O3'
  ]
  zeta_filename = 'zeta_native'
  if get_option('debug')
    zeta_link_args += ['-s', 'ASSERTIONS=1']
  else
    zeta_link_args += ['-s', 'ASSERTIONS=0']
  endif
elif frontend == 'sdl3'
  zeta_sources += zeta_posix_sources + zeta_frontend_sources + zeta_sdl3_sources
  zeta_dependencies += sdl3_dep
  full_frontend = true
  getopt_required = true
  conf_data.set('USE_SDL', true)
  conf_data.set('USE_SDL3', true)
elif frontend == 'sdl2'
  zeta_sources += zeta_posix_sources + zeta_frontend_sources + zeta_sdl2_sources
  zeta_dependencies += sdl2_dep
  full_frontend = true
  getopt_required = true
  conf_data.set('USE_SDL', true)
  conf_data.set('USE_SDL2', true)
elif frontend == 'curses'
  zeta_sources += zeta_posix_sources + zeta_curses_sources
  zeta_dependencies += ncurses_dep
  getopt_required = true
  conf_data.set('USE_CURSES', true)
else
  error('unsupported frontend specified: @0@', frontend)
endif

have_cosf = cc.has_function('cosf', prefix: '#include <math.h>', dependencies: math_dep)
conf_data.set('HAVE_FTRUNCATE', cc.has_function('ftruncate', prefix: '#include <unistd.h>'))
conf_data.set('HAVE_OPENDIR', cc.has_function('opendir', prefix: '#include <dirent.h>'))

if not cc.has_function('getopt', prefix: '#include <unistd.h>') and getopt_required
  error('getopt is required')
endif

unaligned_ok = target_machine.cpu_family() in ['wasm32', 'wasm64', 'x86', 'x86_64']
if target_machine.cpu_family() == 'aarch64'
  unaligned_ok = cc.has_define('__ARM_FEATURE_UNALIGNED')
endif

conf_data.set('UNALIGNED_OK', unaligned_ok)
conf_data.set('ZETA_BIG_ENDIAN', target_machine.endian() == 'big')

if full_frontend
  libpng_dep = dependency('libpng16', required: false)
  if libpng_dep.found()
    zeta_dependencies += libpng_dep
  endif
  conf_data.set('USE_LIBPNG', libpng_dep.found())

  opengl_dep = dependency('GL', required: false)
  if opengl_dep.found()
    zeta_dependencies += opengl_dep
  endif
  conf_data.set('USE_OPENGL', opengl_dep.found())
endif

font2raw_prog = find_program('tools/font2raw.py')
bin2c_prog = find_program('tools/bin2c.py')

generated_8x8_bin = custom_target(
  'generate-8x8-bin',
  input: 'fonts/pc_cga.png',
  output: '8x8.bin',
  command: [font2raw_prog, '@INPUT@', '8', '8', 'a', '@OUTPUT@'],
  install: false
)

generated_8x8_c = custom_target(
  'generate-8x8-c',
  input: generated_8x8_bin,
  output: '8x8.c',
  command: [bin2c_prog, '--field_name', 'res_8x8_bin', '@OUTPUT@', '@INPUT@'],
  install: false
)

generated_8x14_bin = custom_target(
  'generate-8x14-bin',
  input: 'fonts/pc_ega.png',
  output: '8x14.bin',
  command: [font2raw_prog, '@INPUT@', '8', '14', 'a', '@OUTPUT@'],
  install: false
)

generated_8x14_c = custom_target(
  'generate-8x14-c',
  input: generated_8x14_bin,
  output: '8x14.c',
  command: [bin2c_prog, '--field_name', 'res_8x14_bin', '@OUTPUT@', '@INPUT@'],
  install: false
)

zeta_sources += [generated_8x8_c, generated_8x14_c]

resampler = get_option('resampler')
if resampler == 'auto'
  if have_cosf and frontend != 'wasm'
    resampler = 'bandlimited'
  else
    resampler = 'linear'
  endif
endif
conf_data.set('RESAMPLE_LINEAR', resampler == 'linear')
conf_data.set('RESAMPLE_BANDLIMITED', resampler == 'bandlimited')

if math_dep.found()
  zeta_dependencies += math_dep
endif

if target_machine.system() == 'windows'
  windows = import('windows')
  zeta_sources += windows.compile_resources('mingw/resources.rc')
endif

configure_file(input : 'meson_config.h.in',
               output : 'config_build.h',
               configuration : conf_data)

zeta_exe = executable(zeta_filename, zeta_sources,
  include_directories: include_directories(['src']),
  dependencies: zeta_dependencies,
  link_args: zeta_link_args)

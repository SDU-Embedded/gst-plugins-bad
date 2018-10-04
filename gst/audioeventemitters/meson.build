event_sources = [
  'plugin.c',
  'gstpowereventemitter.c',
  'gstentropyeventemitter.c'
]

gstaudioeventemitters = library('gstaudioeventemitters',
  event_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstpbutils_dep, gstaudio_dep, gstvideo_dep, gstfft_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
pkgconfig.generate(gstaudioeventemitters, install_dir : plugins_pkgconfig_install_dir)

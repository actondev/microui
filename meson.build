project('microui', 'c')

microui = library('microui',
                  files('src/microui.c'),
                  include_directories: 'src',
                 )
microui_dep = declare_dependency(
  link_with: microui,
  include_directories: 'src'
)

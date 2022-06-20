project('microui', 'c')

src = files('src/microui/microui.c')

if get_option('demo')
  src += files('src/microui/demo.c')
endif

microui = library('microui',
                  src,
                  include_directories: 'src',
                  dependencies: [
                    dependency('vgir')
                  ]
                 )
microui_dep = declare_dependency(
  link_with: microui,
  include_directories: 'src'
)

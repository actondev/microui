project('microui', 'cpp')

src = files('src/microui/microui.cpp')

if get_option('demo')
  src += files('src/microui/demo.cpp')
endif

microui = library('microui',
                  src,
                  include_directories: 'src',
                  dependencies: [
                    dependency('vgir'),
                    dependency('fmt'),
                    dependency('range-v3'),
                  ]
                 )
microui_dep = declare_dependency(
  link_with: microui,
  include_directories: 'src'
)

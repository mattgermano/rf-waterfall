project(
  'rf-waterfall',
  'cpp',
  'cuda',
  version: '0.0.1',
  meson_version: '>=1.10.1',
  default_options: {
    'cpp_std': 'c++23',
    'cuda_std': 'c++20',
    'warning_level': '2',
    'werror': true,
    'b_colorout': 'always',
    'b_staticpic': true,
    'strip': true,
  },
)

cuda = import('unstable-cuda')
cuda_args = [cuda.nvcc_arch_flags('13.4', 'Auto')]

rf_waterfall_srcs = files(
  'src/main.cpp',
)

rf_waterfall_cuda_srcs = files()

imgui_backends = files(
  'build/bindings/imgui_impl_glfw.cpp',
  'build/bindings/imgui_impl_opengl3.cpp',
)

rf_waterfall_inc = include_directories(
  'build/bindings',
  'src',
)

rf_waterfall_deps = [
  dependency('imgui'),
  dependency('implot'),
  dependency('glfw3'),
  dependency('nlohmann_json'),
  dependency(
    'cuda',
    version: '>=13.4',
    modules: ['cufft'],
  ),
]

executable(
  meson.project_name(),
  rf_waterfall_srcs,
  rf_waterfall_cuda_srcs,
  imgui_backends,
  include_directories: rf_waterfall_inc,
  implicit_include_directories: false,
  cuda_args: cuda_args,
  dependencies: rf_waterfall_deps,
  install: true,
)

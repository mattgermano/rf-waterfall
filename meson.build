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
        'strip': true
    }
)

cuda = import('unstable-cuda')
cuda_args = [
    cuda.nvcc_arch_flags('13.4')
]

rf_waterfall_srcs = files(
    'src/main.cpp',
)

rf_waterfall_cuda_srcs = files()

imgui_backends = files(
  'build/bindings/imgui_impl_glfw.cpp',
  'build/bindings/imgui_impl_opengl3.cpp',
)

imgui = dependency('imgui', version: '1.92.5')
implot = dependency('implot', version: '0.17')
glfw = dependency('glfw3', version: '3.4')
nlohmann_json = dependency('nlohmann_json', version: '3.12.0')

executable(
    meson.project_name(),
    rf_waterfall_srcs,
    rf_waterfall_cuda_srcs,
    imgui_backends,
    include_directories: include_directories('build/bindings'),
    implicit_include_directories: false,
    cuda_args: cuda_args,
    dependencies: [imgui, implot, glfw, nlohmann_json],
    install: true
)

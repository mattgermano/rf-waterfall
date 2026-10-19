import os
from conan import ConanFile
from conan.tools.files import copy

class RfWaterfallRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "PkgConfigDeps", "MesonToolchain"

    def requirements(self):
        self.requires("imgui/1.92.5")
        self.requires("implot/0.17")
        self.requires("glfw/3.4")

    def build_requirements(self):
        self.tool_requires("meson/[>=1.10.1 <2]")

    def generate(self):
        imgui = self.dependencies["imgui"]
        src = os.path.join(imgui.package_folder, "res", "bindings")
        dst = os.path.join(self.build_folder, "bindings")
        copy(self, "imgui_impl_glfw*", src, dst)
        copy(self, "imgui_impl_opengl3*", src, dst)

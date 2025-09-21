#!/usr/bin/python
#
# Copyright 2024 - 2025 Khalil Estell and the libhal contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeToolchain, CMakeDeps
from conan.tools.files import copy
from conan.tools.build import check_min_cppstd
import os


required_conan_version = ">=2.2.0"


class libhal_conan(ConanFile):
    name = "libhal"
    license = "Apache-2.0"
    url = "https://github.com/libhal/libhal"
    homepage = "https://libhal.github.io/libhal"
    description = ("A collection of interfaces and abstractions for embedded "
                   "peripherals and devices using modern C++")
    topics = ("peripherals", "hardware", "abstraction", "devices", "hal")
    settings = "compiler", "build_type", "os", "arch"
    exports_sources = "modules/*", "src/*", "tests/*", "CMakeLists.txt", "LICENSE"
    package_type = "static-library"
    shared = False

    @property
    def _min_cppstd(self):
        return "23"

    @property
    def _compilers_minimum_version(self):
        return {
            "gcc": "14",        # GCC 14+ for modules
            "clang": "16",      # Clang 16+ for modules
            "apple-clang": "16.0.0",
            "msvc": "193.4"     # MSVC 14.34+ for modules
        }

    def validate(self):
        if self.settings.get_safe("compiler.cppstd"):
            check_min_cppstd(self, self._min_cppstd)

        # # Validate compiler versions for modules support
        # compiler = self.settings.compiler
        # version = self.settings.compiler.version
        # min_version = self._compilers_minimum_version.get(str(compiler))

        # if min_version and version < min_version:
        #     raise ConanInvalidConfiguration(
        #         f"{compiler} {min_version}+ required for C++20 modules"
        #     )

    def build_requirements(self):
        self.tool_requires("cmake/4.1.1")  # Updated version
        self.tool_requires("ninja/1.13.1")
        self.tool_requires("libhal-cmake-util/[^4.0.5]")
        self.test_requires("boost-ext-ut/2.3.1")

    def requirements(self):
        self.requires("tl-function-ref/1.0.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja"
        tc.cache_variables["CMAKE_CXX_SCAN_FOR_MODULES"] = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE",
             dst=os.path.join(self.package_folder, "licenses"),
             src=self.source_folder)

        copy(self, "*.pcm",
             dst=os.path.join(self.package_folder, "modules"),
             src=os.path.join(self.build_folder, "CMakeFiles", "libhal.dir"))

        # Package any compiled libraries
        copy(self, "*.a",
             dst=os.path.join(self.package_folder, "lib"),
             src=self.build_folder)

        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["libhal"]
        self.cpp_info.bindirs = []
        self.cpp_info.builddirs = ["."]
        self.cpp_info.frameworkdirs = []
        self.cpp_info.resdirs = []
        self.cpp_info.includedirs = []

        modules_path = os.path.join(self.package_folder, "modules")
        self.cpp_info.cxxflags.append(f"-fprebuilt-module-path={modules_path}")

    def package_id(self):
        pass  # Use default package_id that includes compiler info

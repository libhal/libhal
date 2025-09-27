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
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from pathlib import Path


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
    exports_sources = "modules/*", "tests/*", "CMakeLists.txt", "LICENSE"
    python_requires = "conan_module_support/1.0.0"
    package_type = "static-library"
    shared = False

    @property
    def _min_cppstd(self):
        return "23"

    @property
    def _compilers_minimum_version(self):
        return {
            "gcc": ("14", "GCC 14+ required for C++20 modules"),
            "clang": (
                "19",
                "Clang 19+ required for C++20 modules (response file fixes)"
            ),
            "apple-clang": (
                "19.0.0",
                "Apple Clang 19+ required for C++20 modules"
            ),
            "msvc": (
                "193.4",
                "MSVC 14.34+ (Visual Studio 17.4+) required for C++20 modules"
            )
        }

    def _validate_compiler_version(self):
        """Validate compiler version against minimum requirements"""
        compiler = str(self.settings.compiler)
        version = str(self.settings.compiler.version)

        # Map Visual Studio to msvc for consistency
        compiler_key = "msvc" if compiler == "Visual Studio" else compiler

        min_versions = self._compilers_minimum_version
        if compiler_key not in min_versions:
            raise ConanInvalidConfiguration(
                f"Compiler {compiler} is not supported for C++20 modules")

        min_version, error_msg = min_versions[compiler_key]
        if version < min_version:
            raise ConanInvalidConfiguration(error_msg)

    def validate(self):
        if self.settings.get_safe("compiler.cppstd"):
            check_min_cppstd(self, self._min_cppstd)

        self._validate_compiler_version()

    def build_requirements(self):
        self.tool_requires("cmake/4.1.1")
        self.tool_requires("ninja/1.13.1")
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
        cmake = CMake(self)
        cmake.install()

        copy(self, "LICENSE",
             dst=str(Path(self.package_folder) / "licenses"),
             src=self.source_folder)

        MOD_SUPPORT = self.python_requires["conan_module_support"]
        MOD_SUPPORT.module.install_cxx_modules_json(self, "hal")

    def package_info(self):
        self.cpp_info.libs = ["hal"]
        self.cpp_info.bindirs = []
        self.cpp_info.frameworkdirs = []
        self.cpp_info.resdirs = []
        self.cpp_info.includedirs = []

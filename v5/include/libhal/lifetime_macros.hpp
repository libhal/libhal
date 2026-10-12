// Copyright 2026 Khalil Estell and the libhal contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

/**
 * @file lifetime_macros.hpp
 * @brief Attribute macros for libhal's compile-time lifetime checks.
 *
 * Macros are not exported by `import hal;`, so drivers and applications that
 * annotate their own APIs include this header in their global module fragment:
 *
 *   module;
 *   #include <libhal/lifetime_macros.hpp>
 *   export module my_driver;
 *   import hal;
 *
 * On Clang these expand to the attributes that Clang's lifetime safety
 * analysis (`-Wlifetime-safety`) uses. Other compilers get no analysis, so the
 * macros expand to nothing there.
 */

#if defined(__clang__)
/// Ties the lifetime of a returned or stored reference to this parameter (or,
/// after a member function, to `*this`).
#define HAL_LIFETIMEBOUND [[clang::lifetimebound]]
/// Marks a class as a non-owning view of `T`, so lifetimebound parameters of
/// that class type track what it points at, not the view object itself.
#define HAL_POINTER(T) [[gsl::Pointer(T)]]
/// Like HAL_POINTER, for views that are not tied to a single pointee type.
#define HAL_VIEW [[gsl::Pointer]]
#else
#define HAL_LIFETIMEBOUND
#define HAL_POINTER(T)
#define HAL_VIEW
#endif

/// Set to 1 to enable the debug-only runtime lifetime checks in `hal::ref`.
/// This changes `hal::ref`'s ABI, so every part of an image must agree.
#if !defined(HAL_LIFETIME_CHECKS)
#define HAL_LIFETIME_CHECKS 0
#endif

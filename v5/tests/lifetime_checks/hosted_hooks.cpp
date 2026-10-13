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

// Hosted (Linux / macOS) implementation of the hal::lifetime_checks hooks,
// for tests.
//
// On a Cortex-M the equivalent is just linker-script symbols:
//
//   extern "C" char _sdata[], _ebss[], _sstack[], _estack[];
//   bool in_static_memory(void const* p) noexcept {
//     auto a = reinterpret_cast<char const*>(p);
//     return a >= _sdata && a < _ebss;
//   }
//   bool in_stack_memory(void const* p) noexcept {
//     auto a = reinterpret_cast<char const*>(p);
//     return a >= _sstack && a < _estack;
//   }
//   void violation(void const*, void const*) noexcept { hal::halt(); /* or bkpt
//   */ }
//
// (RTOS task stacks live in .bss, so with an RTOS in_stack_memory needs the
// current task's stack bounds instead.)
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <print>

#if defined(__APPLE__)
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#else
extern "C" char __data_start[];  // start of .data (provided by glibc/ld)
extern "C" char _end[];          // end of .bss
#endif

namespace {

using address = std::uintptr_t;

address to_address(void const* p_pointer)
{
  return reinterpret_cast<address>(p_pointer);
}

// Stack bounds without pthreads: the first time a thread asks, record an
// address near the top of its stack. Everything between the current frame
// and that mark (plus some headroom for the caller frames above it) is that
// thread's stack. Works for the main thread and any std::thread.
constexpr std::size_t stack_headroom = std::size_t{ 64 } * 1024;
thread_local address t_stack_top = 0;

[[gnu::noinline]] address current_frame()
{
  return to_address(__builtin_frame_address(0));
}

}  // namespace

// Defined at global-module linkage, matching the extern "C++" declarations
// in the hal:lifetime partition.
namespace hal::inline v5::lifetime_checks {

bool in_static_memory(void const* p_address) noexcept
{
  auto const a = to_address(p_address);
#if defined(__APPLE__)
  // .data and .bss of the main executable both live in the __DATA segment.
  unsigned long size = 0;
  auto const* start = getsegmentdata(&_mh_execute_header, "__DATA", &size);
  return start != nullptr && a >= to_address(start) &&
         a < to_address(start) + size;
#else
  return a >= to_address(__data_start) && a < to_address(_end);
#endif
}

bool in_stack_memory(void const* p_address) noexcept
{
  auto const frame = current_frame();
  if (t_stack_top == 0) {
    t_stack_top = frame + stack_headroom;
  }
  auto const a = to_address(p_address);
  // Stacks grow down on every supported host.
  return a >= frame && a < t_stack_top;
}

void violation(void const* p_holder, void const* p_target) noexcept
{
  std::println(stderr,
               "hal lifetime violation: ref in static memory ({}) points to "
               "stack memory ({})",
               p_holder,
               p_target);
  std::_Exit(3);  // distinct exit code so the test runner can detect it
}

}  // namespace hal::inline v5::lifetime_checks

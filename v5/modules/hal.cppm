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

export module hal;

// Core 3rd party libraries
export import strong_ptr;
export import async_context;
export import scatter_span;

// Types, definitions, and containers
export import :units;
export import :error;
export import :containers;
export import :aliases;

// Interfaces
export import :analog;
export import :can;
export import :pwm;
export import :gpio;
export import :sensors;
export import :interrupts;
export import :steady_clock;
export import :i2c;
export import :spi;
export import :serial;
export import :servo;
export import :motor;
export import :memory;
export import :containers;
export import :usb;

export namespace hal::inline v5 {
inline constexpr auto version = "5.0.0";
}

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

module;

#include <memory_resource>

export module hal:aliases;

import strong_ptr;
import async_context;

namespace hal::inline v5 {
/// Shorthand for a non-nullable, reference counted pointer to T.
export template<typename T>
using ptr = mem::strong_ptr<T>;

/// Shorthand for the result of an asynchronous operation.
export template<typename T>
using future = async::future<T>;

/// Shorthand for an asynchronous operation with no result.
export using task = async::task;

/// Shorthand for an asynchronous operation that produces a ptr<T>. Common for
/// factory functions that must probe a device before returning a handle to
/// it (e.g. i2c device discovery, CAN node discovery).
export template<typename T>
using future_ptr = async::future<mem::strong_ptr<T>>;

/// Shorthand for the allocator type accepted by ptr<T> factory functions.
export using allocator = std::pmr::polymorphic_allocator<>;
}  // namespace hal::inline v5

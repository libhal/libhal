// Copyright 2024 - 2025 Khalil Estell and the libhal contributors
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

export module hal:callback;

import :inplace_function;

/**
 * @defgroup Functional List of functional and function capturing APIs
 *
 */
export namespace hal::inline v5 {
constexpr auto max_storage = sizeof(void*) * 2;
/**
 * @ingroup Functional
 * @brief Definition of a callable object that stores its callable in a local
 * buffer
 *
 * Use this for storing a callable object when you expect the object to be held
 * for a duration of time AND the lambda is non-empty or the callable is a
 * function object. If you just plan to hold onto a function pointer, then just
 * use a function pointer.
 *
 * The benefit of inplace_function over std::function is that the inplace
 * version uses a local buffer to store the callable object rather than
 * allocating space for it on the heap. This makes the callable deterministic
 * and removes the potential for an allocation failure. The inplace_function
 * will fail to compile if the buffer size is not large enough to hold the
 * callable object, making it a compile time error rather than a runtime error.
 *
 * @tparam signature - function signature
 * @tparam capacity - the amount of bytes to reserve for the callable object.
 * Defaults to 32 bytes which is suitable for most use cases.
 */
template<class signature, auto capacity = 32>
using callback = hal::sg14::inplace_function<signature, capacity>;
}  // namespace hal::inline v5

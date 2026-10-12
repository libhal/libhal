# Copyright 2026 Khalil Estell and the libhal contributors
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

# Compile-time checks: misuse that must not compile, and dangling references
# that Clang's lifetime safety analysis must report. Each case is its own
# target (never part of `all`) that a CTest test builds, passing only when the
# expected diagnostic appears. WARN cases build with -Werror, so they fail and
# re-check on every run instead of being cached as up to date.
#
# Clang 23+ only (needed for -Wlifetime-safety), and only for native builds.

if(CMAKE_CROSSCOMPILING OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS "23")
    message(STATUS "Skipping lifetime compile checks (needs native Clang 23+)")
    return()
endif()

# libhal_compile_check(<file> <case> <expect>)
#   <expect> is a regex the build output must match, or SUCCESS for cases
#   that must build cleanly (known gaps).
function(libhal_compile_check SOURCE CASE EXPECT)
    get_filename_component(GROUP "${SOURCE}" NAME_WE)
    set(TARGET "compile_check_${GROUP}_${CASE}")
    add_executable(${TARGET} EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/${SOURCE}")
    target_compile_features(${TARGET} PRIVATE cxx_std_23)
    target_compile_definitions(${TARGET} PRIVATE CASE_${CASE})
    # Same flags as libhal's tests (e.g. -fno-rtti), so the module interfaces
    # rebuilt for this target match the ones it imports.
    target_compile_options(${TARGET} PRIVATE
        ${LIBHAL_CXX_FLAGS} -Werror -Wlifetime-safety)
    target_link_libraries(${TARGET} PRIVATE libhal)

    add_test(NAME ${TARGET}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                --target ${TARGET} --config $<CONFIG>)
    # Builds in one tree must not run concurrently.
    set_tests_properties(${TARGET} PROPERTIES
        RESOURCE_LOCK libhal_compile_checks)
    if(EXPECT STREQUAL "SUCCESS")
        set_tests_properties(${TARGET} PROPERTIES
            FAIL_REGULAR_EXPRESSION "warning:|error:")
    else()
        set_tests_properties(${TARGET} PROPERTIES
            PASS_REGULAR_EXPRESSION "${EXPECT}")
    endif()
endfunction()

set(CONSTEVAL_REJECTS
    "call to consteval function .*static_ref.* is not a constant expression")
set(RETURNS_STACK "Wlifetime-safety-return-stack-addr")
set(DANGLES_GLOBAL "escapes to the static variable .* which will dangle")

# ---- hal:lifetime ----------------------------------------------------------
foreach(CASE ERR_STATIC_REF_LOCAL ERR_STATIC_REF_HEAP
             ERR_STATIC_REF_RUNTIME_PARAM ERR_STATIC_REF_THREAD_LOCAL
             ERR_STATIC_REF_FROM_LEASE)
    libhal_compile_check(lifetime_cases.cpp ${CASE} "${CONSTEVAL_REJECTS}")
endforeach()
foreach(CASE ERR_REF_TEMPORARY ERR_REF_UNIQUE_PTR ERR_REF_RETURN_LOCAL
             ERR_REF_FROM_TEMP_LEASE)
    libhal_compile_check(lifetime_cases.cpp ${CASE}
        "invokes a deleted function")
endforeach()
libhal_compile_check(lifetime_cases.cpp ERR_REF_RAW_PTR
    "call to deleted constructor")
libhal_compile_check(lifetime_cases.cpp ERR_DEREF_TEMP_LEASE
    "deleted operator '\\*'")
libhal_compile_check(lifetime_cases.cpp ERR_ARROW_TEMP_LEASE
    "deleted operator '->'")
libhal_compile_check(lifetime_cases.cpp ERR_LEASE_COPY
    "implicitly-deleted copy constructor")
libhal_compile_check(lifetime_cases.cpp ERR_LEASE_MOVE_ASSIGN
    "deleted operator '='")
libhal_compile_check(lifetime_cases.cpp ERR_LEASE_RESET_REMOVED
    "no member named 'reset'")
foreach(CASE ERR_BARE_NEW ERR_MAKE_UNIQUE)
    libhal_compile_check(lifetime_cases.cpp ${CASE}
        "deleted function 'operator new'")
endforeach()
foreach(CASE WARN_RETURN_SENSOR_LOCAL WARN_RETURN_SENSOR_LOCAL_LEASE
             WARN_INTERMEDIATE_REF)
    libhal_compile_check(lifetime_cases.cpp ${CASE} "${RETURNS_STACK}")
endforeach()
libhal_compile_check(lifetime_cases.cpp WARN_ESCAPE_TO_STATIC
    "${DANGLES_GLOBAL}")
libhal_compile_check(lifetime_cases.cpp GAP_HEAP_DEREF SUCCESS)

# ---- hal:subscription ------------------------------------------------------
libhal_compile_check(subscription_cases.cpp
    ERR_STATIC_REF_TO_LOCAL_CALLBACK "${CONSTEVAL_REJECTS}")
libhal_compile_check(subscription_cases.cpp
    WARN_DISCARDED_SUBSCRIPTION "Wunused-result")
foreach(CASE WARN_SUBSCRIPTION_OUTLIVES_CALLBACK
             WARN_SUBSCRIPTION_OUTLIVES_PIN
             WARN_RETURN_COUNTER_PAST_LOCAL_PIN)
    libhal_compile_check(subscription_cases.cpp ${CASE} "${RETURNS_STACK}")
endforeach()
libhal_compile_check(subscription_cases.cpp
    WARN_STATIC_COUNTER_ON_LOCAL_PIN "${DANGLES_GLOBAL}")

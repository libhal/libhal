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

# Runtime lifetime checks (HAL_LIFETIME_CHECKS=1): the debug-only backstop
# that catches a ref in static storage bound to a stack object. The setting
# changes hal::ref's ABI, so these cases link against a second build of the
# hal module with checks enabled, made from libhal's own module list.
#
# Whether to ship this option is still undecided; it stays off by default.
# Native builds only (the hosted hooks inspect this process's memory).

if(CMAKE_CROSSCOMPILING)
    return()
endif()

get_target_property(LIBHAL_MODULE_FILES libhal CXX_MODULE_SET)
get_target_property(LIBHAL_LINKED libhal LINK_LIBRARIES)

add_library(libhal_lifetime_checked STATIC)
target_sources(libhal_lifetime_checked PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
    FILES ${LIBHAL_MODULE_FILES}
)
target_compile_features(libhal_lifetime_checked PUBLIC cxx_std_23)
target_compile_definitions(libhal_lifetime_checked PUBLIC HAL_LIFETIME_CHECKS=1)
target_link_libraries(libhal_lifetime_checked PRIVATE ${LIBHAL_LINKED})
target_compile_options(libhal_lifetime_checked PRIVATE ${LIBHAL_CXX_FLAGS})

# libhal_lifetime_check(<case> <expect>)
#   <expect> is CAUGHT (must report a violation) or CLEAN (must run cleanly;
#   also used for known misses).
function(libhal_lifetime_check CASE EXPECT)
    set(TARGET "lifetime_check_${CASE}")
    add_executable(${TARGET}
        ${CMAKE_CURRENT_LIST_DIR}/check_cases.cpp
        ${CMAKE_CURRENT_LIST_DIR}/hosted_hooks.cpp)
    target_compile_features(${TARGET} PRIVATE cxx_std_23)
    target_compile_definitions(${TARGET} PRIVATE CASE_${CASE})
    # The CAUGHT cases contain the bug on purpose, so don't fail the build on
    # warnings about it.
    target_compile_options(${TARGET} PRIVATE ${LIBHAL_CXX_FLAGS} -Wno-error)
    target_link_libraries(${TARGET} PRIVATE libhal_lifetime_checked)
    add_test(NAME ${TARGET} COMMAND ${TARGET})
    if(EXPECT STREQUAL "CAUGHT")
        set_tests_properties(${TARGET} PROPERTIES
            PASS_REGULAR_EXPRESSION "hal lifetime violation")
    endif()
endfunction()

foreach(CASE CATCH_STUDENT_FORGOT_STATIC CATCH_STATIC_REF_ESCAPE
             CATCH_GLOBAL_SENSOR_ASSIGNED CATCH_ARENA_COMPOSITE_TO_LOCAL
             CATCH_FORGOT_STATIC_IN_STD_THREAD)
    libhal_lifetime_check(${CASE} CAUGHT)
endforeach()
foreach(CASE CLEAN_LOCAL_TO_LOCAL CLEAN_LOCAL_TO_LOCAL_IN_STD_THREAD
             CLEAN_STATIC_TO_STATIC CLEAN_LOCAL_TO_STATIC CLEAN_APP_WITH_LEASE
             CLEAN_STATIC_APP_WITH_STATIC_BUS
             # Known miss: the leased driver lives in slot storage (.bss), so
             # the target looks static.
             MISS_STATIC_SENSOR_ON_LOCAL_LEASE)
    libhal_lifetime_check(${CASE} CLEAN)
endforeach()

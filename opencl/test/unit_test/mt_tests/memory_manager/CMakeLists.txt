#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_memory_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_clear_queue_mt_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_mt_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_reuse_cleaner_tests_mt.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_memory_manager})

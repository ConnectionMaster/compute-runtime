#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_api
    # local files
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_api_tests_mt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_create_user_event_tests_mt.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_get_device_info_tests_mt.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_get_platform_ids_tests_mt.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_get_platform_info_tests_mt.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_intel_tracing_tests_mt.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_set_mem_object_destructor_callback_tests_mt.inl

    # necessary dependencies from igdrcl_tests
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/api/cl_api_tests.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_api})

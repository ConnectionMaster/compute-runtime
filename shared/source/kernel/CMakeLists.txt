#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_KERNEL
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_data.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_kernel_encoder_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/grf_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_args.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}implicit_args.h
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_args_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_args_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_descriptor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_metadata.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}kernel_descriptor_ext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor_from_patchtokens.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor_from_patchtokens.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}kernel_descriptor_from_patchtokens_extra.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_execution_type.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/local_ids_cache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/local_ids_cache.h
)

set_property(GLOBAL PROPERTY NEO_CORE_KERNEL ${NEO_CORE_KERNEL})
add_subdirectories()

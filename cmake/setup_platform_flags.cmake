#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NEO_SKIP_BASE_PLATFORMS)
  return()
endif()

SET_FLAGS_FOR_CURRENT("GEN12LP" "TGLLP" "RKL" "ADLS" "ADLP" "DG1" "ADLN")
SET_FLAGS_FOR_CURRENT("XE_HPG_CORE" "DG2" "MTL" "ARL")
SET_FLAGS_FOR_CURRENT("XE2_HPG_CORE" "BMG" "LNL")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET_FLAGS_FOR_CURRENT("XE3_CORE" "PTL")
  SET_FLAGS_FOR_CURRENT("XE_HPC_CORE" "PVC")
else()
  DISABLE_32BIT_FLAGS_FOR("XE3_CORE" "PTL")
  DISABLE_32BIT_FLAGS_FOR("XE_HPC_CORE" "PVC")
endif()

DISABLE_WDDM_LINUX_FOR("XE_HPC_CORE" "PVC")

foreach(CORE_TYPE ${XE_HPC_AND_BEFORE_CORE_TYPES})
  if(TESTS_${CORE_TYPE})
    set(TESTS_XE_HPC_AND_BEFORE 1)
  endif()
  if(SUPPORT_${CORE_TYPE})
    set(SUPPORT_XE_HPC_AND_BEFORE 1)
  endif()
endforeach()

foreach(CORE_TYPE ${XEHP_AND_LATER_CORE_TYPES})
  if(TESTS_${CORE_TYPE})
    set(TESTS_XEHP_AND_LATER 1)
  endif()
  if(SUPPORT_${CORE_TYPE})
    set(SUPPORT_XEHP_AND_LATER 1)
  endif()
endforeach()

foreach(CORE_TYPE ${DG2_AND_LATER_CORE_TYPES})
  if(TESTS_${CORE_TYPE})
    set(TESTS_DG2_AND_LATER 1)
  endif()
  if(SUPPORT_${CORE_TYPE})
    set(SUPPORT_DG2_AND_LATER 1)
  endif()
endforeach()

foreach(CORE_TYPE ${MTL_AND_LATER_CORE_TYPES})
  if(TESTS_${CORE_TYPE})
    set(TESTS_MTL_AND_LATER 1)
  endif()
  if(SUPPORT_${CORE_TYPE})
    set(SUPPORT_MTL_AND_LATER 1)
  endif()
endforeach()

foreach(CORE_TYPE ${PVC_AND_LATER_CORE_TYPES})
  if(TESTS_${CORE_TYPE})
    set(TESTS_PVC_AND_LATER 1)
  endif()
  if(SUPPORT_${CORE_TYPE})
    set(SUPPORT_PVC_AND_LATER 1)
  endif()
endforeach()

foreach(CORE_TYPE ${XE2_AND_LATER_CORE_TYPES})
  if(TESTS_${CORE_TYPE})
    set(TESTS_XE2_AND_LATER 1)
  endif()
  if(SUPPORT_${CORE_TYPE})
    set(SUPPORT_XE2_AND_LATER 1)
  endif()
endforeach()

if(SUPPORT_ARL)
  ENABLE_ADDITIONAL_SKU("MTL")
  if(TESTS_ARL)
    TEST_ADDITIONAL_SKU("MTL")
  endif()
endif()

# Add supported and tested platforms
if(SUPPORT_GEN12LP)
  ADD_AOT_DEFINITION(XE_LP)
  ADD_AOT_DEFINITION(XE)
  if(TESTS_GEN12LP)
    ADD_ITEM_FOR_CORE_TYPE("FAMILY_NAME" "TESTED" "GEN12LP" "Gen12LpFamily")
  endif()
  if(SUPPORT_TGLLP)
    ADD_AOT_DEFINITION(TGLLP)
    set(TGLLP_GEN12LP_REVISIONS 0)
    set(TGLLP_GEN12LP_RELEASES "12.0.0")
    ADD_PRODUCT("SUPPORTED" "TGLLP" "IGFX_TIGERLAKE_LP")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "GEN12LP" "TGLLP")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "GEN12LP" "TGLLP")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "GEN12LP" "TGLLP")
    if(TESTS_TGLLP)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "GEN12LP" "TGLLP")
      ADD_PRODUCT("TESTED" "TGLLP" "IGFX_TIGERLAKE_LP")
    endif()
  endif()

  if(SUPPORT_DG1)
    ADD_AOT_DEFINITION(DG1)
    set(DG1_GEN12LP_REVISIONS 0)
    set(DG1_GEN12LP_RELEASES "12.10.0")
    ADD_PRODUCT("SUPPORTED" "DG1" "IGFX_DG1")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "GEN12LP" "DG1")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "GEN12LP" "DG1")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "GEN12LP" "DG1")
    if(TESTS_DG1)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "GEN12LP" "DG1")
      ADD_PRODUCT("TESTED" "DG1" "IGFX_DG1")
    endif()
  endif()

  if(SUPPORT_RKL)
    ADD_AOT_DEFINITION(RKLC)
    set(RKL_GEN12LP_REVISIONS 0)
    set(RKL_GEN12LP_RELEASES "12.1.0")
    ADD_PRODUCT("SUPPORTED" "RKL" "IGFX_ROCKETLAKE")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "GEN12LP" "RKL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "GEN12LP" "RKL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "GEN12LP" "RKL")
    if(TESTS_RKL)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "GEN12LP" "RKL")
      ADD_PRODUCT("TESTED" "RKL" "IGFX_ROCKETLAKE")
    endif()
  endif()

  if(SUPPORT_ADLS)
    ADD_AOT_DEFINITION(ADLS)
    set(ADLS_GEN12LP_REVISIONS 0)
    set(ADLS_GEN12LP_RELEASES "12.2.0")
    ADD_PRODUCT("SUPPORTED" "ADLS" "IGFX_ALDERLAKE_S")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "GEN12LP" "ADLS")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "GEN12LP" "ADLS")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "GEN12LP" "ADLS")
    if(TESTS_ADLS)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "GEN12LP" "ADLS")
      ADD_PRODUCT("TESTED" "ADLS" "IGFX_ALDERLAKE_S")
    endif()
  endif()

  if(SUPPORT_ADLP)
    ADD_AOT_DEFINITION(ADL)
    set(ADLP_GEN12LP_REVISIONS 0)
    set(ADLP_GEN12LP_RELEASES "12.3.0")
    ADD_PRODUCT("SUPPORTED" "ADLP" "IGFX_ALDERLAKE_P")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "GEN12LP" "ADLP")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "GEN12LP" "ADLP")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "GEN12LP" "ADLP")
    if(TESTS_ADLP)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "GEN12LP" "ADLP")
      ADD_PRODUCT("TESTED" "ADLP" "IGFX_ALDERLAKE_P")
    endif()
  endif()

  if(SUPPORT_ADLN)
    ADD_AOT_DEFINITION(ADL)
    set(ADLN_GEN12LP_REVISIONS 0)
    set(ADLN_GEN12LP_RELEASES "12.4.0")
    ADD_PRODUCT("SUPPORTED" "ADLN" "IGFX_ALDERLAKE_N")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "GEN12LP" "ADLN")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "GEN12LP" "ADLN")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "GEN12LP" "ADLN")
    if(TESTS_ADLN)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "GEN12LP" "ADLN")
      ADD_PRODUCT("TESTED" "ADLN" "IGFX_ALDERLAKE_N")
    endif()
  endif()
endif()

if(SUPPORT_XE_HPG_CORE)
  ADD_AOT_DEFINITION(XE)
  if(TESTS_XE_HPG_CORE)
    ADD_ITEM_FOR_CORE_TYPE("FAMILY_NAME" "TESTED" "XE_HPG_CORE" "XeHpgCoreFamily")
  endif()
  if(SUPPORT_MTL)
    ADD_AOT_DEFINITION(XE_LPG)
    ADD_AOT_DEFINITION(MTL)
    set(MTL_XE_HPG_CORE_REVISIONS 0x7D40/0)
    set(MTL_XE_HPG_CORE_RELEASES "12.70.0" "12.70.4" "12.71.0" "12.71.4")
    ADD_PRODUCT("SUPPORTED" "MTL" "IGFX_METEORLAKE")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "XE_HPG_CORE" "MTL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "XE_HPG_CORE" "MTL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "XE_HPG_CORE" "MTL")
    if(TESTS_MTL)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "XE_HPG_CORE" "MTL")
      ADD_PRODUCT("TESTED" "MTL" "IGFX_METEORLAKE")
    endif()
  endif()
  if(SUPPORT_DG2)
    ADD_AOT_DEFINITION(XE_HPG)
    ADD_AOT_DEFINITION(DG2)
    set(DG2_XE_HPG_CORE_REVISIONS 0x4F80/0)
    set(DG2_XE_HPG_CORE_RELEASES "12.55.0" "12.55.1" "12.55.4" "12.55.8" "12.56.0" "12.56.4" "12.56.5" "12.57.0")
    ADD_PRODUCT("SUPPORTED" "DG2" "IGFX_DG2")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "XE_HPG_CORE" "DG2")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "XE_HPG_CORE" "DG2")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "XE_HPG_CORE" "DG2")
    if(TESTS_DG2)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "XE_HPG_CORE" "DG2")
      ADD_PRODUCT("TESTED" "DG2" "IGFX_DG2")
    endif()
  endif()
  if(SUPPORT_ARL)
    ADD_AOT_DEFINITION(XE_LPGPLUS)
    ADD_AOT_DEFINITION(ARL)
    set(ARL_XE_HPG_CORE_REVISIONS 0x7D41/0)
    set(ARL_XE_HPG_CORE_RELEASES "12.74.0" "12.74.4")
    ADD_PRODUCT("SUPPORTED" "ARL" "IGFX_ARROWLAKE")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "XE_HPG_CORE" "ARL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "XE_HPG_CORE" "ARL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "XE_HPG_CORE" "ARL")
    if(TESTS_ARL)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "XE_HPG_CORE" "ARL")
      ADD_PRODUCT("TESTED" "ARL" "IGFX_ARROWLAKE")
    endif()
  endif()
endif()

if(SUPPORT_XE_HPC_CORE)
  ADD_AOT_DEFINITION(XE)
  ADD_AOT_DEFINITION(XE_HPC)
  ADD_AOT_DEFINITION(XE_HPC_VG)
  set(XE_HPC_CORE_TEST_KERNELS_BLOCKLIST "CopyBuffer_simd8.cl")
  if(TESTS_XE_HPC_CORE)
    ADD_ITEM_FOR_CORE_TYPE("FAMILY_NAME" "TESTED" "XE_HPC_CORE" "XeHpcCoreFamily")
  endif()

  if(SUPPORT_PVC)
    ADD_AOT_DEFINITION(PVC)
    set(PVC_XE_HPC_CORE_REVISIONS 3 47)
    set(PVC_XE_HPC_CORE_RELEASES "12.60.0" "12.60.1" "12.60.3" "12.60.5" "12.60.6" "12.60.7" "12.61.7")
    ADD_PRODUCT("SUPPORTED" "PVC" "IGFX_PVC")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "XE_HPC_CORE" "PVC")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_AUX_TRANSLATION" "XE_HPC_CORE" "PVC")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_STATELESS" "XE_HPC_CORE" "PVC")
    set(PREFERRED_PLATFORM "PVC")
    if(TESTS_PVC)
      set(PREFERRED_FAMILY_NAME "XeHpcCoreFamily")
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "XE_HPC_CORE" "PVC")
      ADD_PRODUCT("TESTED" "PVC" "IGFX_PVC")
    endif()
  endif()
endif()

if(SUPPORT_XE2_HPG_CORE)
  ADD_AOT_DEFINITION(XE2)
  set(XE2_HPG_CORE_TEST_KERNELS_BLOCKLIST "CopyBuffer_simd8.cl")
  if(TESTS_XE2_HPG_CORE)
    ADD_ITEM_FOR_CORE_TYPE("FAMILY_NAME" "TESTED" "XE2_HPG_CORE" "Xe2HpgCoreFamily")
  endif()

  if(SUPPORT_BMG)
    ADD_AOT_DEFINITION(XE2_HPG)
    ADD_AOT_DEFINITION(BMG)
    set(BMG_XE2_HPG_CORE_REVISIONS 0)
    set(BMG_XE2_HPG_CORE_RELEASES "20.1.0")
    ADD_PRODUCT("SUPPORTED" "BMG" "IGFX_BMG")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "XE2_HPG_CORE" "BMG")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "XE2_HPG_CORE" "BMG")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_STATELESS" "XE2_HPG_CORE" "BMG")
    if(TESTS_BMG)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "XE2_HPG_CORE" "BMG")
      ADD_PRODUCT("TESTED" "BMG" "IGFX_BMG")
    endif()
  endif()

  if(SUPPORT_LNL)
    ADD_AOT_DEFINITION(XE2_LPG)
    ADD_AOT_DEFINITION(LNL)
    set(LNL_XE2_HPG_CORE_REVISIONS 4)
    set(LNL_XE2_HPG_CORE_RELEASES "20.4.0" "20.4.1" "20.4.4")
    ADD_PRODUCT("SUPPORTED" "LNL" "IGFX_LUNARLAKE")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "XE2_HPG_CORE" "LNL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "XE2_HPG_CORE" "LNL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_STATELESS" "XE2_HPG_CORE" "LNL")
    if(TESTS_LNL)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "XE2_HPG_CORE" "LNL")
      ADD_PRODUCT("TESTED" "LNL" "IGFX_LUNARLAKE")
    endif()
  endif()
endif()

if(SUPPORT_XE3_CORE)
  ADD_AOT_DEFINITION(XE3)
  set(XE3_CORE_TEST_KERNELS_BLOCKLIST "CopyBuffer_simd8.cl")
  if(TESTS_XE3_CORE)
    ADD_ITEM_FOR_CORE_TYPE("FAMILY_NAME" "TESTED" "XE3_CORE" "Xe3CoreFamily")
  endif()

  if(SUPPORT_PTL)
    ADD_AOT_DEFINITION(XE3_LPG)
    ADD_AOT_DEFINITION(PTL)
    set(PTL_XE3_CORE_REVISIONS 4)
    set(PTL_XE3_CORE_RELEASES "30.0.0" "30.0.4" "30.1.0" "30.1.1")
    ADD_PRODUCT("SUPPORTED" "PTL" "IGFX_PTL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED" "XE3_CORE" "PTL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_IMAGES" "XE3_CORE" "PTL")
    ADD_PLATFORM_FOR_CORE_TYPE("SUPPORTED_STATELESS" "XE3_CORE" "PTL")
    if(TESTS_PTL)
      ADD_ITEM_FOR_CORE_TYPE("PLATFORMS" "TESTED" "XE3_CORE" "PTL")
      ADD_PRODUCT("TESTED" "PTL" "IGFX_PTL")
    endif()
  endif()
endif()

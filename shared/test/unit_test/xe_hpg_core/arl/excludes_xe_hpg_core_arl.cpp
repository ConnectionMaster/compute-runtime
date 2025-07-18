/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded_ForceNonCoherentSupportedMatcher, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenCoherencyWithSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded_ForceNonCoherentSupportedMatcher, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeChangeIsRequiredThenCorrectCommandsAreAdded_ForceNonCoherentSupportedMatcher, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed_ForceNonCoherentSupportedMatcher, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsGreaterThan128ThenLargeGRFModeIsProgrammed_ForceNonCoherentSupportedMatcher, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirementsXeHpgCore, givenComputeModeCmdSizeWhenLargeGrfModeChangeIsRequiredThenSCMCommandSizeIsCalculated, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirementsXeHpgCore, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirementsXeHpgCore, givenCoherencyWithSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(GfxCoreHelperTest, givenDebugVariableSetWhenAskingForAuxTranslationModeThenReturnCorrectValue, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnFalse, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTests, givenRequiredWorkGroupOrderWhenCallAdjustWalkOrderThenWalkerIsNotChanged_IsAtMostXeCore, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenIsAdjustWalkOrderAvailableCallThenFalseReturn, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenCheckBlitEnqueuePreferredThenReturnTrue, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned, IGFX_ARROWLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenBooleanUncachedWhenCallOverridePatIndexThenProperPatIndexIsReturned, IGFX_ARROWLAKE);

/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/engine/sysman_engine_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t EngineImp::engineGetActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) {
    return pOsEngine->getActivityExt(pCount, pStats);
}

ze_result_t EngineImp::engineGetActivity(zes_engine_stats_t *pStats) {
    return pOsEngine->getActivity(pStats);
}

ze_result_t EngineImp::engineGetProperties(zes_engine_properties_t *pProperties) {
    *pProperties = engineProperties;
    return ZE_RESULT_SUCCESS;
}

void EngineImp::init() {
    if (pOsEngine->isEngineModuleSupported()) {
        pOsEngine->getProperties(engineProperties);
        this->initSuccess = true;
    }
}

EngineImp::EngineImp(OsSysman *pOsSysman, MapOfEngineInfo &mapEngineInfo, zes_engine_group_t engineType, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubdevice) {
    pOsEngine = OsEngine::create(pOsSysman, mapEngineInfo, engineType, engineInstance, tileId, onSubdevice);
    init();
}

EngineImp::~EngineImp() = default;

} // namespace Sysman
} // namespace L0

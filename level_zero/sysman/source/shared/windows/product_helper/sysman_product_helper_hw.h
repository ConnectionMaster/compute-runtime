/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"

namespace L0 {
namespace Sysman {

template <PRODUCT_FAMILY gfxProduct>
class SysmanProductHelperHw : public SysmanProductHelper {
  public:
    static std::unique_ptr<SysmanProductHelper> create() {
        auto pSysmanProductHelper = std::unique_ptr<SysmanProductHelper>(new SysmanProductHelperHw());
        return pSysmanProductHelper;
    }

    ~SysmanProductHelperHw() override = default;

    // Temperature
    ze_result_t getSensorTemperature(double *pTemperature, zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) override;
    bool isTempModuleSupported(zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) override;

    // Pci
    ze_result_t getPciStats(zes_pci_stats_t *pStats, WddmSysmanImp *pWddmSysmanImp) override;
    ze_result_t getPciProperties(zes_pci_properties_t *properties) override;

    // Memory
    ze_result_t getMemoryBandWidth(zes_mem_bandwidth_t *pBandwidth, WddmSysmanImp *pWddmSysmanImp) override;

    // Power
    std::vector<zes_power_domain_t> getNumberOfPowerDomainsSupported(WddmSysmanImp *pWddmSysmanImp) override;
    ze_result_t getPowerProperties(zes_power_properties_t *pProperties, zes_power_domain_t powerDomain, WddmSysmanImp *pWddmSysmanImp) override;
    ze_result_t getPowerPropertiesExt(zes_power_ext_properties_t *pExtPoperties, zes_power_domain_t powerDomain, WddmSysmanImp *pWddmSysmanImp) override;
    ze_result_t getPowerEnergyCounter(zes_power_energy_counter_t *pEnergy, zes_power_domain_t powerDomain, WddmSysmanImp *pWddmSysmanImp) override;

    // Pmt
    std::map<unsigned long, std::map<std::string, uint32_t>> *getGuidToKeyOffsetMap() override;

  protected:
    SysmanProductHelperHw() = default;
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableSysmanProductHelper {
    EnableSysmanProductHelper() {
        auto sysmanProductHelperCreateFunction = SysmanProductHelperHw<gfxProduct>::create;
        sysmanProductHelperFactory[gfxProduct] = sysmanProductHelperCreateFunction;
    }
};

} // namespace Sysman
} // namespace L0

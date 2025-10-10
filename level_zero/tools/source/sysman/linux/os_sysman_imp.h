/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/tools/source/sysman/os_sysman.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include <map>
#include <mutex>

namespace NEO {
class Drm;
} // namespace NEO

namespace L0 {

class PmuInterface;
class FirmwareUtil;
class FsAccess;
class PlatformMonitoringTech;
class ProcfsAccess;
class SysfsAccess;
struct Device;
struct SysmanDeviceImp;

class ExecutionEnvironmentRefCountRestore : public NEO::NonCopyableAndNonMovableClass {
  public:
    ExecutionEnvironmentRefCountRestore() = delete;
    ExecutionEnvironmentRefCountRestore(NEO::ExecutionEnvironment *executionEnvironmentRecevied) {
        executionEnvironment = executionEnvironmentRecevied;
        executionEnvironment->incRefInternal();
    }

    ~ExecutionEnvironmentRefCountRestore() {
        executionEnvironment->decRefInternal();
    }
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
};

class LinuxSysmanImp : public OsSysman, NEO::NonCopyableAndNonMovableClass {
  public:
    LinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp);
    ~LinuxSysmanImp() override;

    ze_result_t init() override;

    PmuInterface *getPmuInterface();
    FirmwareUtil *getFwUtilInterface();
    FsAccess &getFsAccess();
    ProcfsAccess &getProcfsAccess();
    SysfsAccess &getSysfsAccess();
    NEO::Drm &getDrm();
    PlatformMonitoringTech *getPlatformMonitoringTechAccess(uint32_t subDeviceId);
    Device *getDeviceHandle();
    std::vector<ze_device_handle_t> &getDeviceHandles() override;
    ze_device_handle_t getCoreDeviceHandle() override;
    ze_bool_t isDriverModelSupported() override;
    SysmanDeviceImp *getSysmanDeviceImp();
    std::string getPciCardBusDirectoryPath(std::string realPciPath);
    uint32_t getMemoryType();
    static std::string getPciRootPortDirectoryPath(std::string realPciPath);
    void releasePmtObject();
    ze_result_t createPmtHandles();
    void createFwUtilInterface();
    void releaseFwUtilInterface();
    void releaseLocalDrmHandle();
    void releaseSysmanDeviceResources();
    MOCKABLE_VIRTUAL void releaseDeviceResources();
    MOCKABLE_VIRTUAL ze_result_t initDevice();
    void reInitSysmanDeviceResources();
    MOCKABLE_VIRTUAL void getPidFdsForOpenDevice(ProcfsAccess *, SysfsAccess *, const ::pid_t, std::vector<int> &);
    MOCKABLE_VIRTUAL ze_result_t osWarmReset();
    MOCKABLE_VIRTUAL ze_result_t osColdReset();
    MOCKABLE_VIRTUAL ze_result_t gpuProcessCleanup(ze_bool_t force);
    std::string getAddressFromPath(std::string &rootPortPath);
    decltype(&NEO::SysCalls::pread) preadFunction = NEO::SysCalls::pread;
    decltype(&NEO::SysCalls::pwrite) pwriteFunction = NEO::SysCalls::pwrite;
    std::string devicePciBdf = "";
    uint32_t rootDeviceIndex = 0u;
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    bool diagnosticsReset = false;
    bool isMemoryDiagnostics = false;
    Device *pDevice = nullptr;
    std::string gtDevicePath;
    bool isUsingPrelimEnabledKmd = false;

  protected:
    FsAccess *pFsAccess = nullptr;
    ProcfsAccess *pProcfsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    NEO::Drm *pDrm = nullptr;
    PmuInterface *pPmuInterface = nullptr;
    FirmwareUtil *pFwUtilInterface = nullptr;
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    ze_result_t initLocalDeviceAndDrmHandles();
    uint32_t memType = unknownMemoryType;

  private:
    bool isMemTypeRetrieved = false;
    LinuxSysmanImp() = delete;
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
    static const std::string deviceDir;
    void clearHPIE(int fd);
    ze_result_t resizeVfBar(uint8_t size);
    std::mutex fwLock;
};

} // namespace L0

#include "PortDeviceManager.h"
#include "Logger.h"
using namespace DeviceManager;
extern Logger logger;


int PortDeviceManager::removePortCallback(enumContext* context)
{
    enumRemoveContext* ctx = (enumRemoveContext*)context;
    std::string portName = str_toupper(getPortName(ctx));
    if (portName == ctx->targetName) {
        // found the port to remove
        SP_REMOVEDEVICE_PARAMS rmdParams{ 0 };
        rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
        rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
        rmdParams.HwProfile = 0;
        if (!SetupDiSetClassInstallParams(ctx->hDevInfo, &ctx->devInfoData, &rmdParams.ClassInstallHeader, sizeof(rmdParams))) {
            logger << "Failed to set class install DIF_REMOVE params. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }
        if (!SetupDiCallClassInstaller(DIF_REMOVE, ctx->hDevInfo, &ctx->devInfoData)) {
            logger << "Failed to call class installer for DIF_REMOVE. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }
        ctx->found = true;
        // terminate the enumeration
        return 1;
    }
    return 0;
}

int PortDeviceManager::listPortCallback(enumContext* context)
{
    enumListContext* ctx = (enumListContext*)context;
    if (logger.getLogLevel() == Logger::VERBOSE_LVL) {
        RegKeyHandle regKey(openDeviceHardwareKey(ctx->hDevInfo, &ctx->devInfoData));
        if (!regKey.isValid()) {
            // this should not happen
            logger << "SetupDiOpenDevRegKey DIREG_DRV failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        else {
            enumKey(regKey.get());
        }
        regKey = openDeviceSoftwareKey(ctx->hDevInfo, &ctx->devInfoData);
        if (!regKey.isValid()) {
            // this should not happen
            logger << "SetupDiOpenDevRegKey DIREG_DRV failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        else {
            enumKey(regKey.get());
        }
        logger.flush(Logger::INFO_LVL);
    }
    else {
        std::string portName = getPortName(ctx);
        logger << ctx->sep << portName;
        ctx->sep = ", ";
    }
    return 0;
}

CallbackFunc PortDeviceManager::RemoveCallback()
{
    CallbackFunc f = std::bind(&PortDeviceManager::removePortCallback, this, std::placeholders::_1);
    return  f;
}

CallbackFunc PortDeviceManager::ListCallback()
{
    CallbackFunc f = std::bind(&PortDeviceManager::listPortCallback, this, std::placeholders::_1);
    return f;
}

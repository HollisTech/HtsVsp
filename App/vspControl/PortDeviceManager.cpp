#include "PortDeviceManager.h"
using namespace DeviceManager;

std::string PortDeviceManager::getPortName(enumContext* context)
{
    std::string portName("");
    RegKeyHandle regKey(openDeviceHardwareKey(context->hDevInfo, &context->devInfoData), api());
    if (!regKey.isValid()) {
        // this should not happen
    }
    else {
        DWORD portNameSize = 0;
        if (api()->RegQueryValueEx(regKey.get(), TEXT("PortName"), NULL, NULL, &portNameSize) == ERROR_SUCCESS) {
            std::vector<char> buffer(portNameSize);
            if (api()->RegQueryValueEx(regKey.get(), TEXT("PortName"), NULL, (LPBYTE)buffer.data(), &portNameSize) == ERROR_SUCCESS) {
                portName = std::string(buffer.data());
            }
        }
    }
    return portName;
}

int PortDeviceManager::devicePortCallback(enumContext* context)
{
    enumDeviceContext* ctx = (enumDeviceContext*)context;
    std::string portName = str_toupper(getPortName(ctx));
    if (portName == ctx->targetName) {
        // found the port to remove
        ctx->found = true;
        // terminate the enumeration
        return 1;
    }
    return 0;
}

int PortDeviceManager::listPortCallback(enumContext* context)
{
    enumListContext* ctx = (enumListContext*)context;
    if (isVerbose()) {
        RegKeyHandle regKey(openDeviceHardwareKey(ctx->hDevInfo, &ctx->devInfoData), api());
        if (!regKey.isValid()) {
            // this should not happen
        }
        else {
            enumKey(regKey.get());
        }
        regKey = openDeviceSoftwareKey(ctx->hDevInfo, &ctx->devInfoData);
        if (!regKey.isValid()) {
            // this should not happen
        }
        else {
            enumKey(regKey.get());
        }
    }
    else {
        std::string portName = getPortName(ctx);
        logger << ctx->sep << portName;
        ctx->sep = ", ";
    }
    ctx->found = true;
    return 0;
}


CallbackFunc PortDeviceManager::enableCallback()
{
    CallbackFunc f = std::bind(&PortDeviceManager::devicePortCallback, this, std::placeholders::_1);
    return  f;
}

CallbackFunc PortDeviceManager::disableCallback()
{
    CallbackFunc f = std::bind(&PortDeviceManager::devicePortCallback, this, std::placeholders::_1);
    return  f;
}

CallbackFunc PortDeviceManager::removeCallback()
{
    CallbackFunc f = std::bind(&PortDeviceManager::devicePortCallback, this, std::placeholders::_1);
    return  f;
}

CallbackFunc PortDeviceManager::listCallback()
{
    CallbackFunc f = std::bind(&PortDeviceManager::listPortCallback, this, std::placeholders::_1);
    return f;
}

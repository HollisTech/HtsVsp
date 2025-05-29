#include "DeviceManager.h"
#include <tchar.h>
#include <cfgmgr32.h>

extern Logger logger;

namespace DeviceManager { 

    std::string str_toupper(std::string s)
    {
        auto s1 = s;
        std::transform(s1.begin(), s1.end(), s1.begin(),
            [](unsigned char c) { return std::toupper(c); }
        );
        return s1;
    }

    LSTATUS DeviceManager::getRegValue(HKEY regKey, const std::string& valueName, std::vector<char>& valueData)
    {
        DWORD valueDataSize = (DWORD)valueData.size();
        return api()->RegQueryValueEx(regKey, valueName, NULL, reinterpret_cast<LPBYTE>(valueData.data()), &valueDataSize);
    }

    HKEY DeviceManager::openDeviceSoftwareKey(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData)
    {
        HKEY regKey = api()->SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
        if (regKey == INVALID_HANDLE_VALUE) {
            logger << "SetupDiOpenDevRegKey DIREG_DRV failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return regKey;
    }

     HKEY DeviceManager::openDeviceHardwareKey(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData)
    {
        HKEY regKey = api()->SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (regKey == INVALID_HANDLE_VALUE) {
            logger << "SetupDiOpenDevRegKey DIREG_DRV failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return regKey;
    }

    std::vector<char> DeviceManager::getDeviceHwIds(
        HDEVINFO hDevInfo,
        SP_DEVINFO_DATA devInfoData)
    {
        std::vector<char> buffer;
        DWORD requiredSize = 0;
        if (!api()->SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, NULL, 0, &requiredSize)) {
            DWORD error = api()->getLastError();
            if (error != ERROR_INSUFFICIENT_BUFFER) {
                if (error != ERROR_NO_SUCH_DEVINST) {
                    logger << "Failed to get device registry property. Error: " << std::hex << api()->getLastError() << std::endl;
                    logger.flush(Logger::ERROR_LVL);
                }
                return buffer;
            }
        }
        buffer.resize(requiredSize);
        if (!api()->SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, reinterpret_cast<PBYTE>(buffer.data()), (DWORD)buffer.size(), NULL)) {
            logger << "Failed to get device registry property. Error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return buffer;
    }

    void DeviceManager::enumKey(HKEY regKey)
    {
        // enumerate key
        std::string valueName;
        DWORD valueType;
        DWORD index = 0;
        LSTATUS status = ERROR_SUCCESS;
        
        while (status == ERROR_SUCCESS) {
            std::vector<char> valueData;
            DWORD valueDataSize = 0;
            status = api()->RegEnumValue(regKey, index, valueName, &valueType, NULL, &valueDataSize);
            if (status != ERROR_SUCCESS) {
                break;
            }
            valueData.resize(valueDataSize);
            status = getRegValue(regKey, valueName, valueData);
            if (status != ERROR_SUCCESS) {
                logger << "Failed to get value data. Error: " << std::hex << status << "\n";
                logger.flush(Logger::ERROR_LVL);
                break;
            }
            std::string valueNameStr(valueName);
            valueNameStr += ": ";
            logger << std::setw(20) << valueNameStr;
            switch (valueType) {
            case REG_EXPAND_SZ: // could use ExpandEnvironmentStrings on the buffer.
            case REG_SZ:
                logger << (char*)valueData.data() << "\n";
                break;
            case REG_DWORD:
                logger << *(DWORD*)valueData.data() << "\n";
                break;
            case REG_MULTI_SZ:
            {
                char* p = (char*)valueData.data();
                char sep = 0;
                while (*p) {
                    logger << sep << p << " ";
                    p += strlen(p) + 1;
                    sep = ',';
                }
                logger << "\n";
            }
            break;
            case REG_BINARY:
            {
                logger << std::hex;
                char sep = 0;
                char prev = std::cout.fill('0');
                for (DWORD i = 0; i < valueDataSize; i++) {
                    logger << sep << std::setw(2) << (int)valueData[i];
                    sep = ' ';
                }
                std::cout.fill(prev);
                logger << std::dec << "\n";
            }
            break;
            default:
                logger << " " << valueType << "\n";
                break;
            }
            index++;
        }
    }
    
    HDEVINFO DeviceManager::getDevInfoSet(DWORD flags)
    {
        return api()->SetupDiGetClassDevs(getClassGuid(), flags);
    }

    BOOL DeviceManager::uninstallDriver(const std::string& infFile)
    {
        DWORD flags = DIGCF_PRESENT;
        HDevInfoHandle hDevInfo(getDevInfoSet(flags), api());

        if (!hDevInfo.isValid()) {
            logger << "Failed to get class devices. Error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }

        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        for (DWORD i = 0; api()->SetupDiEnumDeviceInfo(hDevInfo.get(), i, &devInfoData); ++i) {

            std::vector<char> buffer = getDeviceHwIds(hDevInfo.get(), devInfoData);
            if (buffer.empty()) {
                continue;
            }
            // match the hwid
            bool match = false;
            for (const char* currStringPtr = &buffer[0]; *currStringPtr != '\0'; currStringPtr += strlen(currStringPtr) + 1) {
                // case insensitive compare
                if (_stricmp(currStringPtr, hwid.c_str()) == 0) {
                    match = true;
                    break;
                }
            }
            if (!match) {
                continue;
            }

            // get the 'oem' inf file
            std::vector<char> infBuffer;
            DWORD infSize = 0;
            RegKeyHandle regKeyDrv(openDeviceSoftwareKey(hDevInfo.get(), &devInfoData),api());
            if (regKeyDrv.isValid()) {
                if (api()->RegQueryValueEx(regKeyDrv.get(), TEXT("InfPath"), 
                    NULL, NULL, &infSize) == ERROR_SUCCESS) {
                    infBuffer.resize(infSize);

                    if (api()->RegQueryValueEx(regKeyDrv.get(), TEXT("InfPath"), 
                        NULL, LPBYTE(infBuffer.data()), &infSize) != ERROR_SUCCESS) {
                        logger << "Failed to get inf path. Error: " << std::hex << api()->getLastError() << "\n";
                        logger.flush(Logger::ERROR_LVL);
                    }
                }
                else {
                    logger << "Failed to get infpath size. Error: " << std::hex << api()->getLastError() << "\n";
                    logger.flush(Logger::ERROR_LVL);
                    enumKey(regKeyDrv.get());
                }
            }

            SP_REMOVEDEVICE_PARAMS rmdParams{ 0 };
            rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
            rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
            rmdParams.HwProfile = 0;
            if (!api()->SetupDiSetClassInstallParams(hDevInfo.get(), &devInfoData, &rmdParams.ClassInstallHeader, sizeof(rmdParams))) {
                logger << "Failed to set class install params. Error: " << std::hex << api()->getLastError() << std::endl;
                logger.flush(Logger::ERROR_LVL);
                return FALSE;
            }
            if (!api()->SetupDiCallClassInstaller(DIF_REMOVE, hDevInfo.get(), &devInfoData)) {
                logger << "Failed to call class installer. Error: " << std::hex << api()->getLastError() << std::endl;
                logger.flush(Logger::ERROR_LVL);
                return FALSE;
            }
            if (!infBuffer.empty()) {
                std::string infPath(infBuffer.data());
                logger << "Removing driver package for: " << infPath << "\n";
                logger.flush(Logger::INFO_LVL);
                // remove the driver
                if (!api()->SetupUninstallOEMInf(infPath.c_str(), SUOI_FORCEDELETE)) {
                    logger << "Failed to uninstall driver. Error: " << std::hex << api()->getLastError() << std::endl;
                    logger.flush(Logger::ERROR_LVL);
                    return FALSE;
                }
            }

        }
        return TRUE;
    }

    void DeviceManager::findHwIds(std::vector<std::string>& result)
    {
        DWORD flags = DIGCF_PRESENT;
        HDevInfoHandle hDevInfo(getDevInfoSet(flags), api());
        if (!hDevInfo.isValid()) {
            logger << "Failed to get class devices. Error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return;
        }
        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        for (DWORD i = 0; api()->SetupDiEnumDeviceInfo(hDevInfo.get(), i, &devInfoData); ++i) {
            std::vector<char> buffer = getDeviceHwIds(hDevInfo.get(), devInfoData);
            if (buffer.empty()) {
                continue;
            }
            // Parse the double-NUL-terminated string into a vector<string>
            const char* currStringPtr = &buffer[0];
            while (*currStringPtr)
            {
                // Current string is NUL-terminated, so get its length with wcslen
                const size_t currStringLength = strlen(currStringPtr);
                std::string currString{ currStringPtr, currStringLength };
                if (currString == getHwId()) {
                    // Add current string to result vector
                    result.push_back(currString);
                }
                currStringPtr += currStringLength + 1;
            }
        }
    }

   int SoftwareDeviceManager::installDriver(const std::string& infFile, bool uninstall)
    {
        std::string infPath = api()->getFullPath(infFile);
        if (uninstall) {
            uninstallDriver(infFile);
            logger << "htsvsp uninstalled" << std::endl;
            logger.flush(Logger::INFO_LVL);
        }
        SP_DEVINFO_DATA devInfoData;
        HDevInfoHandle hDevInfo(createNewDeviceInfoSet(&devInfoData), api());
        if (!hDevInfo.isValid()) {
            logger << "SetupDiCreateDeviceInfoList failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }

        if (!addNewDevice(hDevInfo.get(), &devInfoData)) {
            return 1;
        }

        if (!api()->updateDriver(infPath, getHwId())) {
            return 1;
        }
        RegKeyHandle regKey(openDeviceSoftwareKey(hDevInfo.get(), &devInfoData), api());
        if (!regKey.isValid()) {
            logger << "Failed to open device software key. Error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        else {
            enumKey(regKey.get());
        }
        regKey = openDeviceHardwareKey(hDevInfo.get(), &devInfoData);
        if (!regKey.isValid()) {
            logger << "Failed to open device hardware key. Error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        else {
            enumKey(regKey.get());
        }
        return 0;
    }

    int DeviceManager::listDevices()
    {
        enumListContext listContext = { 0 };
        listContext.sep = "";
        int retval = enumClassDevices(listCallback(), &listContext);
        logger.flush(Logger::INFO_LVL);
        return retval;
    }

    int DeviceManager::enumClassDevices(
        CallbackFunc callback, 
        enumContext* context,
        DWORD flags)
    {
        int retval = 1;
        HKEY regKey = (HKEY)INVALID_HANDLE_VALUE;
        HDevInfoHandle hDevInfo(getDevInfoSet(flags), api());
        if (!hDevInfo.isValid()) {
            logger << "Failed to get class devices. Error: " << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return retval;
        }
        context->hDevInfo = hDevInfo.get();
        context->devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        for (DWORD i = 0; api()->SetupDiEnumDeviceInfo(context->hDevInfo, i, &context->devInfoData); ++i) {
            bool match = false;
            std::vector<char> buffer = getDeviceHwIds(context->hDevInfo, context->devInfoData);
            if (buffer.empty()) {
                // not our device. skip
                continue;
            }
            // Parse the double-NUL-terminated string into a vector<string>
            const char* currStringPtr = &buffer[0];
            while (*currStringPtr)
            {
                // Current string is NUL-terminated, so get its length with wcslen
                const size_t currStringLength = strlen(currStringPtr);
                std::string currString{ currStringPtr, currStringLength };
                if (currString == getHwId()) {
                    match = true;
                    break;
                }
                currStringPtr += currStringLength + 1;
            }
            if (match && callback(context)) {
                if (context->found) {
                    retval = 0; // found at least one device
                }
                break;
            }
        }
        return retval;
    }

    HDEVINFO DeviceManager::createNewDeviceInfoSet(SP_DEVINFO_DATA * devInfoData)
    {
        HDEVINFO hDevInfoNew = api()->SetupDiCreateDeviceInfoList(getClassGuid(), NULL);
        if (hDevInfoNew == INVALID_HANDLE_VALUE) {
            logger << "SetupDiCreateDeviceInfoList failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        else {
            devInfoData->cbSize = sizeof(SP_DEVINFO_DATA);

            // Create a new device info set. The software device will exist in enum as ROOT\getClassName()\NNNN
            if (!api()->SetupDiCreateDeviceInfo(hDevInfoNew, getClassName().c_str(), getClassGuid(), NULL, NULL, DICD_GENERATE_ID, devInfoData)) {
                logger << "SetupDiCreateDeviceInfo failed error: " << std::hex << api()->getLastError() << std::endl;
                logger.flush(Logger::ERROR_LVL);
                api()->SetupDiDestroyDeviceInfoList(hDevInfoNew);
                hDevInfoNew = INVALID_HANDLE_VALUE;
            }
        }
        return hDevInfoNew;
    }

    int DeviceManager::setClassDeviceState(enumContext* context, PSP_CLASSINSTALL_HEADER params, DWORD paramsSize)
    {
        if (!api()->SetupDiSetClassInstallParams(context->hDevInfo, &context->devInfoData, params, paramsSize)) {
            logger << "SetupDiSetClassInstallParams failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }
        if (!api()->SetupDiCallClassInstaller(params->InstallFunction, context->hDevInfo, &context->devInfoData)) {
            logger << "SetupDiCallClassInstaller failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }
        return 0;
    }


    bool SoftwareDeviceManager::addNewDevice(
        HDEVINFO DeviceInfoSet,
        SP_DEVINFO_DATA* DeviceInfoData
    )
    {

        DWORD hwIdMSLen = (DWORD)getHwId().length() + 2; // double NUL-terminated
        std::vector<char> hwIdMS(hwIdMSLen);
        //copy the string and add the second NUL
        std::copy(getHwId().begin(), getHwId().end(), hwIdMS.begin());
        //
        // Add the HardwareID to the Device's HardwareID property.
        //
        if (!api()->SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
            DeviceInfoData,
            SPDRP_HARDWAREID,
            (LPBYTE)hwIdMS.data(), hwIdMSLen))
        {
            logger << "SetupDiSetDeviceRegistryProperty failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return false;
        }
        if (!api()->SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DeviceInfoSet, DeviceInfoData)) {
            logger << "SetupDiCallClassInstaller DIF_REGISTERDEVICE failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return false;
        }
        return true;
    }

    int SoftwareDeviceManager::addDevice()
    {
        SP_DEVINFO_DATA devInfoData;
        HDevInfoHandle hDevInfo(createNewDeviceInfoSet(&devInfoData), api());
        if (!hDevInfo.isValid()) {
            logger << "SetupDiCreateDeviceInfoList failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }
        // Add the HardwareID to the Device's HardwareID property.
        if (!addNewDevice(hDevInfo.get(), &devInfoData)) {
            return 1;
        }
        //  use DiInstallDevice.
        if (!api()->installDevice(hDevInfo.get(), &devInfoData)) {
            logger << "DiInstallDevice failed error: " << std::hex << api()->getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }
        return 0;
    }

    int SoftwareDeviceManager::removeDevice(const std::string& device)
    {
        enumDeviceContext removeContext = { 0 };
        removeContext.targetName = device;
        int retval = enumClassDevices(removeCallback(), &removeContext);
        if (retval == 0) {
            SP_REMOVEDEVICE_PARAMS rmdParams{ 0 };
            rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
            rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
            rmdParams.HwProfile = 0;
            if (setClassDeviceState((enumContext *)&removeContext, &rmdParams.ClassInstallHeader, sizeof(rmdParams)) != 0) {
                logger << "Failed to set class device state for DIF_REMOVE." << std::endl;
                logger.flush(Logger::ERROR_LVL);
                return 1;
            }
            return 0;
        }
        return 1;
    }

    int SoftwareDeviceManager::enableDevice(const std::string& device) 
    {
        enumDeviceContext enableContext = { 0 };
        enableContext.targetName = device;
        int retval = enumClassDevices(enableCallback(), &enableContext);
        if (retval == 0) {
            SP_PROPCHANGE_PARAMS params;
            params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
            params.StateChange = DICS_ENABLE;
            params.Scope = DICS_FLAG_GLOBAL;
            params.HwProfile = 0;
            if (setClassDeviceState((enumContext *)&enableContext, &params.ClassInstallHeader, sizeof(params)) != 0) {
                logger << "Failed to set class device state for DICS_ENABLE." << std::endl;
                logger.flush(Logger::ERROR_LVL);
                return 1;
            }
            return 0;
        }
        return 1;
    }
}

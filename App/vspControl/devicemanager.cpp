#include "DeviceManager.h"
#include <newdev.h>
#include <tchar.h>
#include <cfgmgr32.h>
#include "Logger.h"

extern Logger logger;

namespace DeviceManager {


    


#define UPDATEDRIVERFORPLUGANDPLAYDEVICES "UpdateDriverForPlugAndPlayDevicesA"
    typedef BOOL(WINAPI* UpdateDriverForPlugAndPlayDevicesProto)(_In_opt_ HWND hwndParent,
        _In_ LPCTSTR HardwareId,
        _In_ LPCTSTR FullInfPath,
        _In_ DWORD InstallFlags,
        _Out_opt_ PBOOL bRebootRequired);

#define DIINSTALLDEVICE "DiInstallDevice"
    typedef BOOL(WINAPI* DiInstallDeviceProto)(_In_opt_ HWND hwndParent,
        _In_ HDEVINFO DeviceInfoSet,
        _In_ PSP_DEVINFO_DATA DeviceInfoData,
        _In_ PSP_DRVINFO_DATA DriverInfoData,
        _In_ DWORD Flags,
        _Out_opt_ PBOOL NeedReboot);

#define DIUNINSTALLDRIVER "DiUninstallDriverA"
    typedef BOOL(WINAPI* DiUninstallDriverProto)(
        HWND   hwndParent,
        LPCSTR InfPath,
        DWORD  Flags,
        PBOOL  NeedReboot);

    LSTATUS DeviceManager::getRegValue(HKEY regKey, const std::string& valueName, std::vector<char>& valueData)
    {
        DWORD valueDataSize = (DWORD)valueData.size();
        return RegQueryValueEx(regKey, valueName.c_str(), NULL, NULL, reinterpret_cast<LPBYTE>(valueData.data()), &valueDataSize);
    }

    HKEY DeviceManager::openDeviceSoftwareKey(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData)
    {
        HKEY regKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
        if (regKey == INVALID_HANDLE_VALUE) {
            logger << "SetupDiOpenDevRegKey DIREG_DRV failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return regKey;
    }

     HKEY DeviceManager::openDeviceHardwareKey(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData)
    {
        HKEY regKey = SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (regKey == INVALID_HANDLE_VALUE) {
            logger << "SetupDiOpenDevRegKey DIREG_DRV failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return regKey;
    }

    std::string DeviceManager::getPortName(enumContext * context)
    {
        std::string portName("");
        RegKeyHandle regKey(openDeviceHardwareKey(context->hDevInfo, &context->devInfoData));
        if (regKey.get() == INVALID_HANDLE_VALUE) {
            // this should not happen
            logger << "SetupDiOpenDevRegKey DIREG_DRV failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        else {
            DWORD portNameSize = 0;
            if (RegQueryValueEx(regKey.get(), TEXT("PortName"), NULL, NULL, NULL, &portNameSize) == ERROR_SUCCESS) {
                std::vector<char> buffer(portNameSize);
                if (RegQueryValueEx(regKey.get(), TEXT("PortName"), NULL, NULL, (LPBYTE)buffer.data(), &portNameSize) == ERROR_SUCCESS) {
                    portName = std::string(buffer.data());
                }
            }
        }
        return portName;
    }

    std::vector<char> DeviceManager::getDeviceHwIds(
        HDEVINFO hDevInfo,
        SP_DEVINFO_DATA devInfoData)
    {
        std::vector<char> buffer;
        DWORD requiredSize = 0;
        if (!SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, NULL, 0, &requiredSize)) {
            DWORD error = GetLastError();
            if (error != ERROR_INSUFFICIENT_BUFFER) {
                if (error != ERROR_NO_SUCH_DEVINST) {
                    logger << "Failed to get device registry property. Error: " << std::hex << GetLastError() << std::endl;
                    logger.flush(Logger::ERROR_LVL);
                }
                return buffer;
            }
        }
        buffer.resize(requiredSize);
        if (!SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, reinterpret_cast<PBYTE>(buffer.data()), (DWORD)buffer.size(), NULL)) {
            logger << "Failed to get device registry property. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return buffer;
    }

    

    void DeviceManager::enumKey(HKEY regKey)
    {
        // enumerate key
        CHAR valueName[256];
        DWORD valueType;
        DWORD index = 0;
        LSTATUS status = ERROR_SUCCESS;
        
        while (status == ERROR_SUCCESS) {
            std::vector<char> valueData;
            DWORD valueDataSize = 0;
            DWORD valueNameSize = sizeof(valueName);
            status = RegEnumValue(regKey, index, valueName, &valueNameSize, NULL, &valueType, NULL, &valueDataSize);
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

    std::string DeviceManager::getFullPath(const std::string& path) {
        try {
            std::filesystem::path p(path);
            return std::filesystem::absolute(p).string();
        }
        catch (const std::exception& e) {
            logger << "Failed to get full path: " << e.what() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return std::string("");
        }
    }
    
    HDEVINFO DeviceManager::getDevInfoSet(DWORD flags)
    {
        return SetupDiGetClassDevs(getClassGuid(), NULL, NULL, flags);
    }

    BOOL DeviceManager::removeDriver(const std::string& infFile)
    {
        ModuleHandle hNewDev(LoadLibrary(TEXT("newdev.dll")));
        if (hNewDev.get() == NULL) {
            logger << "Failed to load newdev.dll. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
        DiUninstallDriverProto diUninstallDriver = (DiUninstallDriverProto)GetProcAddress(hNewDev.get(), DIUNINSTALLDRIVER);
        if (diUninstallDriver == NULL) {
            logger << "Failed to get proc address for DiUninstallDriver. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
        BOOL result = diUninstallDriver(NULL, TEXT(infFile.c_str()), 0, NULL);
        if (!result) {
            logger << "Failed to remove the driver. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return result;
    }

    BOOL DeviceManager::uninstallDriver(const std::string& infFile)
    {
        DWORD flags = DIGCF_PRESENT;
        HDevInfoHandle hDevInfo(getDevInfoSet(flags));

        if (hDevInfo.get() == INVALID_HANDLE_VALUE) {
            logger << "Failed to get class devices. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }

        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo.get(), i, &devInfoData); ++i) {

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
            RegKeyHandle regKeyDrv(openDeviceSoftwareKey(hDevInfo.get(), &devInfoData));
            if (regKeyDrv.get() != INVALID_HANDLE_VALUE) {
                if (RegQueryValueEx(regKeyDrv.get(), TEXT("InfPath"), NULL, NULL, NULL, &infSize) == ERROR_SUCCESS) {
                    infBuffer.resize(infSize);
                    if (RegQueryValueEx(regKeyDrv.get(), TEXT("InfPath"), NULL, NULL, reinterpret_cast<LPBYTE>(infBuffer.data()), &infSize) != ERROR_SUCCESS) {
                        logger << "Failed to get inf path. Error: " << std::hex << GetLastError() << "\n";
                        logger.flush(Logger::ERROR_LVL);
                    }
                }
                else {
                    logger << "Failed to get infpath size. Error: " << std::hex << GetLastError() << "\n";
                    logger.flush(Logger::ERROR_LVL);
                    enumKey(regKeyDrv.get());
                }
            }

            SP_REMOVEDEVICE_PARAMS rmdParams{ 0 };
            rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
            rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
            rmdParams.HwProfile = 0;
            if (!SetupDiSetClassInstallParams(hDevInfo.get(), &devInfoData, &rmdParams.ClassInstallHeader, sizeof(rmdParams))) {
                logger << "Failed to set class install params. Error: " << std::hex << GetLastError() << std::endl;
                logger.flush(Logger::ERROR_LVL);
                return FALSE;
            }
            if (!SetupDiCallClassInstaller(DIF_REMOVE, hDevInfo.get(), &devInfoData)) {
                logger << "Failed to call class installer. Error: " << std::hex << GetLastError() << std::endl;
                logger.flush(Logger::ERROR_LVL);
                return FALSE;
            }
            if (!infBuffer.empty()) {
                std::string infPath(infBuffer.data());
                logger << "Removing driver package for: " << infPath << "\n";
                logger.flush(Logger::INFO_LVL);
                // remove the driver
                if (!SetupUninstallOEMInf(infPath.c_str(), SUOI_FORCEDELETE, NULL)) {
                    logger << "Failed to uninstall driver. Error: " << std::hex << GetLastError() << std::endl;
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
        HDevInfoHandle hDevInfo(getDevInfoSet(flags));
        if (hDevInfo.get() == INVALID_HANDLE_VALUE) {
            logger << "Failed to get class devices. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return;
        }
        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo.get(), i, &devInfoData); ++i) {
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

    BOOL DeviceManager::updateDriver(const std::string& infFile)
    {
        ModuleHandle hNewDev(LoadLibrary(TEXT("newdev.dll")));
        if (hNewDev.get() == NULL) {
            logger << "Failed to load newdev.dll. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
        UpdateDriverForPlugAndPlayDevicesProto updateDriverForPlugAndPlayDevices = (UpdateDriverForPlugAndPlayDevicesProto)GetProcAddress(hNewDev.get(), UPDATEDRIVERFORPLUGANDPLAYDEVICES);
        if (updateDriverForPlugAndPlayDevices == NULL) {
            logger << "Failed to get proc address for UpdateDriverForPlugAndPlayDevices. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
        BOOL result = updateDriverForPlugAndPlayDevices(NULL, TEXT(hwid.c_str()), TEXT(infFile.c_str()), INSTALLFLAG_FORCE, NULL);
        if (!result) {
            logger << "Failed to install driver. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return result;
    }

    int SoftwareDeviceManager::installDriver(const std::string& infFile, bool uninstall)
    {
        std::string infPath = getFullPath(infFile);
        if (uninstall) {
            uninstallDriver(infFile);
            logger << "htsvsp uninstalled" << std::endl;
            logger.flush(Logger::INFO_LVL);
        }
        SP_DEVINFO_DATA devInfoData;
        HDevInfoHandle hDevInfo(createNewDeviceInfoSet(&devInfoData));
        if (hDevInfo.get() == INVALID_HANDLE_VALUE) {
            logger << "SetupDiCreateDeviceInfoList failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }

        if (!addNewDevice(hDevInfo.get(), &devInfoData)) {
            return 1;
        }

        if (!updateDriver(infPath)) {
            return 1;
        }
        RegKeyHandle regKeyDrv(openDeviceSoftwareKey(hDevInfo.get(), &devInfoData));
        if (regKeyDrv.get() == INVALID_HANDLE_VALUE) {
            logger << "Failed to open device software key. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        else {
            enumKey(regKeyDrv.get());
        }
        RegKeyHandle regKeyDev(openDeviceHardwareKey(hDevInfo.get(), &devInfoData));
        if (regKeyDev.get() == INVALID_HANDLE_VALUE) {
            logger << "Failed to open device hardware key. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        else {
            enumKey(regKeyDev.get());
        }
        return 0;
    }

    int DeviceManager::listDevices()
    {
        enumListContext listContext = { 0 };
        listContext.sep = "";
        int retval = enumClassDevices(ListCallback(), &listContext);
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
        HDevInfoHandle hDevInfo(getDevInfoSet(flags));
        if (hDevInfo.get() == INVALID_HANDLE_VALUE) {
            logger << "Failed to get class devices. Error: " << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return retval;
        }
        context->hDevInfo = hDevInfo.get();
        context->devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        for (DWORD i = 0; SetupDiEnumDeviceInfo(context->hDevInfo, i, &context->devInfoData); ++i) {
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
                break;
            }
        }
        return retval;
    }

    HDEVINFO DeviceManager::createNewDeviceInfoSet(SP_DEVINFO_DATA * devInfoData)
    {
        HDEVINFO hDevInfoNew = SetupDiCreateDeviceInfoList(getClassGuid(), NULL);
        if (hDevInfoNew == INVALID_HANDLE_VALUE) {
            logger << "SetupDiCreateDeviceInfoList failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        else {
            devInfoData->cbSize = sizeof(SP_DEVINFO_DATA);

            // Create a new device info set. The software device will exist in enum as ROOT\getClassName()\NNNN
            if (!SetupDiCreateDeviceInfo(hDevInfoNew, getClassName().c_str(), getClassGuid(), NULL, NULL, DICD_GENERATE_ID, devInfoData)) {
                logger << "SetupDiCreateDeviceInfo failed error: " << std::hex << GetLastError() << std::endl;
                logger.flush(Logger::ERROR_LVL);
                SetupDiDestroyDeviceInfoList(hDevInfoNew);
                hDevInfoNew = INVALID_HANDLE_VALUE;
            }
        }
        return hDevInfoNew;
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
        if (!SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
            DeviceInfoData,
            SPDRP_HARDWAREID,
            (LPBYTE)hwIdMS.data(), hwIdMSLen))
        {
            logger << "SetupDiSetDeviceRegistryProperty failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return false;
        }
        if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DeviceInfoSet, DeviceInfoData)) {
            logger << "SetupDiCallClassInstaller DIF_REGISTERDEVICE failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return false;
        }
        return true;
    }

    int SoftwareDeviceManager::addDevice()
    {
        SP_DEVINFO_DATA devInfoData;
        HDevInfoHandle hDevInfo(createNewDeviceInfoSet(&devInfoData));
        if (hDevInfo.get() == INVALID_HANDLE_VALUE) {
            logger << "SetupDiCreateDeviceInfoList failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }
        // Add the HardwareID to the Device's HardwareID property.
        if (!addNewDevice(hDevInfo.get(), &devInfoData)) {
            return 1;
        }
        //  use DiInstallDevice.
        if (!installDevice(hDevInfo.get(), &devInfoData)) {
            logger << "DiInstallDevice failed error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return 1;
        }
        return 0;
    }

    int SoftwareDeviceManager::removeDevice(const std::string& device)
    {
        enumRemoveContext removeContext = { 0 };
        removeContext.targetName = device;
        int retval = enumClassDevices(RemoveCallback(), &removeContext);
        return removeContext.found ? 0 : 1;
    }

    BOOL SoftwareDeviceManager::installDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA* DeviceInfoData)
    {
        ModuleHandle hNewDev( LoadLibrary(TEXT("newdev.dll")));
        if (hNewDev.get() == NULL) {
            logger << "Failed to load newdev.dll. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }

        DiInstallDeviceProto diInstallDevice = (DiInstallDeviceProto)GetProcAddress(hNewDev.get(), DIINSTALLDEVICE);
        if (diInstallDevice == NULL) {
            logger << "Failed to get proc address for DiInstallDevice. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
#pragma warning(suppress: 6387)
        BOOL result = diInstallDevice(NULL, hDevInfo, DeviceInfoData, NULL, 0, NULL);
        return result;
    }
    
    
    std::string str_toupper(std::string s)
    {
        auto s1 = s;
        std::transform(s1.begin(), s1.end(), s1.begin(),
            [](unsigned char c) { return std::toupper(c); }
        );
        return s1;
    }
}

#include "DeviceManager.h"
#include <newdev.h>

extern Logger logger;

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

namespace DeviceManager {
    BOOL SystemApi::removeDriver(const std::string& infFile)
    {
        ModuleHandle hNewDev(LoadLibrary(TEXT("newdev.dll")), this);
        if (!hNewDev.isValid()) {
            logger << "Failed to load newdev.dll. Error: " << std::hex << getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
        DiUninstallDriverProto diUninstallDriver = (DiUninstallDriverProto)GetProcAddress(hNewDev.get(), DIUNINSTALLDRIVER);
        if (diUninstallDriver == NULL) {
            logger << "Failed to get proc address for DiUninstallDriver. Error: " << std::hex << getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
        BOOL result = diUninstallDriver(NULL, TEXT(infFile.c_str()), 0, NULL);
        if (!result) {
            logger << "Failed to remove the driver. Error: " << std::hex << getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return result;
    }

    BOOL SystemApi::updateDriver(const std::string& infFile, const std::string& hwid)
    {
        ModuleHandle hNewDev(LoadLibrary(TEXT("newdev.dll")), this);
        if (!hNewDev.isValid()) {
            logger << "Failed to load newdev.dll. Error: " << std::hex << getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
        UpdateDriverForPlugAndPlayDevicesProto updateDriverForPlugAndPlayDevices = (UpdateDriverForPlugAndPlayDevicesProto)GetProcAddress(hNewDev.get(), UPDATEDRIVERFORPLUGANDPLAYDEVICES);
        if (updateDriverForPlugAndPlayDevices == NULL) {
            logger << "Failed to get proc address for UpdateDriverForPlugAndPlayDevices. Error: " << std::hex << GetLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
        BOOL result = updateDriverForPlugAndPlayDevices(NULL, LPCTSTR(hwid.c_str()), LPCTSTR(infFile.c_str()), INSTALLFLAG_FORCE, NULL);
        if (!result) {
            logger << "Failed to install driver. Error: " << std::hex << getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
        }
        return result;
    }

    BOOL SystemApi::installDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA* DeviceInfoData)
    {
        ModuleHandle hNewDev(LoadLibrary(TEXT("newdev.dll")), this);
        if (!hNewDev.isValid()) {
            logger << "Failed to load newdev.dll. Error: " << std::hex << getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }

        DiInstallDeviceProto diInstallDevice = (DiInstallDeviceProto)GetProcAddress(hNewDev.get(), DIINSTALLDEVICE);
        if (diInstallDevice == NULL) {
            logger << "Failed to get proc address for DiInstallDevice. Error: " << std::hex << getLastError() << std::endl;
            logger.flush(Logger::ERROR_LVL);
            return FALSE;
        }
#pragma warning(suppress: 6387)
        BOOL result = diInstallDevice(NULL, hDevInfo, DeviceInfoData, NULL, 0, NULL);
        return result;
    }

}

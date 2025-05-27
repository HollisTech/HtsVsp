#pragma once
#include "../VspControl/DeviceManager.h"

using namespace DeviceManager;
using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::NiceMock;

// 1. Mock for ISystemApi
class MockSystemApi : public ISystemApi {
public:
    MOCK_METHOD(HDEVINFO, SetupDiGetClassDevs, (const GUID*, DWORD), (override));
    MOCK_METHOD(HKEY, SetupDiOpenDevRegKey, (HDEVINFO, PSP_DEVINFO_DATA, DWORD, DWORD, DWORD, REGSAM), (override));
    MOCK_METHOD(BOOL, SetupDiEnumDeviceInfo, (HDEVINFO, DWORD, PSP_DEVINFO_DATA), (override));
    MOCK_METHOD(BOOL, SetupDiSetClassInstallParams, (HDEVINFO, PSP_DEVINFO_DATA, PSP_CLASSINSTALL_HEADER, size_t), (override));
    MOCK_METHOD(BOOL, SetupDiCallClassInstaller, (DWORD, HDEVINFO, PSP_DEVINFO_DATA), (override));
    MOCK_METHOD(BOOL, SetupUninstallOEMInf, (const char*, DWORD), (override));
    MOCK_METHOD(BOOL, SetupDiGetDeviceRegistryProperty, (HDEVINFO, PSP_DEVINFO_DATA, DWORD, DWORD*, PBYTE, DWORD, DWORD*), (override));
    MOCK_METHOD(BOOL, SetupDiSetDeviceRegistryProperty, (HDEVINFO, PSP_DEVINFO_DATA, DWORD, const BYTE*, DWORD), (override));
    MOCK_METHOD(HDEVINFO, SetupDiCreateDeviceInfoList, (const GUID*, HWND), (override));
    MOCK_METHOD(BOOL, SetupDiCreateDeviceInfo, (HDEVINFO, const char*, const GUID*, const char*, HWND, DWORD, SP_DEVINFO_DATA*), (override));
    MOCK_METHOD(BOOL, SetupDiDestroyDeviceInfoList, (HDEVINFO), (override));
    MOCK_METHOD(HKEY, RegOpenKey, (HKEY, const std::string&), (override));
    MOCK_METHOD(LSTATUS, RegCloseKey, (HKEY), (override));
    MOCK_METHOD(LSTATUS, RegQueryValueEx, (HKEY, const std::string&, DWORD*, LPBYTE, DWORD*), (override));
    MOCK_METHOD(LSTATUS, RegQueryValue, (HKEY, const std::string&, std::vector<char>&), (override));
    MOCK_METHOD(LSTATUS, RegEnumValue, (HKEY, DWORD, std::string&, DWORD*, LPBYTE, DWORD*), (override));
    MOCK_METHOD(BOOL, FreeLibrary, (HMODULE), (override));
    MOCK_METHOD(BOOL, updateDriver, (const std::string&, const std::string&), (override));
    MOCK_METHOD(BOOL, installDevice, (HDEVINFO, SP_DEVINFO_DATA*), (override));
    MOCK_METHOD(BOOL, removeDriver, (const std::string&), (override));
    MOCK_METHOD(std::string, getFullPath, (const std::string&), (override));
    MOCK_METHOD(DWORD, getLastError, (), (override));
};

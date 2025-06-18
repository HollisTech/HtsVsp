#pragma once
namespace DeviceManager {
    class ISystemApi {
    public:
        virtual ~ISystemApi() = default;

        virtual HDEVINFO SetupDiGetClassDevs(const GUID* ClassGuid, DWORD flags) = 0;

        virtual HKEY SetupDiOpenDevRegKey(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD Scope,
            DWORD HwProfile, DWORD KeyType, REGSAM samDesired) = 0;

        virtual BOOL SetupDiEnumDeviceInfo(HDEVINFO DeviceInfoSet, DWORD MemberIndex, PSP_DEVINFO_DATA DeviceInfoData) = 0;

        virtual BOOL SetupDiSetClassInstallParams(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
            PSP_CLASSINSTALL_HEADER ClassInstallParams, size_t size) = 0;

        virtual BOOL SetupDiCallClassInstaller(DWORD InstallFunction, HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData) = 0;

        virtual BOOL SetupUninstallOEMInf(const char* infFile, DWORD flags) = 0;

        virtual BOOL SetupDiGetDeviceRegistryProperty(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD Property,
            DWORD* PropertyRegDataType, PBYTE PropertyBuffer, DWORD PropertyBufferSize, DWORD* RequiredSize) = 0;

        virtual BOOL SetupDiSetDeviceRegistryProperty(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD Property,
            const BYTE* PropertyBuffer, DWORD PropertyBufferSize) = 0;

        virtual HDEVINFO SetupDiCreateDeviceInfoList(const GUID* ClassGuid, HWND hwndParent) = 0;

        virtual BOOL SetupDiCreateDeviceInfo(HDEVINFO DeviceInfoSet, const char* DeviceName, const GUID* ClassGuid,
            const char* DeviceDescription, HWND hwndParent, DWORD CreationFlags, SP_DEVINFO_DATA* DeviceInfoData) = 0;

        virtual BOOL SetupDiDestroyDeviceInfoList(HDEVINFO DeviceInfoSet) = 0;

        virtual HKEY RegOpenKey(HKEY hKey, const std::string& subKey) = 0;

        virtual LSTATUS RegCloseKey(HKEY hKey) = 0;

        virtual LSTATUS RegQueryValueEx(HKEY hKey, const std::string& valueName, DWORD* type, LPBYTE data, DWORD* size) = 0;

        virtual LSTATUS RegQueryValue(HKEY hKey, const std::string& valueName, std::vector<char>& valueData) = 0;

        virtual LSTATUS RegEnumValue(HKEY hKey, DWORD index, std::string& valueName, DWORD* type, LPBYTE data, DWORD* size) = 0;


        virtual BOOL FreeLibrary(HMODULE hModule) = 0;

        virtual BOOL updateDriver(const std::string& infFile, const std::string& hwid) = 0;

        virtual BOOL installDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA* DeviceInfoData) = 0;

        virtual BOOL removeDriver(const std::string& infFile) = 0;

        virtual std::string getFullPath(const std::string& path) = 0;

        virtual DWORD getLastError() = 0;
    };

    // Default implementation using real Windows API
    class SystemApi : public ISystemApi {
    public:
        HDEVINFO SetupDiGetClassDevs(const GUID* ClassGuid, DWORD flags) override {
            return ::SetupDiGetClassDevs(ClassGuid, nullptr, nullptr, flags);
        }

        HKEY SetupDiOpenDevRegKey(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
            DWORD Scope, DWORD HwProfile, DWORD KeyType, REGSAM samDesired) override {
            return ::SetupDiOpenDevRegKey(DeviceInfoSet, DeviceInfoData, Scope, HwProfile, KeyType, samDesired);
        }

        BOOL SetupDiEnumDeviceInfo(HDEVINFO DeviceInfoSet, DWORD MemberIndex,
            PSP_DEVINFO_DATA DeviceInfoData) override {
            return ::SetupDiEnumDeviceInfo(DeviceInfoSet, MemberIndex, DeviceInfoData);
        }

        BOOL SetupDiSetClassInstallParams(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
            PSP_CLASSINSTALL_HEADER ClassInstallParams, size_t size) override {
            return ::SetupDiSetClassInstallParams(DeviceInfoSet, DeviceInfoData,
                ClassInstallParams, (DWORD)size);
        }

        BOOL SetupDiCallClassInstaller(DWORD InstallFunction, HDEVINFO DeviceInfoSet,
            PSP_DEVINFO_DATA DeviceInfoData) override {
            return ::SetupDiCallClassInstaller(InstallFunction, DeviceInfoSet, DeviceInfoData);
        }

        BOOL SetupUninstallOEMInf(const char* infFile, DWORD flags) override {
            return ::SetupUninstallOEMInf(infFile, flags, NULL);
        }

        BOOL SetupDiGetDeviceRegistryProperty(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
            DWORD Property, DWORD* PropertyRegDataType, PBYTE PropertyBuffer, DWORD PropertyBufferSize,
            DWORD* RequiredSize) override
        {
            return ::SetupDiGetDeviceRegistryProperty(DeviceInfoSet, DeviceInfoData,
                Property, PropertyRegDataType, PropertyBuffer, PropertyBufferSize, RequiredSize);
        }

        BOOL SetupDiSetDeviceRegistryProperty(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
            DWORD Property, const BYTE* PropertyBuffer, DWORD PropertyBufferSize) override
        {
            return ::SetupDiSetDeviceRegistryProperty(DeviceInfoSet, DeviceInfoData,
                Property, PropertyBuffer, PropertyBufferSize);
        }

        HDEVINFO SetupDiCreateDeviceInfoList(const GUID* ClassGuid, HWND hwndParent) override {
            return ::SetupDiCreateDeviceInfoList(ClassGuid, hwndParent);
        }

        BOOL SetupDiCreateDeviceInfo(HDEVINFO DeviceInfoSet, const char* DeviceName, const GUID* ClassGuid,
            const char* DeviceDescription, HWND hwndParent, DWORD CreationFlags,
            SP_DEVINFO_DATA* DeviceInfoData) override
        {
            return ::SetupDiCreateDeviceInfoA(DeviceInfoSet, DeviceName, ClassGuid,
                DeviceDescription, hwndParent, CreationFlags, DeviceInfoData);
        }

        BOOL SetupDiDestroyDeviceInfoList(HDEVINFO DeviceInfoSet) override {
            return ::SetupDiDestroyDeviceInfoList(DeviceInfoSet);
        }


        HKEY RegOpenKey(HKEY hKey, const std::string& subKey) override {
            HKEY result = nullptr;
            ::RegOpenKeyExA(hKey, subKey.c_str(), 0, KEY_READ, &result);
            return result;
        }
        LSTATUS RegCloseKey(HKEY hKey) override {
            return ::RegCloseKey(hKey);
        }
        LSTATUS RegQueryValueEx(HKEY hKey, const std::string& valueName, DWORD* type, LPBYTE data, DWORD* size) override {
            return ::RegQueryValueEx(hKey, valueName.c_str(), nullptr, type, data, size);
        }
        LSTATUS RegQueryValue(HKEY hKey, const std::string& valueName, std::vector<char>& valueData) override {
            DWORD type = 0, size = 0;
            LSTATUS status = ::RegQueryValueEx(hKey, valueName.c_str(), nullptr, &type, nullptr, &size);
            if (status != ERROR_SUCCESS) return status;
            valueData.resize(size);
            return ::RegQueryValueExA(hKey, valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(valueData.data()), &size);
        }
        LSTATUS RegEnumValue(HKEY hKey, DWORD index, std::string& valueName, DWORD* type, LPBYTE data, DWORD* size) override {
            DWORD nameSize = 256;
            valueName.resize(nameSize);
            LSTATUS status = ::RegEnumValue(hKey, index, &valueName[0], &nameSize, nullptr, type, data, size);
            if (status == ERROR_SUCCESS) {
                valueName.resize(nameSize); // Resize to actual length
            }
            return status;
        }

        BOOL FreeLibrary(HMODULE hModule) override {
            return ::FreeLibrary(hModule);
        }
        BOOL updateDriver(const std::string& infFile, const std::string& hwid) override;
        BOOL installDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA* DeviceInfoData) override;
        BOOL removeDriver(const std::string& infFile)  override;
        std::string getFullPath(const std::string& path) override {
            try {
                std::filesystem::path p(path);
                return std::filesystem::absolute(p).string();
            }
            catch (const std::exception&) {
                return path;
            }
        }
        DWORD getLastError() override
        {
            return ::GetLastError();
        }
    };

    class HDevInfoHandle {
    public:
        HDevInfoHandle(HDEVINFO hDevInfo, ISystemApi* api) : _hDevInfo(hDevInfo), _api(api) {}
        ~HDevInfoHandle() {
            if (_hDevInfo != INVALID_HANDLE_VALUE) {
                _api->SetupDiDestroyDeviceInfoList(_hDevInfo);
            }
        }
        HDEVINFO get() {
            return _hDevInfo;
        }

        bool isValid() const { return _hDevInfo != INVALID_HANDLE_VALUE; }
    private:
        HDEVINFO _hDevInfo;
        ISystemApi* _api;
    };

    class RegKeyHandle {
    public:
        RegKeyHandle(HKEY hKey, ISystemApi* api) : _hKey(hKey), _api(api) {}
        ~RegKeyHandle() {
            if (_hKey) {
                _api->RegCloseKey(_hKey);
            }
        }
        // No Copy constructor
        RegKeyHandle(const RegKeyHandle& other) = delete;

        // No Copy assignment operator
        RegKeyHandle& operator=(const RegKeyHandle& other) = delete;

        // Assignment operator
        RegKeyHandle& operator=(HKEY hKey) {
            if (_hKey != hKey) {
                if (_hKey) {
                    _api->RegCloseKey(_hKey);
                }
                _hKey = hKey;
            }
            return *this;
        }

        HKEY get() const { return _hKey; }

        bool isValid() const { return _hKey != NULL; }

    private:
        HKEY _hKey;
        ISystemApi* _api;
    };

    class ModuleHandle {

    public:
        explicit ModuleHandle(HMODULE handle, ISystemApi* api) : handle_(handle), _api(api) {}

        ~ModuleHandle() {
            if (handle_ != NULL) {
                _api->FreeLibrary(handle_);
            }
        }

        // Prevent copying
        ModuleHandle(const ModuleHandle&) = delete;
        ModuleHandle& operator=(const ModuleHandle&) = delete;

        // Allow moving
        ModuleHandle(ModuleHandle&& other) noexcept : handle_(other.handle_), _api(other._api) {
            other.handle_ = NULL;
        }

        ModuleHandle& operator=(ModuleHandle&& other) noexcept {
            if (this != &other) {
                if (handle_ != NULL) {
                    FreeLibrary(handle_);
                }
                handle_ = other.handle_;
                other.handle_ = NULL;
                _api = other._api;
            }
            return *this;
        }

        HMODULE get() const { return handle_; }

        bool isValid() const { return handle_ != NULL; }

    private:
        HMODULE handle_;
        ISystemApi* _api;
    };
}

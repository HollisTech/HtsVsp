#pragma once
#include <windows.h>
#include <SetupAPI.h>
#include <string>
#include <devguid.h>
#include <iostream>
#include <vector>
#include <functional>
#include <infstr.h>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <algorithm>

namespace DeviceManager {
    
    class HDevInfoHandle {
    public:
        HDevInfoHandle(HDEVINFO hDevInfo) : _hDevInfo(hDevInfo) {}
        ~HDevInfoHandle() {
            if (_hDevInfo != INVALID_HANDLE_VALUE) {
                SetupDiDestroyDeviceInfoList(_hDevInfo);
            }
        }
        HDEVINFO get() {
            return _hDevInfo;
        }

        bool isValid() const { return _hDevInfo != INVALID_HANDLE_VALUE; }
    private:
        HDEVINFO _hDevInfo;
    };

    class RegKeyHandle {
    public:
        RegKeyHandle(HKEY hKey) : _hKey(hKey) {}
        ~RegKeyHandle() {
            if (_hKey) {
                RegCloseKey(_hKey);
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
                    RegCloseKey(_hKey);
                }
                _hKey = hKey;
            }
            return *this;
        }

        HKEY get() const { return _hKey; }

        bool isValid() const { return _hKey != NULL; }

    private:
        HKEY _hKey;
    };

    class ModuleHandle {

    public:
        explicit ModuleHandle(HMODULE handle) : handle_(handle) {}

        ~ModuleHandle() {
            if (handle_ != NULL) {
                FreeLibrary(handle_);
            }
        }

        // Prevent copying
        ModuleHandle(const ModuleHandle&) = delete;
        ModuleHandle& operator=(const ModuleHandle&) = delete;

        // Allow moving
        ModuleHandle(ModuleHandle&& other) noexcept : handle_(other.handle_) {
            other.handle_ = NULL;
        }

        ModuleHandle& operator=(ModuleHandle&& other) noexcept {
            if (this != &other) {
                if (handle_ != NULL) {
                    FreeLibrary(handle_);
                }
                handle_ = other.handle_;
                other.handle_ = NULL;
            }
            return *this;
        }

        HMODULE get() const { return handle_; }

        bool isValid() const { return handle_ != NULL; }

    private:
        HMODULE handle_;
    };

    struct enumContext {
        HDEVINFO hDevInfo;
        SP_DEVINFO_DATA devInfoData;
        DWORD Index;
    };

    struct enumListContext : enumContext {
        std::string sep;

    };

    struct enumRemoveContext : enumContext {
        std::string targetName;
        bool found;
    };

    std::string str_toupper(std::string s);

    /**
     * @brief Callback function type for device enumeration.
     *
     * @param context User-defined context passed to the callback function.
     * @return int Return 0 to continue enumeration, non-zero to stop.
     */
     //typedef int (*CallbackFunc)(_In_ PVOID context);
#define CallbackFunc std::function<int(enumContext*)>

    /**
     * @brief Base class for managing devices.
     *
     * Provides methods for installing, uninstalling, listing, adding, removing, enabling, and disabling devices.
     */
    class DeviceManager {
    public:
        /**
         * @brief Constructs a DeviceManager object.
         *
         * @param ClassName The name of the device class.
         * @param ClassGuid The GUID of the device class.
         * @param HwId The hardware ID of the device.
         */
        DeviceManager(const char* ClassName, GUID ClassGuid, const char* HwId)
            : className(ClassName), classGuid(ClassGuid), hwid(HwId) {
        }

        virtual ~DeviceManager() {}

        /**
         * @brief Installs a driver from an INF file.
         *
         * @param infFile The path to the INF file.
         * @param uninstall Whether to uninstall the existing driver before installing the new one.
         * @return int 0 on success, non-zero on failure.
         */
        virtual int installDriver(const std::string& infFile, bool uninstall = false) = 0;

        /**
         * @brief Uninstalls the driver.
         *
         * @return int 0 on success, non-zero on failure.
         */
        virtual int uninstallDriver(const std::string& infFile);

        /**
         * @brief Lists the devices.
         *
         * @return int 0 on success, non-zero on failure.
         */
        virtual int listDevices();

        /**
         * @brief Adds a device.
         *
         * @return int 0 on success, non-zero on failure.
         */
        virtual int addDevice() { return 0; }

        /**
         * @brief Removes a device.
         *
         * @param device The name of the device to remove.
         * @return int 0 on success, non-zero on failure.
         */
        virtual int removeDevice(const std::string& device) { return 0; }

        /**
         * @brief Enables a device.
         *
         * @param device The name of the device to enable.
         * @return int 0 on success, non-zero on failure.
         */
        virtual int enableDevice(const std::string& device) { return 0; }

        /**
         * @brief Disables a device.
         *
         * @param device The name of the device to disable.
         * @return int 0 on success, non-zero on failure.
         */
        virtual int disableDevice(const std::string& device) { return 0; }

    protected:
        /**
         * @brief Gets the class GUID.
         *
         * @return const GUID* Pointer to the class GUID.
         */
        const GUID* getClassGuid() { return &classGuid; }

        /**
         * @brief Gets the class name.
         *
         * @return const std::string& Reference to the class name.
         */
        const std::string& getClassName() { return className; }

        /**
         * @brief Gets the hardware ID.
         *
         * @return const std::string& Reference to the hardware ID.
         */
        const std::string& getHwId() { return hwid; }

        /**
         * @brief Enumerates the registry key.
         *
         * @param regKey The registry key to enumerate.
         */
        virtual void enumKey(HKEY regKey);

        /**
         * @brief Gets the full path of a file.
         *
         * @param path The relative path.
         * @return std::string The full path.
         */
        virtual std::string getFullPath(const std::string& path);

        /**
         * @brief Finds hardware IDs of devices.
         *
         * @param result Vector to store the found hardware IDs.
         */
        virtual void findHwIds(std::vector<std::string>& result);

        /**
         * @brief Gets the device information set.
         *
         * @param flags The flags for SetupDiGetClassDevs.
         * @return HDEVINFO The handle to the device information set.
         */
        virtual HDEVINFO getDevInfoSet(DWORD flags);

        /**
         * @brief Updates the driver using an INF file.
         *
         * @param infFile The path to the INF file.
         * @return int 0 on success, non-zero on failure.
         */
        virtual int updateDriver(const std::string& infFile);

        /**
         * @brief Removes the driver using an INF file.
         *
         * @param infFile The path to the INF file.
         * @return BOOL TRUE on success, FALSE on failure.
         */
        virtual BOOL removeDriver(const std::string& infFile);

        /**
         * @brief Gets the callback function for removing devices.
         *
         * @return CallbackFunc The callback function.
         */
        virtual CallbackFunc RemoveCallback() = 0;

        /**
         * @brief Gets the callback function for listing devices.
         *
         * @return CallbackFunc The callback function.
         */
        virtual CallbackFunc ListCallback() = 0;

        /**
         * @brief Enumerates class devices and calls a callback function for each device.
         *
         * @param callback The callback function.
         * @param context The user-defined context passed to the callback function.
         * @param flags The flags for SetupDiGetClassDevs.
         * @return int 0 on success, non-zero on failure.
         */
        virtual int enumClassDevices(CallbackFunc callback, enumContext * context, DWORD flags = DIGCF_PRESENT);

        /**
         * @brief Creates a new device information set.
         *
         * @param devInfoData The device information data.
         * @return HDEVINFO The handle to the new device information set.
         */
        virtual HDEVINFO createNewDeviceInfoSet(SP_DEVINFO_DATA* devInfoData);

        /**
         * @brief Gets the port name.
         *
         * @param context The context header containing device information.
         * @return std::string The port name.
         */
        std::string getPortName(enumContext* context);

        /**
         * @brief Opens the hardware key for a device.
         *
         * @param DeviceInfoSet The handle to the device information set.
         * @param DeviceInfoData The device information data.
         * @return HKEY The handle to the opened hardware key.
         */
        HKEY openDeviceHardwareKey(
            HDEVINFO DeviceInfoSet,
            PSP_DEVINFO_DATA DeviceInfoData);

        /**
         * @brief Opens the software key for a device.
         *
         * @param DeviceInfoSet The handle to the device information set.
         * @param DeviceInfoData The device information data.
         * @return HKEY The handle to the opened software key.
         */
        HKEY openDeviceSoftwareKey(
            HDEVINFO DeviceInfoSet,
            PSP_DEVINFO_DATA DeviceInfoData);

        /**
         * @brief Gets a registry value.
         *
         * @param regKey The handle to the registry key.
         * @param valueName The name of the value to retrieve.
         * @param valueData The buffer to store the value data.
         * @return LSTATUS The status of the registry query.
         */
        LSTATUS getRegValue(HKEY regKey,
            const std::string& valueName,
            std::vector<char>& valueData);

        /**
         * @brief Gets the hardware IDs of a device.
         *
         * @param hDevInfo The handle to the device information set.
         * @param devInfoData The device information data.
         * @return std::vector<char> The hardware IDs of the device.
         */
        std::vector<char> getDeviceHwIds(
            HDEVINFO hDevInfo,
            SP_DEVINFO_DATA devInfoData);

    private:
        GUID classGuid;              ///< The GUID of the device class.
        const std::string className; ///< The name of the device class.
        const std::string hwid;      ///< The hardware ID of the device.

        // Prevent copying
        DeviceManager(const DeviceManager& other) = delete;
        DeviceManager& operator=(const DeviceManager& other) = delete;
    };

    /**
     * @brief Derived class for managing software devices.
     *
     * Provides methods for adding and removing software devices.
     */
    class SoftwareDeviceManager : public DeviceManager {
    public:
        /**
         * @brief Constructs a SoftwareDeviceManager object.
         *
         * @param ClassName The name of the device class.
         * @param ClassGuid The GUID of the device class.
         * @param HwId The hardware ID of the device.
         */
        SoftwareDeviceManager(const char* ClassName, GUID ClassGuid, const char* HwId)
            : DeviceManager(ClassName, ClassGuid, HwId) {
        }

        virtual ~SoftwareDeviceManager() {}

        virtual int installDriver(const std::string& infFile, bool uninstall = false);

        /**
         * @brief Adds a device.
         *
         * @return int 0 on success, non-zero on failure.
         */
        virtual int addDevice();

        /**
         * @brief Removes a device.
         *
         * @param device The name of the device to remove.
         * @return int 0 on success, non-zero on failure.
         */
        virtual int removeDevice(const std::string& device);

    protected:
        /**
         * @brief Installs a device.
         *
         * @param hDevInfo The handle to the device information set.
         * @param DeviceInfoData The device information data.
         * @return BOOL TRUE on success, FALSE on failure.
         */
        virtual BOOL installDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA* DeviceInfoData);

        /**
         * @brief Adds a new device.
         *
         * @param DeviceInfoSet The handle to the device information set.
         * @param DeviceInfoData The device information data.
         * @return bool true on success, false on failure.
         */
        virtual bool addNewDevice(HDEVINFO DeviceInfoSet, SP_DEVINFO_DATA* DeviceInfoData);

    private:
        // Prevent copying
        SoftwareDeviceManager(const SoftwareDeviceManager& other) = delete;
        SoftwareDeviceManager& operator=(const SoftwareDeviceManager& other) = delete;
    };
}

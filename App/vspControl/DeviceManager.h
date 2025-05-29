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
#include "Logger.h"
#include "devapi.h"

namespace DeviceManager {    

    struct enumContext {
        HDEVINFO hDevInfo;
        SP_DEVINFO_DATA devInfoData;
        DWORD Index;
        bool found;
    };

    struct enumListContext : enumContext {
        std::string sep;

    };

    struct enumDeviceContext : enumContext {
        std::string targetName;
    };

    std::string str_toupper(std::string s);

    /**
     * @brief Callback function type for device enumeration.
     *
     * @param context User-defined context passed to the callback function.
     * @return int Return 0 to continue enumeration, non-zero to stop.
     * enumContext.found indicates that the target of the enumeration
     * was processed successfully.
     */
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
        DeviceManager(const char* ClassName, GUID ClassGuid, const char* HwId, ISystemApi* api)
            : className(ClassName), classGuid(ClassGuid), hwid(HwId), api_(api) {
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
        virtual int addDevice() = 0;

        /**
         * @brief Removes a device.
         *
         * @param device The name of the device to remove.
         * @return int 0 on success, non-zero on failure.
         */
        virtual int removeDevice(const std::string& device) = 0;

        /**
         * @brief Enables a device.
         *
         * @param device The name of the device to enable.
         * @return int 0 on success, non-zero on failure.
         */
        virtual int enableDevice(const std::string& device) = 0;

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
         * @brief Gets the callback function for removing devices.
         *
         * @return CallbackFunc The callback function.
         */
        virtual CallbackFunc removeCallback() = 0;

        /**
         * @brief Gets the callback function for listing devices.
         *
         * @return CallbackFunc The callback function.
         */
        virtual CallbackFunc listCallback() = 0;

        virtual CallbackFunc enableCallback() = 0;

        /**
         * @brief Enumerates class devices and calls a callback function for each device.
         *
         * @param callback The callback function.
         * @param context The user-defined context passed to the callback function.
         * @param flags The flags for SetupDiGetClassDevs.
         * @return int 0 on success nonzero on failure.
         * 
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

        int setClassDeviceState(enumContext* context, PSP_CLASSINSTALL_HEADER params, DWORD paramsSize);

        ISystemApi* api() { return api_; }

    private:
        GUID classGuid;              ///< The GUID of the device class.
        const std::string className; ///< The name of the device class.
        const std::string hwid;      ///< The hardware ID of the device.

        // Prevent copying
        DeviceManager(const DeviceManager& other) = delete;
        DeviceManager& operator=(const DeviceManager& other) = delete;

        ISystemApi* api_; ///< Pointer to the system API interface.
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
        SoftwareDeviceManager(const char* ClassName, GUID ClassGuid, const char* HwId, ISystemApi* api)
            : DeviceManager(ClassName, ClassGuid, HwId, api) {
        }

        virtual ~SoftwareDeviceManager() {}

        virtual int installDriver(const std::string& infFile, bool uninstall);

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

        virtual int enableDevice(const std::string& device);

    protected:

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

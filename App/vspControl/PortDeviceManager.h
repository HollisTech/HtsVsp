#pragma once

#include "DeviceManager.h"
using namespace DeviceManager;
/**
 * @brief Derived class for managing port devices.
 *
 * Specializes in managing port devices and provides methods for listing and removing port devices.
 */
class PortDeviceManager : public SoftwareDeviceManager {
public:
    /**
     * @brief Constructs a PortDeviceManager object.
     *
     * @param HwId The hardware ID of the port device.
     */
    PortDeviceManager(const char* HwId)
        : SoftwareDeviceManager("PORTS", GUID_DEVCLASS_PORTS, HwId) {
    }

    virtual ~PortDeviceManager() {}

    /**
     * @brief Gets the callback function for removing port devices.
     *
     * @return CallbackFunc The callback function.
     */
    virtual CallbackFunc RemoveCallback();

    /**
     * @brief Gets the callback function for listing port devices.
     *
     * @return CallbackFunc The callback function.
     */
    virtual CallbackFunc ListCallback();

    int removePortCallback(enumContext * context);

    int listPortCallback(enumContext* context);

private:
    // Prevent copying
    PortDeviceManager(const PortDeviceManager& other) = delete;
    PortDeviceManager& operator=(const PortDeviceManager& other) = delete;
};

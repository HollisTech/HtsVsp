#include <windows.h>
#include <SetupAPI.h>
#include <infstr.h>
#include <newdev.h>
#include <tchar.h>
#include <cfgmgr32.h>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <devguid.h>

// TBD override these default values with a json file.
const std::string hwid("UMDF\\HtsVsp");
const std::string infFile("htsvsp.inf");

GUID classGuid = GUID_DEVCLASS_PORTS;
const std::string className("PORTS");

#define UPDATEDRIVERFORPLUGANDPLAYDEVICES "UpdateDriverForPlugAndPlayDevicesA"
typedef BOOL(WINAPI* UpdateDriverForPlugAndPlayDevicesProto)(_In_opt_ HWND hwndParent,
	_In_ LPCTSTR HardwareId,
	_In_ LPCTSTR FullInfPath,
	_In_ DWORD InstallFlags,
	_Out_opt_ PBOOL bRebootRequired	);

#define DIINSTALLDEVICE "DiInstallDevice"
typedef BOOL(WINAPI* DiInstallDeviceProto)(_In_opt_ HWND hwndParent,
	_In_ HDEVINFO DeviceInfoSet,
	_In_ PSP_DEVINFO_DATA DeviceInfoData,
	_In_ PSP_DRVINFO_DATA DriverInfoData,
	_In_ DWORD Flags,
	_Out_opt_ PBOOL NeedReboot
	);

BOOL installDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA *DeviceInfoData)
{
	HMODULE hNewDev = LoadLibrary(TEXT("newdev.dll"));
	if (hNewDev == NULL) {
		std::cerr << "Failed to load newdev.dll. Error: " << GetLastError() << std::endl;
		return FALSE;
	}

	DiInstallDeviceProto diInstallDevice = (DiInstallDeviceProto)GetProcAddress(hNewDev, DIINSTALLDEVICE);
	if (diInstallDevice == NULL) {
		std::cerr << "Failed to get proc address for DiInstallDevice. Error: " << GetLastError() << std::endl;
		FreeLibrary(hNewDev);
		return FALSE;
	}
	BOOL result = diInstallDevice(NULL, hDevInfo, DeviceInfoData, NULL, 0, NULL);
	if (!result) {
		std::cerr << "Failed to install device. Error: " << GetLastError() << std::endl;
	}

	FreeLibrary(hNewDev);
	return result;
}

BOOL updateDriver(const std::string& infFile)
{
	HMODULE hNewDev = LoadLibrary(TEXT("newdev.dll"));
	if (hNewDev == NULL) {
		std::cerr << "Failed to load newdev.dll. Error: " << GetLastError() << std::endl;
		return FALSE;
	}
	UpdateDriverForPlugAndPlayDevicesProto updateDriverForPlugAndPlayDevices = (UpdateDriverForPlugAndPlayDevicesProto)GetProcAddress(hNewDev, UPDATEDRIVERFORPLUGANDPLAYDEVICES);
	if (updateDriverForPlugAndPlayDevices == NULL) {
		std::cerr << "Failed to get proc address for UpdateDriverForPlugAndPlayDevices. Error: "<< std::hex << GetLastError() << std::endl;
		FreeLibrary(hNewDev);
		return FALSE;
	}
	BOOL result = updateDriverForPlugAndPlayDevices(NULL, TEXT(hwid.c_str()), TEXT(infFile.c_str()), INSTALLFLAG_FORCE, NULL);
	if (!result) {
		std::cerr << "Failed to install driver. Error: " << std::hex << GetLastError() << std::endl;
	}
	FreeLibrary(hNewDev);
	return result;
}

std::string  getINFClass(
	const std::string& infFilePath,
	GUID* classGuid)
{
	GUID guid;
	char className[MAX_CLASS_NAME_LEN];
	DWORD requiredSize = 0;
	std::string infClass;

	BOOL result = SetupDiGetINFClass(
		infFilePath.c_str(),
		&guid,
		className,
		sizeof(className),
		&requiredSize
	);

	if (result) {
		WCHAR guidString[40] = { 0 };
		(void)StringFromGUID2(guid, (LPOLESTR)guidString, 40);
		std::wcout << L"Class GUID: " << guidString << std::endl;
		infClass = className;
		std::cout << "Class Name: " << className << std::endl;
		if (classGuid) {
			*classGuid = guid;
		}
	}
	else {
		std::cerr << "Failed to get INF class. Error: " << GetLastError() << std::endl;
	}
	return infClass;
}

std::string getFullPath(const std::string& path) {
	try {
		std::filesystem::path p(path);
		return std::filesystem::absolute(p).string();
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to get full path: " << e.what() << std::endl;
		return std::string("");
	}
}

HDEVINFO getDevInfoSet(const GUID* classGuid, DWORD flags)
{
	HDEVINFO hDevInfo = SetupDiGetClassDevs(classGuid, NULL, NULL, DIGCF_PRESENT);
	return hDevInfo;
}

std::vector<char> getHwIds(
	HDEVINFO hDevInfo,
	SP_DEVINFO_DATA devInfoData)
{
	std::vector<char> buffer;
	DWORD requiredSize = 0;
	if (!SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, NULL, 0, &requiredSize)) {
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			std::cerr << "Failed to get device registry property. Error: " << GetLastError() << std::endl;
			return buffer;
		}
	}
	buffer.resize(requiredSize);
	if (!SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, reinterpret_cast<PBYTE>(buffer.data()), (DWORD)buffer.size(), NULL)) {
		std::cerr << "Failed to get device registry property. Error: " << GetLastError() << std::endl;
	}
	return buffer;
}

BOOL removeDriver(const std::string& hwid, GUID* classGuid)
{
	DWORD flags = classGuid ? DIGCF_PRESENT : DIGCF_ALLCLASSES | DIGCF_PRESENT;
	HDEVINFO hDevInfo = getDevInfoSet(classGuid, flags);
	if (hDevInfo == INVALID_HANDLE_VALUE) {
		std::cerr << "Failed to get class devices. Error: " << GetLastError() << std::endl;
		return FALSE;
	}

	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {

		std::vector<char> buffer = getHwIds(hDevInfo, devInfoData);
		if (buffer.empty()) {
			continue;
		}
		// match the hwid
		bool match = false;
		for (const char* currStringPtr = &buffer[0]; *currStringPtr != '\0'; currStringPtr += strlen(currStringPtr) + 1) {
			if (std::string(currStringPtr) == hwid) {
				match = true;
				break;
			}
		}
		if (!match) {
			continue;
		}

		SP_REMOVEDEVICE_PARAMS rmdParams{ 0 };
		rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
		rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
		rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
		rmdParams.HwProfile = 0;
		if (!SetupDiSetClassInstallParams(hDevInfo, &devInfoData, &rmdParams.ClassInstallHeader, sizeof(rmdParams))) {
			std::cerr << "Failed to set class install params. Error: " << GetLastError() << std::endl;
			return FALSE;
		}
		if (!SetupDiCallClassInstaller(DIF_REMOVE, hDevInfo, &devInfoData)) {
			std::cerr << "Failed to call class installer. Error: " << GetLastError() << std::endl;
			return FALSE;
		}
	}
	return TRUE;
}

void findHwIds(const std::string& hwid,
	GUID* classGuid,
	std::vector<std::string>& result)
{
	DWORD flags = classGuid ? DIGCF_PRESENT : DIGCF_ALLCLASSES | DIGCF_PRESENT;
	HDEVINFO hDevInfo = getDevInfoSet(classGuid, flags);
	if (hDevInfo == INVALID_HANDLE_VALUE) {
		std::cout << "Failed to get class devices. Error: " << GetLastError() << std::endl;
		return;
	}
	try {
		SP_DEVINFO_DATA devInfoData;
		devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
			DWORD requiredSize = 0;

			std::vector<char> buffer = getHwIds(hDevInfo, devInfoData);
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
				if (currString == hwid) {
					// Add current string to result vector
					result.push_back(currString);
				}
				currStringPtr += currStringLength + 1;
			}
		}
	}
	catch (const std::exception& e) {
		std::cout << "Exception: " << e.what() << std::endl;
	}
	SetupDiDestroyDeviceInfoList(hDevInfo);
}

void enumKey(HKEY regKey)
{
	// enumerate key
	CHAR valueName[256];
	DWORD valueNameSize = sizeof(valueName);
	DWORD valueType;
	BYTE valueData[256];
	DWORD valueDataSize = sizeof(valueData);
	DWORD index = 0;
	while (RegEnumValue(regKey, index, valueName, &valueNameSize, NULL, &valueType, valueData, &valueDataSize) == ERROR_SUCCESS) {
		std::string valueNameStr(valueName);
		valueNameStr += ": ";
		std::cout << std::setw(20) << valueNameStr;
		switch (valueType) {
		case REG_SZ:
			std::cout << (char *)valueData << std::endl;
			break;	
		case REG_DWORD:
			std::cout  << *(DWORD*)valueData << std::endl;
			break;
		case REG_MULTI_SZ:
		{
			char* p = (char*)valueData;
			char sep = 0;
			while (*p) {
				std::cout << sep << p << " ";
				p += strlen(p) + 1;
				sep = ',';
			}
			std::cout << std::endl;
		}
		break;
		case REG_BINARY:
		{
			std::cout << std::hex;
			char sep = 0;
			char prev = std::cout.fill('0');
			for (DWORD i = 0; i < valueDataSize; i++) {
				std::cout << sep << std::setw(2) <<  (int)valueData[i];
				sep = ' ';
			}
			std::cout.fill(prev);
			std::cout << std::dec << std::endl;
		}
			break;
		default:
			std::cout << " " << valueType << std::endl;
			break;
		}
		index++;
		valueNameSize = sizeof(valueName);
		valueDataSize = sizeof(valueData);
	}
	std::cout << std::endl;
}

bool addNewDevice(
	HDEVINFO DeviceInfoSet,
	SP_DEVINFO_DATA *DeviceInfoData
)
{
	DWORD hwIdMSLen = (DWORD)hwid.length() + 2; // double NUL-terminated
	std::vector<char> hwIdMS(hwIdMSLen);
	//copy the string and add the second NUL
	std::copy(hwid.begin(), hwid.end(), hwIdMS.begin());
	//
	// Add the HardwareID to the Device's HardwareID property.
	//
	if (!SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
		DeviceInfoData,
		SPDRP_HARDWAREID,
		(LPBYTE)hwIdMS.data(), hwIdMSLen))
	{
		std::cout << "SetupDiSetDeviceRegistryProperty failed error: " << std::hex << GetLastError() << std::endl;
		return false;
	}
	if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DeviceInfoSet, DeviceInfoData)) {
		std::cout << "SetupDiCallClassInstaller DIF_REGISTERDEVICE failed error: " << std::hex << GetLastError() << std::endl;
		return false;
	}
	return true;
}

int addDevice()
{
	int retval = 1;
	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	// Create a new device info element
	HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList(&GUID_DEVCLASS_PORTS, NULL);
	if (hDevInfo == INVALID_HANDLE_VALUE) {
		std::cout << "SetupDiCreateDeviceInfoList failed error: " << GetLastError() << std::endl;
	}
	// Create a new device info set
	if (!SetupDiCreateDeviceInfo(hDevInfo, TEXT("PORTS"), &GUID_DEVCLASS_PORTS, NULL, NULL, DICD_GENERATE_ID, &devInfoData)) {
		std::cout << "SetupDiCreateDeviceInfo failed error: " << GetLastError() << std::endl;
		goto final;
	}
	// Add the HardwareID to the Device's HardwareID property.
	if (!addNewDevice(hDevInfo, &devInfoData)) {
		goto final;
	}
	//  use DiInstallDevice.
	if (!installDevice(hDevInfo, &devInfoData)) {
		std::cout << "DiInstallDevice failed error: " << std::hex << GetLastError() << std::endl;
		goto final;
	}
	retval = 0;

	final:
	// Clean up
	SetupDiDestroyDeviceInfoList(hDevInfo);
	return retval;
}

struct enumPortsContextHeader {
	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA devInfoData;
	DWORD Index;
	std::string portName;
};

struct enumPortsListContext {
	enumPortsContextHeader header;
	std::string sep;

};

struct enumPortsRemoveContext {
	enumPortsContextHeader header;
	std::string targetName;
};
typedef int (*CallbackFunc)(_In_ PVOID context);

int enumPortDevices(CallbackFunc callback, PVOID context)
{
	int retval = 1;
	HKEY regKey = (HKEY)INVALID_HANDLE_VALUE;
	enumPortsContextHeader* ctx = (enumPortsContextHeader*)context;
	ctx->hDevInfo = getDevInfoSet(&classGuid, DIGCF_PRESENT);
	if (ctx->hDevInfo == INVALID_HANDLE_VALUE) {
		std::cout << "Failed to get class devices. Error: " << GetLastError() << std::endl;
		return retval;
	}
	ctx->devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (DWORD i = 0; SetupDiEnumDeviceInfo(ctx->hDevInfo, i, &ctx->devInfoData); ++i) {
		std::vector<char> buffer = getHwIds(ctx->hDevInfo, ctx->devInfoData);
		if (buffer.empty()) {
			// not our port. skip
			continue;
		}
		// open the hardware key (i.e. the enum key
		regKey = SetupDiOpenDevRegKey(ctx->hDevInfo, &ctx->devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
		if (regKey == INVALID_HANDLE_VALUE) {
			// this should not happen
			std::cout << "SetupDiOpenDevRegKey DIREG_DRV failed error: " << GetLastError() << std::endl;
			goto final;
		}
		// get PortName string
		DWORD portNameSize = 0;
		if (RegQueryValueEx(regKey, TEXT("PortName"), NULL, NULL, NULL, &portNameSize) == ERROR_SUCCESS) {
			std::vector<char> portName(portNameSize);
			if (RegQueryValueEx(regKey, TEXT("PortName"), NULL, NULL, (LPBYTE)portName.data(), &portNameSize) == ERROR_SUCCESS) {
				ctx->portName = portName.data();
				if (callback(context)) {
					// callback returned non-zero. stop
					break;
				}
			}
		}
		RegCloseKey(regKey);

	}
	retval = 0;
	final:
	return retval;
}

int removeCallback(PVOID context)
{
	enumPortsRemoveContext* ctx = (enumPortsRemoveContext*)context;
	if (ctx->header.portName == ctx->targetName) {
		// found the port to remove
		SP_REMOVEDEVICE_PARAMS rmdParams{ 0 };
		rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
		rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
		rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
		rmdParams.HwProfile = 0;
		if (!SetupDiSetClassInstallParams(ctx->header.hDevInfo, &ctx->header.devInfoData, &rmdParams.ClassInstallHeader, sizeof(rmdParams))) {
			std::cerr << "Failed to set class install params. Error: " << GetLastError() << std::endl;
			return 1;
		}
		if (!SetupDiCallClassInstaller(DIF_REMOVE, ctx->header.hDevInfo, &ctx->header.devInfoData)) {
			std::cerr << "Failed to call class installer. Error: " << GetLastError() << std::endl;
			return 1;
		}
		// terminate the enumeration
		return 1;
	}
	return 0;
}

int removeDevice(const std::string& comport)
{
	enumPortsRemoveContext removeContext = { 0 };
	removeContext.targetName = comport;
	int retval = enumPortDevices(removeCallback, &removeContext);
	return retval;
}

int listCallback(PVOID context)
{
	enumPortsListContext* ctx = (enumPortsListContext*)context;
	std::cout << ctx->sep << ctx->header.portName;
	ctx->sep = ", ";
	return 0;
}

int listDevices()
{
	enumPortsListContext listContext = { 0 };
	listContext.sep = "";
	int retval = enumPortDevices(listCallback, &listContext);
	std::cout << std::endl;
	return retval;
}

int installDriver(const std::string& infFile,
	bool uninstall)
{
	HKEY regKey = (HKEY) INVALID_HANDLE_VALUE;
	BOOL reboot = FALSE;

	std::string infPath = getFullPath(infFile);
	std::vector<std::string> hwidStrings;

	findHwIds(hwid, &classGuid, hwidStrings);

	for (auto it = hwidStrings.begin(); it != hwidStrings.end(); ++it) {
		std::cout << "hwid: " << *it << std::endl;
	}

	if (hwidStrings.size()) {

		if (!removeDriver(hwid, &classGuid)) {
			std::cout << "Failed to remove driver" << std::endl;
			return 1;
		}
	}
	else {
		std::cout << "no htsvsp devices found" << std::endl;
	}
	if (uninstall) {
		std::cout << "htsvsp uninstalled" << std::endl;
		return 0;
	}

	SP_DEVINFO_DATA DeviceInfoData;
	HDEVINFO DeviceInfoSet = SetupDiCreateDeviceInfoList(&classGuid, 0);
	if (DeviceInfoSet == INVALID_HANDLE_VALUE)
	{
		std::cout << "SetupDiCreateDeviceInfoList failed error: " << GetLastError() << std::endl;
		return 1;
	}

	int retval = 1;

	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	if (!SetupDiCreateDeviceInfo(DeviceInfoSet,
		className.c_str(),
		&classGuid,
		NULL,
		0,
		DICD_GENERATE_ID,
		&DeviceInfoData))
	{
		std::cout << "SetupDiCreateDeviceInfo failed error: " << GetLastError() << std::endl;
		goto final;
	}

	if (!addNewDevice(DeviceInfoSet, &DeviceInfoData)) {
		goto final;
	}

	if (!updateDriver(infPath)) {
		goto final;
	} 
	
	regKey = SetupDiOpenDevRegKey(DeviceInfoSet, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
	if (regKey == INVALID_HANDLE_VALUE) {
		std::cout << "SetupDiOpenDevRegKey DIREG_DRV failed error: " << GetLastError() << std::endl;
		goto final;
	}
	std::cout << std::endl <<  "DIREG_DRV" << std::endl;
	enumKey(regKey);
	RegCloseKey(regKey); 

	regKey = SetupDiOpenDevRegKey(DeviceInfoSet, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
	if (regKey == INVALID_HANDLE_VALUE) {
		std::cout << "SetupDiOpenDevRegKey DIREG_DEV failed error: " << GetLastError() << std::endl;
		goto final;
	}
	std::cout << "DIREG_DEV" << std::endl;
	enumKey(regKey);
	RegCloseKey(regKey);


	retval = 0;

	final:
	if (DeviceInfoSet != INVALID_HANDLE_VALUE) {
		SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	}

	return retval;
}

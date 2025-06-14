#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../VspControl/DeviceManager.h"
#include "../vspControl/Logger.h"
#include "MockSystemApi.h" // Your Google Mock for ISystemApi

Logger::Level Logger::currentLevel = Logger::INFO_LVL;
std::mutex Logger::logMutex;
Logger logger;

using namespace DeviceManager;
using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::SetErrnoAndReturn;

const char* testhwid = "HWID2";

class TestDeviceManager : public SoftwareDeviceManager {
public:
    TestDeviceManager(const char* className, GUID classGuid, const char* hwid, ISystemApi * api)
        : SoftwareDeviceManager(className, classGuid, hwid, api) {
    }
    
    int deviceTestCallback(enumContext* context)
    {
        enumDeviceContext* ctx = (enumDeviceContext*)context;
        std::string targetName = ctx->targetName;
        if (targetName == testhwid) {
            ctx->found = true;
            return 1; // Simulate successful removal
        }
        return 0;
    }
    int listTestCallback(enumContext* ctx)
    { 
        ctx->found = true;
        return 0;
    }
    CallbackFunc listCallback()
    {
        CallbackFunc f = std::bind(&TestDeviceManager::listTestCallback, this, std::placeholders::_1);
        return f;
    }
    CallbackFunc removeCallback() 
    {
        CallbackFunc f = std::bind(&TestDeviceManager::deviceTestCallback, this, std::placeholders::_1);
        return  f;
    }
    CallbackFunc enableCallback()
    {
        CallbackFunc f = std::bind(&TestDeviceManager::deviceTestCallback, this, std::placeholders::_1);
        return  f;
    }
    CallbackFunc disableCallback()
    {
        CallbackFunc f = std::bind(&TestDeviceManager::deviceTestCallback, this, std::placeholders::_1);
        return  f;
    }
};

std::vector<char> makeMultiString(const std::vector<std::string>& strings) {
    std::vector<char> buffer;
    for (const auto& s : strings) {
        buffer.insert(buffer.end(), s.begin(), s.end());
        buffer.push_back('\0');
    }
    buffer.push_back('\0'); // double NUL
    return buffer;
}

class DeviceManagerUnitTest : public ::testing::Test {
protected:

    GUID guid = { 0x12345678, 0x1234, 0x5678, {0,1,2,3,4,5,6,7} };
    NiceMock<MockSystemApi> mockApi;
    std::unique_ptr<TestDeviceManager> manager;

    HDEVINFO dummyDevInfo = (HDEVINFO)0x1234;

    // The multi-string to return
    std::vector<std::string> hwids = { "HWID1", testhwid };
    std::vector<char> multistring = makeMultiString(hwids);
    DWORD requiredSize = static_cast<DWORD>(multistring.size());

    void SetUp() override {
        manager.reset(new TestDeviceManager("TestClass", guid, testhwid, &mockApi));
    }
};

TEST_F(DeviceManagerUnitTest, AddDeviceReturnsTrue) {
    ON_CALL(mockApi, SetupDiCreateDeviceInfoList(_, _))
        .WillByDefault(Return((HDEVINFO)12345));
    ON_CALL(mockApi, SetupDiCreateDeviceInfo(_, _, _, _, _, _, _))
        .WillByDefault(Return(TRUE));
    ON_CALL(mockApi, SetupDiSetDeviceRegistryProperty(_, _, _, _, _))
        .WillByDefault(Return(TRUE));
    ON_CALL(mockApi, SetupDiCallClassInstaller(_, _, _))
        .WillByDefault(Return(TRUE));
    ON_CALL(mockApi, installDevice(_, _))
        .WillByDefault(Return(TRUE));

    EXPECT_TRUE(manager->addDevice());
}

TEST_F(DeviceManagerUnitTest, AddDeviceErrorPaths) {
    int expectedCalls = 5;

    EXPECT_CALL(mockApi, SetupDiCreateDeviceInfoList(_, _))
        .WillOnce(Return(INVALID_HANDLE_VALUE))
        .WillRepeatedly(Return((HDEVINFO)12345));

    EXPECT_CALL(mockApi, SetupDiCreateDeviceInfo(_, _, _, _, _, _, _))
        .WillOnce(Return(FALSE))
        .WillRepeatedly(Return(TRUE));

    EXPECT_CALL(mockApi, SetupDiSetDeviceRegistryProperty(_, _, _, _, _))
        .WillOnce(Return(FALSE))
        .WillRepeatedly(Return(TRUE));

    EXPECT_CALL(mockApi, SetupDiCallClassInstaller(_, _, _))
        .WillOnce(Return(FALSE))
        .WillRepeatedly(Return(TRUE));

    EXPECT_CALL(mockApi, installDevice(_, _))
        .WillOnce(Return(FALSE))
        .WillRepeatedly(Return(TRUE));

    for (int i = 0; i < expectedCalls; ++i) {
        EXPECT_FALSE(manager->addDevice());
    }
}

TEST_F(DeviceManagerUnitTest, RemoveDeviceReturnsTrue) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));
    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(TRUE))
        .WillRepeatedly(Return(FALSE));
    EXPECT_CALL(mockApi, getLastError)
        .WillOnce(Return(ERROR_INSUFFICIENT_BUFFER))
        .WillRepeatedly(Return(ERROR_SUCCESS));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(requiredSize),
            Return(FALSE)));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, requiredSize, _))
        .WillOnce(DoAll(
            SetArrayArgument<4>(multistring.begin(), multistring.end()), // fill PropertyBuffer
            Return(TRUE)
        ));

    EXPECT_CALL(mockApi, SetupDiSetClassInstallParams)
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, SetupDiCallClassInstaller)
        .WillOnce(Return(TRUE));
    EXPECT_TRUE(manager->removeDevice(testhwid));
}

TEST_F(DeviceManagerUnitTest, RemoveDeviceErrorSetupDiGetClassDevs) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillOnce(Return(INVALID_HANDLE_VALUE));
    EXPECT_FALSE(manager->removeDevice(testhwid));
}
TEST_F(DeviceManagerUnitTest, RemoveDeviceEmptySetupDiEnumDeviceInfoError) {
    HDEVINFO dummyDevInfo = (HDEVINFO)0x1234;
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));

    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillRepeatedly(Return(FALSE));

    EXPECT_FALSE(manager->removeDevice(testhwid));
}
TEST_F(DeviceManagerUnitTest, RemoveDeviceSetupDiGetDeviceRegistryPropertyWrongError) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));
    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(TRUE))
        .WillRepeatedly(Return(FALSE));

    EXPECT_CALL(mockApi, getLastError)
        .WillRepeatedly(Return(ERROR_INVALID_DATA));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(requiredSize),
            Return(FALSE)));

    EXPECT_FALSE(manager->removeDevice(testhwid));
}
TEST_F(DeviceManagerUnitTest, RemoveDeviceSetupDiGetDeviceRegistryPropertyError) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));
    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(TRUE))
        .WillRepeatedly(Return(FALSE));
    EXPECT_CALL(mockApi, getLastError)
        .WillOnce(Return(ERROR_INSUFFICIENT_BUFFER))
        .WillOnce(Return(ERROR_SUCCESS))
        .WillRepeatedly(Return(ERROR_INVALID_DATA));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(requiredSize),
            Return(FALSE))); 
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, requiredSize, _))
        .WillOnce(DoAll(
            SetArrayArgument<4>(multistring.begin(), multistring.end()), // fill PropertyBuffer
            Return(FALSE)
        ));

    EXPECT_FALSE(manager->removeDevice(testhwid));
}

TEST_F(DeviceManagerUnitTest, RemoveDeviceSetupDiSetClassInstallParamsError){
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));
    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(TRUE))
        .WillRepeatedly(Return(FALSE));
    EXPECT_CALL(mockApi, getLastError)
        .WillOnce(Return(ERROR_INSUFFICIENT_BUFFER))
        .WillRepeatedly(Return(ERROR_SUCCESS));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(requiredSize),
            Return(FALSE)));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, requiredSize, _))
        .WillOnce(DoAll(
            SetArrayArgument<4>(multistring.begin(), multistring.end()), // fill PropertyBuffer
            Return(TRUE)
        ));
    EXPECT_CALL(mockApi, SetupDiSetClassInstallParams)
        .WillOnce(Return(FALSE));


    EXPECT_FALSE(manager->removeDevice(testhwid));
}

TEST_F(DeviceManagerUnitTest, RemoveDeviceSetupDiCallClassInstallerError) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));

    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(TRUE))
        .WillRepeatedly(Return(FALSE));

    EXPECT_CALL(mockApi, getLastError)
        .WillOnce(Return(ERROR_INSUFFICIENT_BUFFER))
        .WillRepeatedly(Return(ERROR_SUCCESS));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(requiredSize),
            Return(FALSE)));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, requiredSize, _))
        .WillOnce(DoAll(
            SetArrayArgument<4>(multistring.begin(), multistring.end()), // fill PropertyBuffer
            Return(TRUE)
        ));

    EXPECT_CALL(mockApi, SetupDiSetClassInstallParams)
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, SetupDiCallClassInstaller)
        .WillOnce(Return(FALSE));

    EXPECT_FALSE(manager->removeDevice(testhwid));
}

TEST_F(DeviceManagerUnitTest, EnableDeviceReturnsTrue) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));
    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(TRUE))
        .WillRepeatedly(Return(FALSE));
    EXPECT_CALL(mockApi, getLastError)
        .WillOnce(Return(ERROR_INSUFFICIENT_BUFFER))
        .WillRepeatedly(Return(ERROR_SUCCESS));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(requiredSize),
            Return(FALSE)));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, requiredSize, _))
        .WillOnce(DoAll(
            SetArrayArgument<4>(multistring.begin(), multistring.end()), // fill PropertyBuffer
            Return(TRUE)
        ));
    EXPECT_CALL(mockApi, SetupDiSetClassInstallParams)
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, SetupDiCallClassInstaller)
        .WillOnce(Return(TRUE));

    EXPECT_TRUE(manager->enableDevice(testhwid));
}

TEST_F(DeviceManagerUnitTest, EnableDeviceErrorSetupDiGetClassDevs) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillOnce(Return(INVALID_HANDLE_VALUE));
    EXPECT_FALSE(manager->enableDevice(testhwid));
}
TEST_F(DeviceManagerUnitTest, EnableDeviceSetupDiEnumDeviceInfoError) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));

    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(FALSE));
    EXPECT_FALSE(manager->enableDevice(testhwid));
}
TEST_F(DeviceManagerUnitTest, EnableDeviceSetupDiGetDeviceRegistryPropertyError) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));

    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(TRUE))
        .WillRepeatedly(Return(FALSE));

    EXPECT_CALL(mockApi, getLastError)
        .WillOnce(Return(ERROR_INSUFFICIENT_BUFFER))
        .WillOnce(Return(ERROR_SUCCESS))
        .WillRepeatedly(Return(ERROR_INVALID_DATA));


    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(requiredSize),
            Return(FALSE)));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, requiredSize, _))
        .WillOnce(DoAll(
            SetArrayArgument<4>(multistring.begin(), multistring.end()), // fill PropertyBuffer
            Return(FALSE)
        ));
    EXPECT_FALSE(manager->enableDevice(testhwid));
}

TEST_F(DeviceManagerUnitTest, DisableDeviceReturnsTrue) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));
    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(TRUE))
        .WillRepeatedly(Return(FALSE));
    EXPECT_CALL(mockApi, getLastError)
        .WillOnce(Return(ERROR_INSUFFICIENT_BUFFER))
        .WillRepeatedly(Return(ERROR_SUCCESS));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(requiredSize),
            Return(FALSE)));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, requiredSize, _))
        .WillOnce(DoAll(
            SetArrayArgument<4>(multistring.begin(), multistring.end()), // fill PropertyBuffer
            Return(TRUE)
        ));
    EXPECT_CALL(mockApi, SetupDiSetClassInstallParams)
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, SetupDiCallClassInstaller)
        .WillOnce(Return(TRUE));

    EXPECT_TRUE(manager->disableDevice(testhwid));
}

TEST_F(DeviceManagerUnitTest, ListDevicesReturnsTrue) {
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillRepeatedly(Return(dummyDevInfo));

    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(Return(TRUE))
        .WillRepeatedly(Return(FALSE));
    EXPECT_CALL(mockApi, getLastError)
        .WillOnce(Return(ERROR_INSUFFICIENT_BUFFER))
        .WillRepeatedly(Return(ERROR_SUCCESS));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(requiredSize),
            Return(FALSE)));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, requiredSize, _))
        .WillOnce(DoAll(
            SetArrayArgument<4>(multistring.begin(), multistring.end()), // fill PropertyBuffer
            Return(TRUE)
        ));
    EXPECT_TRUE(manager->listDevices());
}

TEST_F(DeviceManagerUnitTest, installDriverReturnsTrue) {
    EXPECT_CALL(mockApi, getFullPath)
        .WillOnce(Return("test.inf"));
    EXPECT_CALL(mockApi, SetupDiCreateDeviceInfoList(_, _))
        .WillOnce(Return((HDEVINFO)12345));
    EXPECT_CALL(mockApi, SetupDiCreateDeviceInfo(_, _, _, _, _, _, _))
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, SetupDiSetDeviceRegistryProperty(_, _, _, _, _))
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, SetupDiCallClassInstaller(_, _, _))
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, updateDriver)
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, SetupDiOpenDevRegKey)
        .WillRepeatedly(Return((HKEY)12345));
    EXPECT_CALL(mockApi, RegEnumValue)
        .WillRepeatedly(Return(ERROR_NO_MORE_ITEMS));

    EXPECT_TRUE(manager->installDriver("test.inf", false));
}
TEST_F(DeviceManagerUnitTest, UninstallDriverReturnsTrue) {
    SP_DEVINFO_DATA dummyDevInfoData = {};
    dummyDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    std::string infPath = "oem1.inf";
    DWORD infSize = static_cast<DWORD>(infPath.size() + 1);
    EXPECT_CALL(mockApi, SetupDiGetClassDevs(_, _))
        .WillOnce(Return(dummyDevInfo));
    EXPECT_CALL(mockApi, SetupDiEnumDeviceInfo(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(dummyDevInfoData), Return(TRUE)))
        .WillRepeatedly(Return(FALSE));
    EXPECT_CALL(mockApi, getLastError)
        .WillOnce(Return(ERROR_INSUFFICIENT_BUFFER))
        .WillRepeatedly(Return(ERROR_SUCCESS));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, 0, _))
        .WillOnce(DoAll(SetArgPointee<6>(requiredSize), Return(FALSE)));
    EXPECT_CALL(mockApi, SetupDiGetDeviceRegistryProperty(_, _, SPDRP_HARDWAREID, _, _, requiredSize, _))
        .WillOnce(DoAll(SetArrayArgument<4>(multistring.begin(), multistring.end()), Return(TRUE)));
    EXPECT_CALL(mockApi, SetupDiOpenDevRegKey(_, _, _, _, _, _))
        .WillOnce(Return((HKEY)12345));
    EXPECT_CALL(mockApi, RegQueryValueEx((HKEY)12345, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(infSize), Return(ERROR_SUCCESS)))
        .WillOnce(Return(ERROR_SUCCESS));
    EXPECT_CALL(mockApi, SetupDiSetClassInstallParams(_, _, _, _))
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, SetupDiCallClassInstaller(DIF_REMOVE, _, _))
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mockApi, SetupUninstallOEMInf(_, SUOI_FORCEDELETE))
        .WillOnce(Return(TRUE));
    EXPECT_TRUE(manager->uninstallDriver("test.inf"));
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

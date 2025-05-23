#include <windows.h>
#include <string>
#include <iostream>
#include <filesystem>
#include "service.hpp"
#include "data.hpp"

#undef UNICODE
#undef _UNICODE

bool ExistOtherService(SC_HANDLE service_manager) {
    DWORD spaceNeeded = 0;
    DWORD numServices = 0;
    if (!EnumServicesStatusA(service_manager, SERVICE_DRIVER, SERVICE_STATE_ALL, NULL, 0, &spaceNeeded, &numServices, 0) && GetLastError() != ERROR_MORE_DATA) {
        return true;
    }
    spaceNeeded += sizeof(ENUM_SERVICE_STATUSA);
    LPENUM_SERVICE_STATUSA buffer = (LPENUM_SERVICE_STATUSA)new BYTE[spaceNeeded];

    if (EnumServicesStatusA(service_manager, SERVICE_DRIVER, SERVICE_STATE_ALL, buffer, spaceNeeded, &spaceNeeded, &numServices, 0)) {
        for (DWORD i = 0; i < numServices; i++) {
            ENUM_SERVICE_STATUSA service = buffer[i];
            SC_HANDLE service_handle = OpenServiceA(service_manager, service.lpServiceName, SERVICE_QUERY_CONFIG);
            if (service_handle) {
                LPQUERY_SERVICE_CONFIGA config = (LPQUERY_SERVICE_CONFIGA)new BYTE[8096]; //8096 = max size of QUERY_SERVICE_CONFIGA
                DWORD needed = 0;
                if (QueryServiceConfigA(service_handle, config, 8096, &needed)) {
                    if (strstr(config->lpBinaryPathName, intel_driver::driver_name)) {
                        delete[] buffer;
                        CloseServiceHandle(service_handle);
                        return false;
                    }
                }
                CloseServiceHandle(service_handle);
            }
        }
        delete[] buffer;
        return false; //no equal services we can continue
    }
    delete[] buffer;
    return true;
}

bool ExistsValorantService(SC_HANDLE service_manager) {
    DWORD spaceNeeded = 0;
    DWORD numServices = 0;
    if (!EnumServicesStatusA(service_manager, SERVICE_DRIVER, SERVICE_STATE_ALL, NULL, 0, &spaceNeeded, &numServices, 0) && GetLastError() != ERROR_MORE_DATA) {
        return true;
    }
    spaceNeeded += sizeof(ENUM_SERVICE_STATUSA);
    LPENUM_SERVICE_STATUSA buffer = (LPENUM_SERVICE_STATUSA)new BYTE[spaceNeeded];

    if (EnumServicesStatusA(service_manager, SERVICE_DRIVER, SERVICE_STATE_ALL, buffer, spaceNeeded, &spaceNeeded, &numServices, 0)) {
        for (DWORD i = 0; i < numServices; i++) {
            ENUM_SERVICE_STATUSA service = buffer[i];
            if (strstr(service.lpServiceName, XorString("vgk"))) {
                if ((service.ServiceStatus.dwCurrentState == SERVICE_RUNNING || service.ServiceStatus.dwCurrentState == SERVICE_START_PENDING)) {
                    delete[] buffer;
                    return true;
                }
            }
        }
        delete[] buffer;
        return false; //no valorant service found
    }
    delete[] buffer;
    return true;
}

bool service::RegisterAndStart(const std::string& driver_path)
{
    const std::string driver_name = std::filesystem::path(driver_path).filename().string();
    const SC_HANDLE sc_manager_handle = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);

    if (!sc_manager_handle) {
        return false;
    }
    if (ExistOtherService(sc_manager_handle)) {
        return false;
    }

    if (ExistsValorantService(sc_manager_handle)) {
        return false;
    }

    SC_HANDLE service_handle = CreateServiceA(
        sc_manager_handle,
        driver_name.c_str(),
        driver_name.c_str(),
        SERVICE_START | SERVICE_STOP | DELETE,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        driver_path.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (!service_handle) {
        service_handle = OpenServiceA(sc_manager_handle, driver_name.c_str(), SERVICE_START);
        if (!service_handle) {
            CloseServiceHandle(sc_manager_handle);
            return false;
        }
    }

    const bool result = StartServiceA(service_handle, 0, nullptr);

    CloseServiceHandle(service_handle);
    CloseServiceHandle(sc_manager_handle);
    return result;
}

bool service::StopAndRemove(const std::string& driver_name)
{
    const SC_HANDLE sc_manager_handle = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);

    if (!sc_manager_handle)
        return false;

    const SC_HANDLE service_handle = OpenServiceA(sc_manager_handle, driver_name.c_str(), SERVICE_STOP | DELETE);

    if (!service_handle)
    {
        CloseServiceHandle(sc_manager_handle);
        return false;
    }

    SERVICE_STATUS status = { 0 };
    const bool result = ControlService(service_handle, SERVICE_CONTROL_STOP, &status) && DeleteService(service_handle);

    CloseServiceHandle(service_handle);
    CloseServiceHandle(sc_manager_handle);

    return result;
}

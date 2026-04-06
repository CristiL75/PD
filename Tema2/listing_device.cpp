#define UNICODE
#define _UNICODE

#include <windows.h>
#include <setupapi.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")

struct DeviceInfoSetHandle {
    HDEVINFO handle = INVALID_HANDLE_VALUE;

    ~DeviceInfoSetHandle() {
        if (handle != INVALID_HANDLE_VALUE) {
            SetupDiDestroyDeviceInfoList(handle);
        }
    }
};

struct DevicePropertyDescriptor {
    DWORD property;
    const wchar_t* name;
};

std::wstring Win32ErrorToString(DWORD errorCode) {
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = FormatMessageW(flags, nullptr, errorCode, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    std::wstring message;
    if (size > 0 && buffer != nullptr) {
        message.assign(buffer, size);
        LocalFree(buffer);
    } else {
        message = L"Eroare necunoscuta";
    }

    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
        message.pop_back();
    }

    return message;
}

std::wstring BytesToHex(const BYTE* data, DWORD size) {
    std::wstringstream ss;
    ss << std::hex << std::setfill(L'0');
    for (DWORD i = 0; i < size; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(data[i]);
        if (i + 1 < size) {
            ss << L' ';
        }
    }
    return ss.str();
}

std::wstring ReadWideString(const BYTE* data, DWORD sizeInBytes) {
    if (data == nullptr || sizeInBytes == 0) {
        return L"";
    }

    const wchar_t* asWide = reinterpret_cast<const wchar_t*>(data);
    const size_t charCount = sizeInBytes / sizeof(wchar_t);

    size_t len = 0;
    while (len < charCount && asWide[len] != L'\0') {
        ++len;
    }

    return std::wstring(asWide, len);
}

std::wstring ReadMultiWideString(const BYTE* data, DWORD sizeInBytes) {
    if (data == nullptr || sizeInBytes == 0) {
        return L"";
    }

    const wchar_t* asWide = reinterpret_cast<const wchar_t*>(data);
    const size_t charCount = sizeInBytes / sizeof(wchar_t);
    std::wstringstream ss;

    size_t i = 0;
    bool first = true;
    while (i < charCount) {
        if (asWide[i] == L'\0') {
            break;
        }

        const size_t start = i;
        while (i < charCount && asWide[i] != L'\0') {
            ++i;
        }

        if (!first) {
            ss << L" | ";
        }
        first = false;
        ss << std::wstring(asWide + start, i - start);

        if (i < charCount && asWide[i] == L'\0') {
            ++i;
        }
    }

    return ss.str();
}

std::wstring FormatRegistryData(DWORD type, const BYTE* data, DWORD size) {
    switch (type) {
    case REG_SZ:
    case REG_EXPAND_SZ:
        return ReadWideString(data, size);
    case REG_MULTI_SZ:
        return ReadMultiWideString(data, size);
    case REG_DWORD:
        if (size >= sizeof(DWORD)) {
            DWORD value = 0;
            std::memcpy(&value, data, sizeof(DWORD));
            std::wstringstream ss;
            ss << value << L" (0x" << std::hex << std::uppercase << value << L")";
            return ss.str();
        }
        return L"<dimensiune invalida pentru REG_DWORD>";
    case REG_QWORD:
        if (size >= sizeof(ULONGLONG)) {
            ULONGLONG value = 0;
            std::memcpy(&value, data, sizeof(ULONGLONG));
            std::wstringstream ss;
            ss << value << L" (0x" << std::hex << std::uppercase << value << L")";
            return ss.str();
        }
        return L"<dimensiune invalida pentru REG_QWORD>";
    case REG_BINARY:
        return BytesToHex(data, size);
    case REG_NONE:
    default:
        return L"<date brute> " + BytesToHex(data, size);
    }
}

bool TryParseIndex(const std::wstring& text, int& outIndex) {
    if (text.empty()) {
        return false;
    }

    wchar_t* end = nullptr;
    const long value = std::wcstol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != L'\0' || value < 0) {
        return false;
    }

    outIndex = static_cast<int>(value);
    return true;
}

std::wstring GetDeviceInstanceId(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& deviceInfoData) {
    DWORD requiredSize = 0;
    SetupDiGetDeviceInstanceIdW(deviceInfoSet, &deviceInfoData, nullptr, 0, &requiredSize);

    if (requiredSize == 0) {
        return L"<necunoscut>";
    }

    std::vector<wchar_t> buffer(requiredSize + 1, L'\0');
    if (!SetupDiGetDeviceInstanceIdW(deviceInfoSet, &deviceInfoData, buffer.data(), static_cast<DWORD>(buffer.size()), &requiredSize)) {
        return L"<eroare: " + Win32ErrorToString(GetLastError()) + L">";
    }

    return buffer.data();
}

bool QueryDeviceProperty(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& deviceInfoData, DWORD property, std::wstring& outValue) {
    DWORD dataType = 0;
    DWORD requiredSize = 0;
    std::vector<BYTE> buffer(256);

    while (true) {
        const BOOL ok = SetupDiGetDeviceRegistryPropertyW(
            deviceInfoSet,
            &deviceInfoData,
            property,
            &dataType,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &requiredSize
        );

        if (ok) {
            outValue = FormatRegistryData(dataType, buffer.data(), requiredSize);
            return true;
        }

        const DWORD error = GetLastError();
        if (error == ERROR_INSUFFICIENT_BUFFER) {
            buffer.resize(requiredSize);
            continue;
        }

        if (error == ERROR_INVALID_DATA || error == ERROR_NOT_FOUND) {
            return false;
        }

        outValue = L"<eroare: " + Win32ErrorToString(error) + L">";
        return true;
    }
}

void PrintDevice(HDEVINFO deviceInfoSet, DWORD index, SP_DEVINFO_DATA& deviceInfoData) {
    static const DevicePropertyDescriptor kProperties[] = {
        {SPDRP_DEVICEDESC, L"DeviceDesc"},
        {SPDRP_FRIENDLYNAME, L"FriendlyName"},
        {SPDRP_MFG, L"Manufacturer"},
        {SPDRP_CLASS, L"Class"},
        {SPDRP_CLASSGUID, L"ClassGuid"},
        {SPDRP_DRIVER, L"Driver"},
        {SPDRP_SERVICE, L"Service"},
        {SPDRP_ENUMERATOR_NAME, L"EnumeratorName"},
        {SPDRP_LOCATION_INFORMATION, L"LocationInformation"},
        {SPDRP_LOCATION_PATHS, L"LocationPaths"},
        {SPDRP_HARDWAREID, L"HardwareId"},
        {SPDRP_COMPATIBLEIDS, L"CompatibleIds"},
        {SPDRP_UPPERFILTERS, L"UpperFilters"},
        {SPDRP_LOWERFILTERS, L"LowerFilters"},
        {SPDRP_CAPABILITIES, L"Capabilities"},
        {SPDRP_ADDRESS, L"Address"},
        {SPDRP_BUSNUMBER, L"BusNumber"},
        {SPDRP_REMOVAL_POLICY, L"RemovalPolicy"},
        {SPDRP_REMOVAL_POLICY_HW_DEFAULT, L"RemovalPolicyHwDefault"},
        {SPDRP_INSTALL_STATE, L"InstallState"},
        {SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, L"PhysicalDeviceObjectName"}
    };

    std::wcout << L"Dispozitiv #" << index << L"\n";
    std::wcout << L"InstanceId: " << GetDeviceInstanceId(deviceInfoSet, deviceInfoData) << L"\n";

    for (const auto& descriptor : kProperties) {
        std::wstring value;
        if (QueryDeviceProperty(deviceInfoSet, deviceInfoData, descriptor.property, value)) {
            std::wcout << L"- " << descriptor.name << L": " << value << L"\n";
        }
    }

    std::wcout << L"----------------------------------------\n";
}

int wmain(int argc, wchar_t* argv[]) {
    int selectedIndex = -1;
    if (argc >= 2) {
        const std::wstring argument = argv[1];
        if (!argument.empty() && argument != L"all" && argument != L"ALL") {
            if (!TryParseIndex(argument, selectedIndex)) {
                std::wcerr << L"Argument invalid. Foloseste un index numeric sau 'all'.\n";
                return 1;
            }
        }
    }

    DeviceInfoSetHandle deviceInfoSet;
    deviceInfoSet.handle = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);

    if (deviceInfoSet.handle == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Nu s-a putut deschide lista de device-uri: " << Win32ErrorToString(GetLastError()) << L"\n";
        return 2;
    }

    DWORD totalDevices = 0;
    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA deviceInfoData = {};
        deviceInfoData.cbSize = sizeof(deviceInfoData);

        if (!SetupDiEnumDeviceInfo(deviceInfoSet.handle, index, &deviceInfoData)) {
            const DWORD error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) {
                break;
            }

            std::wcerr << L"Eroare la enumerarea device-urilor: " << Win32ErrorToString(error) << L"\n";
            return 3;
        }

        ++totalDevices;
        if (selectedIndex < 0 || static_cast<DWORD>(selectedIndex) == index) {
            PrintDevice(deviceInfoSet.handle, index, deviceInfoData);
        }
    }

    if (totalDevices == 0) {
        std::wcout << L"Nu au fost gasite device-uri prezente in sistem.\n";
        return 0;
    }

    if (selectedIndex >= 0 && static_cast<DWORD>(selectedIndex) >= totalDevices) {
        std::wcerr << L"Index inexistent. Sunt disponibile " << totalDevices << L" device-uri prezente.\n";
        return 4;
    }

    if (selectedIndex < 0) {
        std::wcout << L"Total device-uri prezente: " << totalDevices << L"\n";
        std::wcout << L"Pentru un singur device, ruleaza programul cu un index numeric.\n";
    }

    return 0;
}

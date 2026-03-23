#define UNICODE
#define _UNICODE

#include <windows.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct ParsedRegistryPath {
    HKEY root = nullptr;
    std::wstring subKey;
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

bool StartsWithInsensitive(const std::wstring& text, const std::wstring& prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }

    for (size_t i = 0; i < prefix.size(); ++i) {
        wchar_t a = text[i];
        wchar_t b = prefix[i];
        if (a >= L'a' && a <= L'z') {
            a = static_cast<wchar_t>(a - (L'a' - L'A'));
        }
        if (b >= L'a' && b <= L'z') {
            b = static_cast<wchar_t>(b - (L'a' - L'A'));
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

bool ParseRegistryPath(const std::wstring& fullPath, ParsedRegistryPath& outPath) {
    const std::pair<std::wstring, HKEY> roots[] = {
        {L"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT},
        {L"HKCR", HKEY_CLASSES_ROOT},
        {L"HKEY_CURRENT_USER", HKEY_CURRENT_USER},
        {L"HKCU", HKEY_CURRENT_USER},
        {L"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
        {L"HKLM", HKEY_LOCAL_MACHINE},
        {L"HKEY_USERS", HKEY_USERS},
        {L"HKU", HKEY_USERS},
        {L"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG},
        {L"HKCC", HKEY_CURRENT_CONFIG}
    };

    for (const auto& entry : roots) {
        const std::wstring& rootName = entry.first;
        if (StartsWithInsensitive(fullPath, rootName)) {
            if (fullPath.size() == rootName.size()) {
                outPath.root = entry.second;
                outPath.subKey.clear();
                return true;
            }

            if (fullPath[rootName.size()] == L'\\') {
                outPath.root = entry.second;
                outPath.subKey = fullPath.substr(rootName.size() + 1);
                return true;
            }
        }
    }

    return false;
}

const wchar_t* RegistryTypeToString(DWORD type) {
    switch (type) {
    case REG_NONE: return L"REG_NONE";
    case REG_SZ: return L"REG_SZ";
    case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";
    case REG_BINARY: return L"REG_BINARY";
    case REG_DWORD: return L"REG_DWORD";
    case REG_DWORD_BIG_ENDIAN: return L"REG_DWORD_BIG_ENDIAN";
    case REG_LINK: return L"REG_LINK";
    case REG_MULTI_SZ: return L"REG_MULTI_SZ";
    case REG_RESOURCE_LIST: return L"REG_RESOURCE_LIST";
    case REG_FULL_RESOURCE_DESCRIPTOR: return L"REG_FULL_RESOURCE_DESCRIPTOR";
    case REG_RESOURCE_REQUIREMENTS_LIST: return L"REG_RESOURCE_REQUIREMENTS_LIST";
    case REG_QWORD: return L"REG_QWORD";
    default: return L"REG_UNKNOWN";
    }
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

std::wstring ReadRegSz(const BYTE* data, DWORD sizeInBytes) {
    if (data == nullptr || sizeInBytes == 0) {
        return L"";
    }

    const wchar_t* asWide = reinterpret_cast<const wchar_t*>(data);
    const size_t charCount = sizeInBytes / sizeof(wchar_t);

    size_t safeLen = 0;
    while (safeLen < charCount && asWide[safeLen] != L'\0') {
        ++safeLen;
    }

    return std::wstring(asWide, safeLen);
}

std::wstring ReadRegMultiSz(const BYTE* data, DWORD sizeInBytes) {
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

        size_t start = i;
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

std::wstring FormatRegistryValue(DWORD type, const BYTE* data, DWORD size) {
    switch (type) {
    case REG_SZ:
    case REG_EXPAND_SZ:
        return ReadRegSz(data, size);

    case REG_MULTI_SZ:
        return ReadRegMultiSz(data, size);

    case REG_DWORD:
        if (size >= sizeof(DWORD)) {
            DWORD value = *reinterpret_cast<const DWORD*>(data);
            std::wstringstream ss;
            ss << value << L" (0x" << std::hex << std::uppercase << value << L")";
            return ss.str();
        }
        return L"<dimensiune invalida pentru REG_DWORD>";

    case REG_QWORD:
        if (size >= sizeof(ULONGLONG)) {
            ULONGLONG value = *reinterpret_cast<const ULONGLONG*>(data);
            std::wstringstream ss;
            ss << value << L" (0x" << std::hex << std::uppercase << value << L")";
            return ss.str();
        }
        return L"<dimensiune invalida pentru REG_QWORD>";

    case REG_BINARY:
        return BytesToHex(data, size);

    default:
        return L"<tip necunoscut - afisare in hex> " + BytesToHex(data, size);
    }
}

int wmain(int argc, wchar_t* argv[]) {
    // Exemplu cerut: daca nu se primeste argument, folosim subcheia de test.
    const std::wstring defaultPath = L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const std::wstring targetPath = (argc >= 2) ? argv[1] : defaultPath;

    ParsedRegistryPath parsed;
    if (!ParseRegistryPath(targetPath, parsed)) {
        std::wcerr << L"Cale Registry invalida: " << targetPath << L"\n";
        std::wcerr << L"Exemplu valid: HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\n";
        return 1;
    }

    HKEY hKey = nullptr;
    const LONG openResult = RegOpenKeyExW(parsed.root, parsed.subKey.empty() ? nullptr : parsed.subKey.c_str(), 0, KEY_READ, &hKey);

    if (openResult != ERROR_SUCCESS) {
        std::wcerr << L"Nu s-a putut deschide cheia: " << targetPath << L"\n";
        if (openResult == ERROR_FILE_NOT_FOUND) {
            std::wcerr << L"Motiv: cheia nu exista.\n";
        } else if (openResult == ERROR_ACCESS_DENIED) {
            std::wcerr << L"Motiv: acces interzis.\n";
        } else {
            std::wcerr << L"Motiv: " << Win32ErrorToString(static_cast<DWORD>(openResult)) << L"\n";
        }
        return 2;
    }

    DWORD valueCount = 0;
    DWORD maxValueNameLen = 0;
    DWORD maxValueDataLen = 0;

    const LONG infoResult = RegQueryInfoKeyW(
        hKey,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &valueCount,
        &maxValueNameLen,
        &maxValueDataLen,
        nullptr,
        nullptr
    );

    if (infoResult != ERROR_SUCCESS) {
        std::wcerr << L"RegQueryInfoKey a esuat: " << Win32ErrorToString(static_cast<DWORD>(infoResult)) << L"\n";
        RegCloseKey(hKey);
        return 3;
    }

    std::wcout << L"Cheie: " << targetPath << L"\n";
    std::wcout << L"Numar valori: " << valueCount << L"\n\n";

    std::vector<wchar_t> nameBuffer(maxValueNameLen + 1);
    std::vector<BYTE> dataBuffer(maxValueDataLen);

    for (DWORD index = 0; index < valueCount; ++index) {
        DWORD nameLen = static_cast<DWORD>(nameBuffer.size());
        DWORD dataLen = static_cast<DWORD>(dataBuffer.size());
        DWORD type = 0;

        LONG enumResult = RegEnumValueW(
            hKey,
            index,
            nameBuffer.data(),
            &nameLen,
            nullptr,
            &type,
            dataBuffer.data(),
            &dataLen
        );

        if (enumResult == ERROR_MORE_DATA) {
            // Reincercam cu buffer extins in cazul in care valorile au crescut intre timp.
            dataBuffer.resize(dataLen);
            nameBuffer.resize(nameLen + 1);
            nameLen = static_cast<DWORD>(nameBuffer.size());
            enumResult = RegEnumValueW(
                hKey,
                index,
                nameBuffer.data(),
                &nameLen,
                nullptr,
                &type,
                dataBuffer.data(),
                &dataLen
            );
        }

        if (enumResult != ERROR_SUCCESS) {
            std::wcerr << L"[Index " << index << L"] Eroare la enumerare: "
                       << Win32ErrorToString(static_cast<DWORD>(enumResult)) << L"\n";
            continue;
        }

        const std::wstring valueName = (nameLen == 0) ? L"(Default)" : std::wstring(nameBuffer.data(), nameLen);
        const std::wstring formattedValue = FormatRegistryValue(type, dataBuffer.data(), dataLen);

        std::wcout << L"Valoare: " << valueName << L"\n";
        std::wcout << L"Tip:     " << RegistryTypeToString(type) << L" (" << type << L")\n";
        std::wcout << L"Date:    " << formattedValue << L"\n";
        std::wcout << L"----------------------------------------\n";
    }

    RegCloseKey(hKey);
    return 0;
}

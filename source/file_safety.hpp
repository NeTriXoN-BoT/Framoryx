#pragma once
#include "platform.hpp"
#include <bcrypt.h>
#include <filesystem>
#include <optional>
#include <atomic>
#include <mutex>

namespace fg {
// One transaction spans root-config reads, backups and include updates for every audio effect.
inline std::recursive_mutex audioConfigMutex;
inline bool localFilePath(const std::wstring& input) {
    std::wstring_view path(input);
    if (path.starts_with(L"\\\\?\\"))
        path.remove_prefix(4);
    if (path.size() < 3 ||
        !((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) ||
        path[1] != L':' || (path[2] != L'\\' && path[2] != L'/'))
        return false;
    for (size_t i = 0; i < path.size(); ++i)
        if (path[i] < 32 || (path[i] == L':' && i != 1) || path[i] == L'"' || path[i] == L'|' ||
            path[i] == L'<' || path[i] == L'>' || path[i] == L'*' || path[i] == L'?')
            return false;
    wchar_t root[]{path[0], L':', L'\\', 0};
    auto type = GetDriveTypeW(root);
    return type == DRIVE_FIXED || type == DRIVE_REMOVABLE || type == DRIVE_RAMDISK;
}
inline std::optional<std::vector<unsigned char>> readRegularFile(const std::filesystem::path& path,
                                                                 size_t limit) {
    if (!localFilePath(path.wstring()))
        return {};
    Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION information{};
    LARGE_INTEGER length{};
    if (!file || GetFileType(file.h) != FILE_TYPE_DISK ||
        !GetFileInformationByHandle(file.h, &information) ||
        (information.dwFileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
        !GetFileSizeEx(file.h, &length) || length.QuadPart < 0 || uint64_t(length.QuadPart) > limit)
        return {};
    std::vector<unsigned char> bytes(size_t(length.QuadPart));
    size_t offset = 0;
    while (offset < bytes.size()) {
        DWORD count = 0, chunk = DWORD(std::min<size_t>(bytes.size() - offset, 1024 * 1024));
        if (!ReadFile(file.h, bytes.data() + offset, chunk, &count, nullptr) || !count)
            return {};
        offset += count;
    }
    return bytes;
}
inline std::string sha256Bytes(const std::vector<unsigned char>& bytes) {
    unsigned char digest[32]{};
    if (bytes.size() > ULONG_MAX ||
        BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, const_cast<PUCHAR>(bytes.data()),
                   ULONG(bytes.size()), digest, 32) < 0)
        return {};
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (auto c : digest) {
        result += hex[c >> 4];
        result += hex[c & 15];
    }
    return result;
}
inline bool atomicFileWrite(const std::filesystem::path& destination, const std::string& bytes) {
    if (!localFilePath(destination.wstring()))
        return false;
    static std::atomic<unsigned> serial{0};
    std::filesystem::path temporary;
    Handle file;
    for (int attempt = 0; attempt < 4; ++attempt) {
        temporary = destination;
        temporary += L".framoryx-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                     std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(++serial) + L".tmp";
        file.reset(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
        if (file)
            break;
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
            return false;
    }
    if (!file)
        return false;
    bool ok = true;
    size_t offset = 0;
    while (offset < bytes.size() && ok) {
        DWORD count = 0, chunk = DWORD(std::min<size_t>(bytes.size() - offset, 1024 * 1024));
        ok = WriteFile(file.h, bytes.data() + offset, chunk, &count, nullptr) && count;
        offset += count;
    }
    if (ok)
        ok = FlushFileBuffers(file.h) != FALSE;
    file.reset();
    if (ok)
        ok = MoveFileExW(temporary.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok)
        DeleteFileW(temporary.c_str()); // Only the new temporary file created above.
    return ok;
}
inline bool plainDirectory(const std::filesystem::path& path) {
    auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(path.c_str(), nullptr))
            return false;
        attributes = GetFileAttributesW(path.c_str());
    }
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
           !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}
inline std::optional<std::filesystem::path> stageAudioPlugin(const std::wstring& directory,
                                                             const std::wstring& source,
                                                             const std::string& expectedDigest) {
    if (!localFilePath(directory) || !localFilePath(source))
        return {};
    auto bytes = readRegularFile(source, 32u * 1024u * 1024u);
    if (!bytes || bytes->empty())
        return {};
    auto digest = sha256Bytes(*bytes);
    if (digest.empty() || (!expectedDigest.empty() && digest != expectedDigest))
        return {};
    auto plugins = std::filesystem::path(directory) / L"FramoryxPlugins";
    auto version = plugins / wide(digest);
    if (!plainDirectory(plugins) || !plainDirectory(version))
        return {};
    auto target = version / std::filesystem::path(source).filename();
    if (GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES) {
        auto existing = readRegularFile(target, 32u * 1024u * 1024u);
        if (!existing || sha256Bytes(*existing) != digest)
            return {};
    } else if (!atomicFileWrite(target, std::string(reinterpret_cast<const char*>(bytes->data()),
                                                    bytes->size())))
        return {};
    return target;
}
} // namespace fg

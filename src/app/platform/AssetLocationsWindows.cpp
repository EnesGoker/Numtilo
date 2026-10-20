#include "app/AssetLocations.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <limits>
#include <string>
#include <vector>

namespace app {

NativeAssetLocations discoverAssetLocations() {
    std::vector<wchar_t> buffer(256);
    for (;;) {
        SetLastError(ERROR_SUCCESS);
        const DWORD copied =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0) {
            const DWORD error = GetLastError();
            return {std::nullopt, std::nullopt,
                    error == ERROR_SUCCESS
                        ? std::make_error_code(std::errc::io_error)
                        : std::error_code(static_cast<int>(error), std::system_category())};
        }
        if (copied < buffer.size()) {
            const std::filesystem::path executable(std::wstring(buffer.data(), copied));
            if (executable.empty() || !executable.is_absolute()) {
                return {std::nullopt, std::nullopt,
                        std::make_error_code(std::errc::invalid_argument)};
            }
            return {executable.parent_path(), std::nullopt, {}};
        }
        // A return equal to capacity means truncation, including on older Windows
        // versions where GetLastError may not report ERROR_INSUFFICIENT_BUFFER.
        if (buffer.size() > std::numeric_limits<DWORD>::max() / 2U) {
            return {std::nullopt, std::nullopt, std::make_error_code(std::errc::filename_too_long)};
        }
        buffer.resize(buffer.size() * 2U);
    }
}

} // namespace app

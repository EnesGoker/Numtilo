#include "app/AssetLocations.hpp"

#include <unistd.h>

#include <cerrno>
#include <limits>
#include <string>
#include <vector>

namespace app {

NativeAssetLocations discoverAssetLocations() {
    std::vector<char> buffer(256);
    for (;;) {
        const auto copied = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (copied < 0) {
            if (errno == EINTR) {
                continue;
            }
            return {std::nullopt, std::nullopt, {errno, std::generic_category()}};
        }
        if (static_cast<std::size_t>(copied) < buffer.size()) {
            // readlink does not append a terminator; use exactly the returned bytes.
            const std::filesystem::path executable(
                std::string(buffer.data(), static_cast<std::size_t>(copied)));
            if (executable.empty() || !executable.is_absolute()) {
                return {std::nullopt, std::nullopt,
                        std::make_error_code(std::errc::invalid_argument)};
            }
            return {executable.parent_path(), std::nullopt, {}};
        }
        if (buffer.size() > static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()) / 2U) {
            return {std::nullopt, std::nullopt, std::make_error_code(std::errc::filename_too_long)};
        }
        buffer.resize(buffer.size() * 2U);
    }
}

} // namespace app

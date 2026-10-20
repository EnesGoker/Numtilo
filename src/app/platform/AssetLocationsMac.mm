#include "app/AssetLocations.hpp"

#import <Foundation/Foundation.h>

#include <mach-o/dyld.h>

#include <cstdint>
#include <vector>

namespace app {
namespace {

NativeAssetLocations executableLocations() {
    std::uint32_t required = 0;
    static_cast<void>(_NSGetExecutablePath(nullptr, &required));
    if (required == 0) {
        return {std::nullopt, std::nullopt, std::make_error_code(std::errc::io_error)};
    }
    std::vector<char> buffer(required);
    if (_NSGetExecutablePath(buffer.data(), &required) != 0) {
        return {std::nullopt, std::nullopt, std::make_error_code(std::errc::io_error)};
    }
    const std::filesystem::path executable(buffer.data());
    if (executable.empty() || !executable.is_absolute()) {
        return {std::nullopt, std::nullopt, std::make_error_code(std::errc::invalid_argument)};
    }
    // dyld may return a symlink. Resolve only its absolute executable path, without
    // deriving a location from the working directory or searching relative assets.
    std::error_code error;
    const auto realExecutable = std::filesystem::canonical(executable, error);
    if (error) {
        return {std::nullopt, std::nullopt, error};
    }
    return {realExecutable.parent_path(), std::nullopt, {}};
}

} // namespace

NativeAssetLocations discoverAssetLocations() {
    @autoreleasepool {
        NSBundle *bundle = [NSBundle mainBundle];
        NSString *extension = [[bundle bundlePath] pathExtension];
        // Command-line programs can also get an NSBundle representing their parent
        // directory. Only an application bundle selects bundle resources.
        if (extension != nil && [extension caseInsensitiveCompare:@"app"] == NSOrderedSame) {
            NSURL *resources = [bundle resourceURL];
            const char *nativePath = resources == nil || ![resources isFileURL]
                                         ? nullptr
                                         : resources.fileSystemRepresentation;
            if (nativePath == nullptr || *nativePath == '\0') {
                // This bundle remains authoritative; discovery must not fall back
                // to executable-relative resources after selecting it.
                return {std::nullopt, std::filesystem::path{},
                        std::make_error_code(std::errc::io_error)};
            }
            return {std::nullopt, std::filesystem::path(nativePath), {}};
        }
        return executableLocations();
    }
}

} // namespace app

#include "app/UserDataPaths.hpp"

#import <Foundation/Foundation.h>

namespace app {

NativeUserDataResult discoverUserDataBase() {
    @autoreleasepool {
        NSError *error = nil;
        NSURL *url = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
                                                            inDomain:NSUserDomainMask
                                                   appropriateForURL:nil
                                                              create:NO
                                                               error:&error];
        if (url == nil) {
            if (error != nil && [error.domain isEqualToString:NSPOSIXErrorDomain]) {
                return {std::nullopt, {static_cast<int>(error.code), std::generic_category()}};
            }
            return {std::nullopt, std::make_error_code(std::errc::io_error)};
        }
        const char *nativePath = url.fileSystemRepresentation;
        if (nativePath == nullptr || *nativePath == '\0') {
            return {std::nullopt, std::make_error_code(std::errc::invalid_argument)};
        }
        return {std::filesystem::path(nativePath), {}};
    }
}

} // namespace app

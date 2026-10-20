#include "app/UserDataPaths.hpp"

#include <cstdlib>

namespace app {
namespace {

std::optional<std::filesystem::path> environmentPath(const char *name) {
    const char *value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::filesystem::path(value);
}

} // namespace

NativeUserDataResult discoverUserDataBase() {
    return detail::selectXdgDataBase(environmentPath("XDG_DATA_HOME"), environmentPath("HOME"));
}

} // namespace app

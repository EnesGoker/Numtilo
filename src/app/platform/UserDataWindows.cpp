#include "app/UserDataPaths.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

namespace app {
namespace {

std::error_code hresultError(HRESULT result) {
    if (HRESULT_FACILITY(result) == FACILITY_WIN32) {
        return {static_cast<int>(HRESULT_CODE(result)), std::system_category()};
    }
    return std::make_error_code(std::errc::io_error);
}

class ComScope {
  public:
    ComScope() : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {
    }
    ~ComScope() {
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }
    HRESULT result() const noexcept {
        return result_;
    }

  private:
    HRESULT result_;
};

struct KnownFolderPath {
    PWSTR value{nullptr};
    ~KnownFolderPath() {
        CoTaskMemFree(value);
    }
};

} // namespace

NativeUserDataResult discoverUserDataBase() {
    const ComScope com;
    if (FAILED(com.result()) && com.result() != RPC_E_CHANGED_MODE) {
        return {std::nullopt, hresultError(com.result())};
    }
    KnownFolderPath path;
    const auto result =
        SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DONT_VERIFY, nullptr, &path.value);
    if (FAILED(result)) {
        return {std::nullopt, hresultError(result)};
    }
    if (path.value == nullptr || *path.value == L'\0') {
        return {std::nullopt, std::make_error_code(std::errc::invalid_argument)};
    }
    return {std::filesystem::path(path.value), {}};
}

} // namespace app

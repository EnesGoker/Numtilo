#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

namespace numtilo_core::detail {

// This seam exposes write stages so failure tests can exercise the real storage pipeline.
class StorageOutputFile {
  public:
    virtual ~StorageOutputFile() = default;
    virtual bool write(std::string_view bytes) = 0;
    virtual bool flush() = 0;
    virtual bool close() = 0;
};

class StorageFileOperations {
  public:
    virtual ~StorageFileOperations() = default;
    virtual std::unique_ptr<StorageOutputFile>
    createExclusive(const std::filesystem::path &path) = 0;
    virtual bool publish(const std::filesystem::path &from, const std::filesystem::path &to,
                         bool replace) = 0;
    virtual void remove(const std::filesystem::path &path) noexcept = 0;
};

std::shared_ptr<StorageFileOperations> makeNativeScoreFileOperations();

} // namespace numtilo_core::detail

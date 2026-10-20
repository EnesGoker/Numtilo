#include "core/FileStorage.hpp"

#include "core/detail/FileSnapshot.hpp"
#include "core/detail/ScoreFileOperations.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace numtilo_core {
namespace {

using detail::FileReadResult;
using detail::FileReadStatus;

class NativeFileHandle {
  public:
#if defined(_WIN32)
    using Value = HANDLE;
    static inline const Value kInvalid = INVALID_HANDLE_VALUE;
#else
    using Value = int;
    static constexpr Value kInvalid = -1;
#endif

    NativeFileHandle() = default;
    explicit NativeFileHandle(const Value value) : value_(value) {
    }
    NativeFileHandle(const NativeFileHandle &) = delete;
    NativeFileHandle &operator=(const NativeFileHandle &) = delete;

    ~NativeFileHandle() {
        close();
    }

    Value get() const noexcept {
        return value_;
    }

    void adopt(const Value value) noexcept {
        value_ = value;
    }

    bool close() noexcept {
        if (value_ == kInvalid) {
            return closeSucceeded_;
        }
        const Value value = std::exchange(value_, kInvalid);
#if defined(_WIN32)
        closeSucceeded_ = CloseHandle(value) != 0;
#else
        // A failed close must not be retried: the descriptor may already be released.
        closeSucceeded_ = ::close(value) == 0;
#endif
        return closeSucceeded_;
    }

  private:
    Value value_{kInvalid};
    bool closeSucceeded_{true};
};

FileReadStatus classifyMissingPath(const std::filesystem::path &path) {
    // Win32 may report file/child as ERROR_PATH_NOT_FOUND. Confirm a usable ancestor
    // before treating it as absence rather than an I/O obstruction.
    auto parent = path.parent_path();
    while (!parent.empty()) {
        std::error_code error;
        auto status = std::filesystem::symlink_status(parent, error);
        if (status.type() == std::filesystem::file_type::not_found &&
            (!error || error == std::errc::no_such_file_or_directory)) {
            const auto next = parent.parent_path();
            if (next == parent) {
                return FileReadStatus::IoError;
            }
            parent = next;
            continue;
        }
        if (error) {
            return FileReadStatus::IoError;
        }
        if (std::filesystem::is_symlink(status)) {
            status = std::filesystem::status(parent, error);
        }
        return !error && std::filesystem::is_directory(status) ? FileReadStatus::Missing
                                                               : FileReadStatus::IoError;
    }
    return FileReadStatus::Missing;
}

// Open the file itself rather than following a symlink/reparse point. The handle is then
// checked as well, so a path changed between the status check and open is not followed.
FileReadResult readFileSnapshotImpl(const std::filesystem::path &path) {
    FileReadResult result;
    if (path.empty() || path.native().find(std::filesystem::path::value_type{}) !=
                            std::filesystem::path::string_type::npos) {
        result.status = FileReadStatus::Invalid;
        return result;
    }
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (status.type() == std::filesystem::file_type::not_found &&
        (!ec || ec == std::errc::no_such_file_or_directory)) {
        result.status = classifyMissingPath(path);
        return result;
    }
    if (ec) {
        return result;
    }
    if (!std::filesystem::is_regular_file(status)) {
        result.status = FileReadStatus::Invalid;
        return result;
    }

    std::array<char, 64 * 1024> buffer{};
#if defined(_WIN32)
    const HANDLE handle = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            result.status = classifyMissingPath(path);
        }
        return result;
    }
    NativeFileHandle owner(handle);
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(handle, &information)) {
        return result;
    }
    if (GetFileType(handle) != FILE_TYPE_DISK ||
        (information.dwFileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        result.status = FileReadStatus::Invalid;
        return result;
    }
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) {
            return result;
        }
        if (count == 0) {
            break;
        }
        result.bytes.append(buffer.data(), count);
    }
    if (!owner.close()) {
        return result;
    }
#else
    // A regular path could be replaced by a FIFO before open. Nonblocking open
    // avoids waiting for a writer; fstat below still rejects every non-regular handle.
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) {
        if (errno == ENOENT) {
            result.status = classifyMissingPath(path);
        } else if (errno == ELOOP) {
            result.status = FileReadStatus::Invalid;
        }
        return result;
    }
    NativeFileHandle owner(descriptor);
    struct stat information {};
    if (::fstat(descriptor, &information) != 0) {
        return result;
    }
    if (!S_ISREG(information.st_mode)) {
        result.status = FileReadStatus::Invalid;
        return result;
    }
    for (;;) {
        const auto count = ::read(descriptor, buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return result;
        }
        if (count == 0) {
            break;
        }
        result.bytes.append(buffer.data(), static_cast<std::size_t>(count));
    }
    if (!owner.close()) {
        return result;
    }
#endif
    result.status = FileReadStatus::Read;
    return result;
}

FileSaveStatus checkSource(const std::filesystem::path &path,
                           const std::optional<std::string> &expected) {
    const auto contents = detail::readFileSnapshot(path);
    if (contents.status == FileReadStatus::IoError) {
        return FileSaveStatus::IoError;
    }
    if ((!expected && contents.status == FileReadStatus::Missing) ||
        (expected && contents.status == FileReadStatus::Read && contents.bytes == *expected)) {
        return FileSaveStatus::Saved;
    }
    return FileSaveStatus::SourceChanged;
}

class NativeOutputFile final : public detail::StorageOutputFile {
  public:
    bool openExclusive(const std::filesystem::path &path) {
#if defined(_WIN32)
        handle_.adopt(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
#else
        handle_.adopt(::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600));
#endif
        return handle_.get() != NativeFileHandle::kInvalid;
    }

    bool write(const std::string_view bytes) override {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
#if defined(_WIN32)
            if (handle_.get() == INVALID_HANDLE_VALUE) {
                return false;
            }
            const auto chunk = static_cast<DWORD>(
                std::min<std::size_t>(bytes.size() - offset, std::numeric_limits<DWORD>::max()));
            DWORD count = 0;
            if (!WriteFile(handle_.get(), bytes.data() + offset, chunk, &count, nullptr) ||
                count == 0) {
                return false;
            }
#else
            if (handle_.get() < 0) {
                return false;
            }
            const auto chunk = std::min<std::size_t>(
                bytes.size() - offset,
                static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
            const auto count = ::write(handle_.get(), bytes.data() + offset, chunk);
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                return false;
            }
#endif
            offset += static_cast<std::size_t>(count);
        }
        return true;
    }

    bool flush() override {
#if defined(_WIN32)
        return handle_.get() != INVALID_HANDLE_VALUE && FlushFileBuffers(handle_.get()) != 0;
#else
        if (handle_.get() < 0) {
            return false;
        }
        int status = 0;
        do {
            status = ::fsync(handle_.get());
        } while (status != 0 && errno == EINTR);
        return status == 0;
#endif
    }

    bool close() override {
        return handle_.close();
    }

  private:
    NativeFileHandle handle_;
};

class NativeFileOperations final : public detail::StorageFileOperations {
  public:
    std::unique_ptr<detail::StorageOutputFile>
    createExclusive(const std::filesystem::path &path) override {
        // Allocate the owner before opening, so an allocation failure cannot leak a temp.
        auto output = std::make_unique<NativeOutputFile>();
        if (!output->openExclusive(path)) {
            return nullptr;
        }
        return output;
    }

    bool publish(const std::filesystem::path &from, const std::filesystem::path &to,
                 const bool replace) override {
#if defined(_WIN32)
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
        return MoveFileExW(from.c_str(), to.c_str(), flags) != 0;
#else
        if (replace) {
            return ::rename(from.c_str(), to.c_str()) == 0;
        }
        if (::link(from.c_str(), to.c_str()) != 0) {
            return false;
        }
        // Publishing succeeded; a failed unlink is left for the owning temp's cleanup.
        ::unlink(from.c_str());
        return true;
#endif
    }

    void remove(const std::filesystem::path &path) noexcept override {
#if defined(_WIN32)
        DeleteFileW(path.c_str());
#else
        ::unlink(path.c_str());
#endif
    }
};

std::string temporaryNonce() {
    static const std::string nonce = [] {
        std::random_device random;
        std::ostringstream out;
        out << std::hex;
        for (int word = 0; word < 4; ++word) {
            out << random() << '-';
        }
        return out.str();
    }();
    return nonce;
}

class TemporaryFile {
  public:
    explicit TemporaryFile(std::shared_ptr<detail::StorageFileOperations> operations)
        : operations_(std::move(operations)) {
    }

    ~TemporaryFile() {
        output_.reset();
        if (owned_) {
            operations_->remove(path_);
        }
    }

    bool create(const std::filesystem::path &destination) {
        static std::atomic<std::uint64_t> nextId{0};
        // A collision never becomes ownership. Each retry chooses a different path.
        for (int attempt = 0; attempt < 32; ++attempt) {
            path_ = destination;
            path_ += ".tmp-" + temporaryNonce() + std::to_string(nextId.fetch_add(1));
            output_ = operations_->createExclusive(path_);
            if (output_) {
                owned_ = true;
                return true;
            }
        }
        return false;
    }

    bool prepare(const std::string_view bytes) {
        return output_ && output_->write(bytes) && output_->flush() && output_->close();
    }

    bool publish(const std::filesystem::path &destination, const bool replace) {
        return operations_->publish(path_, destination, replace);
    }

  private:
    std::shared_ptr<detail::StorageFileOperations> operations_;
    std::filesystem::path path_;
    std::unique_ptr<detail::StorageOutputFile> output_;
    bool owned_{false};
};

FileSaveStatus checkBackup(const std::filesystem::path &path, const std::string &expectedBytes) {
    const auto contents = detail::readFileSnapshot(path);
    if (contents.status == FileReadStatus::Missing) {
        return FileSaveStatus::NotSaved;
    }
    if (contents.status == FileReadStatus::IoError) {
        return FileSaveStatus::IoError;
    }
    return contents.status == FileReadStatus::Read && contents.bytes == expectedBytes
               ? FileSaveStatus::Saved
               : FileSaveStatus::BackupConflict;
}

class AtomicFileStorage final : public FileStorage {
  public:
    explicit AtomicFileStorage(std::shared_ptr<detail::StorageFileOperations> operations)
        : operations_(std::move(operations)) {
    }

    FileSaveStatus save(const std::filesystem::path &path, const std::string &serialized,
                        const std::optional<std::string> &expectedSource,
                        const bool backupLegacy) override {
        try {
            return saveSnapshot(path, serialized, expectedSource, backupLegacy);
        } catch (const std::runtime_error &) {
            // Native dependency failures (including entropy initialization) must
            // retain the old file and leave the same memory snapshot retryable.
            return FileSaveStatus::IoError;
        }
    }

  private:
    FileSaveStatus saveSnapshot(const std::filesystem::path &path, const std::string &serialized,
                                const std::optional<std::string> &expectedSource,
                                const bool backupLegacy) {
        if (!operations_) {
            return FileSaveStatus::IoError;
        }
        if (backupLegacy && !expectedSource) {
            return FileSaveStatus::SourceChanged;
        }
        const auto sourceStatus = checkSource(path, expectedSource);
        if (sourceStatus != FileSaveStatus::Saved) {
            return sourceStatus;
        }
        const auto parent = path.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                return FileSaveStatus::IoError;
            }
        }

        TemporaryFile replacement(operations_);
        if (!replacement.create(path) || !replacement.prepare(serialized)) {
            return FileSaveStatus::IoError;
        }

        std::filesystem::path backupPath = path;
        backupPath += ".bak";
        if (backupLegacy) {
            auto backupStatus = checkBackup(backupPath, *expectedSource);
            if (backupStatus == FileSaveStatus::NotSaved) {
                TemporaryFile backup(operations_);
                if (!backup.create(backupPath) || !backup.prepare(*expectedSource)) {
                    return FileSaveStatus::IoError;
                }
                if (!backup.publish(backupPath, false)) {
                    // A concurrent publication can only be reused if it is the same backup.
                    backupStatus = checkBackup(backupPath, *expectedSource);
                    if (backupStatus == FileSaveStatus::NotSaved) {
                        return FileSaveStatus::IoError;
                    }
                } else {
                    backupStatus = FileSaveStatus::Saved;
                }
            }
            if (backupStatus != FileSaveStatus::Saved) {
                return backupStatus;
            }
            // Preserve an already valid backup on every failed attempt and retry.
            const auto verifiedBackup = checkBackup(backupPath, *expectedSource);
            if (verifiedBackup != FileSaveStatus::Saved) {
                return verifiedBackup == FileSaveStatus::NotSaved ? FileSaveStatus::IoError
                                                                  : verifiedBackup;
            }
        }

        const auto finalSourceStatus = checkSource(path, expectedSource);
        if (finalSourceStatus != FileSaveStatus::Saved) {
            return finalSourceStatus;
        }
        // No remove-before-rename, and no fallible durability step after this commit.
        if (!replacement.publish(path, expectedSource.has_value())) {
            return FileSaveStatus::IoError;
        }
        return FileSaveStatus::Saved;
    }

    std::shared_ptr<detail::StorageFileOperations> operations_;
};

} // namespace

detail::FileReadResult detail::readFileSnapshot(const std::filesystem::path &path) {
    return readFileSnapshotImpl(path);
}

std::shared_ptr<detail::StorageFileOperations> detail::makeNativeScoreFileOperations() {
    return std::make_shared<NativeFileOperations>();
}

std::shared_ptr<FileStorage> makeFileStorage() {
    return makeFileStorage(detail::makeNativeScoreFileOperations());
}

std::shared_ptr<FileStorage>
makeFileStorage(std::shared_ptr<detail::StorageFileOperations> operations) {
    return std::make_shared<AtomicFileStorage>(std::move(operations));
}

} // namespace numtilo_core

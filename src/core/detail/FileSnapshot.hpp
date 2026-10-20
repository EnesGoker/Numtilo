#pragma once

#include <filesystem>
#include <string>

namespace numtilo_core::detail {

enum class FileReadStatus { Missing, Read, Invalid, IoError };

struct FileReadResult {
    FileReadStatus status{FileReadStatus::IoError};
    std::string bytes;
};

// Reads one immutable byte snapshot from a regular file, rejecting symlinks and
// reparse points both before open and on the opened handle. No files are modified.
FileReadResult readFileSnapshot(const std::filesystem::path &path);

} // namespace numtilo_core::detail

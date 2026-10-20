#pragma once

#include "core/FileStorage.hpp"

namespace numtilo_core {

// Preserve the score persistence API while sharing its model-independent backend.
using ScoreSaveStatus = FileSaveStatus;
using ScoreStorage = FileStorage;

std::shared_ptr<ScoreStorage> makeScoreStorage();
std::shared_ptr<ScoreStorage>
makeScoreStorage(std::shared_ptr<detail::StorageFileOperations> operations);

} // namespace numtilo_core

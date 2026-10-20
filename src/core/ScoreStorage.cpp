#include "core/ScoreStorage.hpp"

#include <utility>

namespace numtilo_core {

std::shared_ptr<ScoreStorage> makeScoreStorage() {
    return makeFileStorage();
}

std::shared_ptr<ScoreStorage>
makeScoreStorage(std::shared_ptr<detail::StorageFileOperations> operations) {
    return makeFileStorage(std::move(operations));
}

} // namespace numtilo_core

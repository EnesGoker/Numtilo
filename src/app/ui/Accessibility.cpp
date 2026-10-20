#include "app/ui/Accessibility.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace app::ui {

namespace {
bool validUtf8(const std::string &value) {
    for (std::size_t i = 0; i < value.size();) {
        const auto first = static_cast<unsigned char>(value[i++]);
        if (first < 0x80U)
            continue;
        unsigned int scalar = 0;
        unsigned int minimum = 0;
        unsigned int remaining = 0;
        if (first >= 0xc2U && first <= 0xdfU) {
            scalar = first & 0x1fU;
            minimum = 0x80U;
            remaining = 1;
        } else if (first >= 0xe0U && first <= 0xefU) {
            scalar = first & 0x0fU;
            minimum = 0x800U;
            remaining = 2;
        } else if (first >= 0xf0U && first <= 0xf4U) {
            scalar = first & 0x07U;
            minimum = 0x10000U;
            remaining = 3;
        } else
            return false;
        if (remaining > value.size() - i)
            return false;
        while (remaining-- != 0U) {
            const auto next = static_cast<unsigned char>(value[i++]);
            if ((next & 0xc0U) != 0x80U)
                return false;
            scalar = (scalar << 6U) | (next & 0x3fU);
        }
        if (scalar < minimum || scalar > 0x10ffffU || (scalar >= 0xd800U && scalar <= 0xdfffU))
            return false;
    }
    return true;
}
} // namespace

bool isValidAccessibilitySnapshot(const AccessibilitySnapshot &snapshot) {
    if (snapshot.clientPixelWidth == 0U || snapshot.clientPixelHeight == 0U ||
        snapshot.nodes.size() > 512U || !validUtf8(snapshot.roundId) ||
        !validUtf8(snapshot.sceneName) || !validUtf8(snapshot.announcement)) {
        return false;
    }
    std::unordered_map<std::string, const AccessibilityNode *> nodes;
    for (const auto &node : snapshot.nodes) {
        if (node.id.empty() || !validUtf8(node.id) || !validUtf8(node.parentId) ||
            !validUtf8(node.name) || !validUtf8(node.value) || !validUtf8(node.help) ||
            !validUtf8(node.error) || !nodes.emplace(node.id, &node).second ||
            !std::isfinite(node.frame.x) || !std::isfinite(node.frame.y) ||
            !std::isfinite(node.frame.width) || !std::isfinite(node.frame.height) ||
            node.frame.width < 0.f || node.frame.height < 0.f) {
            return false;
        }
        for (const auto &custom : node.customActions)
            if (!validUtf8(custom.name))
                return false;
    }
    for (const auto &node : snapshot.nodes) {
        std::unordered_set<std::string> ancestors{node.id};
        auto parent = node.parentId;
        while (!parent.empty()) {
            const auto found = nodes.find(parent);
            if (found == nodes.end() || !ancestors.insert(parent).second) {
                return false;
            }
            parent = found->second->parentId;
        }
    }
    return true;
}

bool acceptsAccessibilityAction(const AccessibilitySnapshot &snapshot,
                                const AccessibilityAction &action) {
    if (action.generation != snapshot.generation || action.roundId != snapshot.roundId ||
        action.value.size() > 4096U || !validUtf8(action.value)) {
        return false;
    }
    const auto found = std::find_if(snapshot.nodes.begin(), snapshot.nodes.end(),
                                    [&](const auto &node) { return node.id == action.targetId; });
    if (found == snapshot.nodes.end() || !found->enabled) {
        return false;
    }
    if (action.kind == AccessibilityActionKind::Focus) {
        return found->focusable;
    }
    if (action.kind == AccessibilityActionKind::SetValue) {
        const bool text = found->role == AccessibilityRole::TextField && found->editable;
        const bool toggle = found->role == AccessibilityRole::Toggle &&
                            (action.value == "true" || action.value == "false");
        if (!text && !toggle)
            return false;
    }
    return std::find(found->actions.begin(), found->actions.end(), action.kind) !=
               found->actions.end() ||
           std::any_of(found->customActions.begin(), found->customActions.end(),
                       [&](const auto &custom) { return custom.kind == action.kind; });
}

namespace {
struct ActionQueue {
    std::mutex mutex;
    bool attached{false};
    AccessibilitySnapshot snapshot;
    std::deque<AccessibilityAction> actions;
};
} // namespace

struct AccessibilityBridge::Impl {
    std::shared_ptr<ActionQueue> queue = std::make_shared<ActionQueue>();
    std::unique_ptr<detail::AccessibilityPlatform> platform;

    Impl() {
        const std::weak_ptr<ActionQueue> weakQueue = queue;
        platform = detail::makeAccessibilityPlatform([weakQueue](AccessibilityAction action) {
            const auto state = weakQueue.lock();
            if (!state) {
                return false;
            }
            const std::lock_guard lock(state->mutex);
            if (!state->attached || state->actions.size() >= 64U ||
                !acceptsAccessibilityAction(state->snapshot, action)) {
                return false;
            }
            state->actions.push_back(std::move(action));
            return true;
        });
    }
};

AccessibilityBridge::AccessibilityBridge() : impl_(std::make_unique<Impl>()) {
}
AccessibilityBridge::~AccessibilityBridge() {
    detach();
}

bool AccessibilityBridge::supported() const {
    return impl_->platform->supported();
}

bool AccessibilityBridge::attach(void *nativeWindow) {
    detach();
    if (nativeWindow == nullptr || !impl_->platform->attach(nativeWindow)) {
        return false;
    }
    const std::lock_guard lock(impl_->queue->mutex);
    impl_->queue->attached = true;
    return true;
}

void AccessibilityBridge::detach() {
    {
        const std::lock_guard lock(impl_->queue->mutex);
        impl_->queue->attached = false;
        impl_->queue->actions.clear();
        impl_->queue->snapshot = {};
    }
    impl_->platform->detach();
}

bool AccessibilityBridge::attached() const {
    const std::lock_guard lock(impl_->queue->mutex);
    return impl_->queue->attached;
}

bool AccessibilityBridge::publish(const AccessibilitySnapshot &snapshot) {
    if (!isValidAccessibilitySnapshot(snapshot)) {
        return false;
    }
    bool changed = false;
    {
        const std::lock_guard lock(impl_->queue->mutex);
        if (!impl_->queue->attached) {
            return false;
        }
        changed = impl_->queue->snapshot != snapshot;
        if (changed) {
            impl_->queue->snapshot = snapshot;
            std::erase_if(impl_->queue->actions, [&](const auto &action) {
                return !acceptsAccessibilityAction(snapshot, action);
            });
        }
    }
    // AppKit can synchronously query/enqueue while reconnecting first-responder
    // ownership, so native work runs after committing state and releasing the lock.
    impl_->platform->refreshFocus();
    if (changed)
        impl_->platform->publish(snapshot);
    return true;
}

std::vector<AccessibilityAction> AccessibilityBridge::takeActions() {
    const std::lock_guard lock(impl_->queue->mutex);
    std::vector<AccessibilityAction> result;
    result.reserve(impl_->queue->actions.size());
    while (!impl_->queue->actions.empty()) {
        auto action = std::move(impl_->queue->actions.front());
        impl_->queue->actions.pop_front();
        if (acceptsAccessibilityAction(impl_->queue->snapshot, action)) {
            result.push_back(std::move(action));
        }
    }
    return result;
}

} // namespace app::ui

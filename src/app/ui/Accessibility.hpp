#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace app::ui {

enum class AccessibilityRole { Group, StaticText, Button, TextField, Toggle, Table, Row, Cell };
enum class AccessibilityActionKind {
    Press,
    Focus,
    SetValue,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown
};

struct AccessibilityFrame {
    float x{0.f}, y{0.f}, width{0.f}, height{0.f};
    bool operator==(const AccessibilityFrame &) const = default;
};

struct AccessibilityCustomAction {
    std::string name;
    AccessibilityActionKind kind{AccessibilityActionKind::Press};
    bool operator==(const AccessibilityCustomAction &) const = default;
};

struct AccessibilityNode {
    std::string id;
    std::string parentId;
    AccessibilityRole role{AccessibilityRole::StaticText};
    std::string name;
    std::string value;
    std::string help;
    std::string error;
    AccessibilityFrame frame;
    bool enabled{true};
    bool focused{false};
    bool focusable{false};
    bool editable{false};
    bool required{false};
    bool checked{false};
    int row{-1};
    int column{-1};
    int rowCount{0};
    int columnCount{0};
    std::vector<AccessibilityActionKind> actions;
    std::vector<AccessibilityCustomAction> customActions;
    bool operator==(const AccessibilityNode &) const = default;
};

struct AccessibilitySnapshot {
    std::uint64_t generation{0};
    std::string roundId;
    std::string sceneName;
    std::string announcement;
    unsigned int clientPixelWidth{0};
    unsigned int clientPixelHeight{0};
    bool modal{false};
    // Frames are SFML client pixels (getSize/mapCoordsToPixel), with an upper-left origin.
    std::vector<AccessibilityNode> nodes;
    bool operator==(const AccessibilitySnapshot &) const = default;
};

struct AccessibilityAction {
    std::string targetId;
    AccessibilityActionKind kind{AccessibilityActionKind::Press};
    std::string value;
    std::uint64_t generation{0};
    std::string roundId;
};

bool isValidAccessibilitySnapshot(const AccessibilitySnapshot &snapshot);
bool acceptsAccessibilityAction(const AccessibilitySnapshot &snapshot,
                                const AccessibilityAction &action);

// Native callbacks only enqueue tagged requests. App drains and revalidates them
// against its current scene/round before applying existing scene commands.
class AccessibilityBridge {
  public:
    AccessibilityBridge();
    ~AccessibilityBridge();
    AccessibilityBridge(const AccessibilityBridge &) = delete;
    AccessibilityBridge &operator=(const AccessibilityBridge &) = delete;

    bool supported() const;
    bool attach(void *nativeWindow);
    void detach();
    bool attached() const;
    bool publish(const AccessibilitySnapshot &snapshot);
    std::vector<AccessibilityAction> takeActions();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

namespace detail {

class AccessibilityPlatform {
  public:
    virtual ~AccessibilityPlatform() = default;
    virtual bool supported() const = 0;
    virtual bool attach(void *nativeWindow) = 0;
    virtual void detach() = 0;
    // Native responder ownership can change without changing scene semantics.
    virtual void refreshFocus() {
    }
    virtual void publish(const AccessibilitySnapshot &snapshot) = 0;
};

using AccessibilityEnqueue = std::function<bool(AccessibilityAction)>;
std::unique_ptr<AccessibilityPlatform> makeAccessibilityPlatform(AccessibilityEnqueue enqueue);

} // namespace detail
} // namespace app::ui

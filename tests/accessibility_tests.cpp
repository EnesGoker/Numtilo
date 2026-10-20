#include "app/ui/Accessibility.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <limits>
#include <utility>

namespace {

using namespace app::ui;

struct PlatformProbe {
    detail::AccessibilityEnqueue enqueue;
    bool acceptsAttach{true};
    bool attached{false};
    bool ownsNativeFocus{false};
    int focusRefreshes{0};
    int publications{0};
    int destructions{0};
    AccessibilitySnapshot lastSnapshot;
    std::function<void()> duringPublish;
    std::function<void()> duringDetach;
    std::function<void()> duringRefresh;
};

std::shared_ptr<PlatformProbe> latestPlatform;

class FakePlatform final : public detail::AccessibilityPlatform {
  public:
    explicit FakePlatform(std::shared_ptr<PlatformProbe> probe) : probe_(std::move(probe)) {
    }

    ~FakePlatform() override {
        ++probe_->destructions;
    }

    bool supported() const override {
        return true;
    }

    bool attach(void *handle) override {
        probe_->attached = handle != nullptr && probe_->acceptsAttach;
        return probe_->attached;
    }

    void detach() override {
        probe_->attached = false;
        probe_->ownsNativeFocus = false;
        if (probe_->duringDetach)
            probe_->duringDetach();
    }

    void refreshFocus() override {
        ++probe_->focusRefreshes;
        if (probe_->attached)
            probe_->ownsNativeFocus = true;
        if (probe_->duringRefresh)
            probe_->duringRefresh();
    }

    void publish(const AccessibilitySnapshot &snapshot) override {
        ++probe_->publications;
        probe_->lastSnapshot = snapshot;
        if (probe_->duringPublish)
            probe_->duringPublish();
    }

  private:
    std::shared_ptr<PlatformProbe> probe_;
};

AccessibilitySnapshot snapshot() {
    AccessibilitySnapshot result;
    result.generation = 7;
    result.roundId = "round-seven";
    result.sceneName = "Oyuncu Girişi";
    result.clientPixelWidth = 920;
    result.clientPixelHeight = 660;

    AccessibilityNode group;
    group.id = "group";
    group.role = AccessibilityRole::Group;
    group.frame = {32.f, 32.f, 856.f, 596.f};
    result.nodes.push_back(group);

    AccessibilityNode name;
    name.id = "name";
    name.parentId = "group";
    name.role = AccessibilityRole::TextField;
    name.name = "Oyuncu Adı";
    name.value = "Çağrı𐐷";
    name.frame = {280.f, 220.f, 360.f, 54.f};
    name.editable = name.focusable = true;
    name.actions = {AccessibilityActionKind::Focus, AccessibilityActionKind::SetValue};
    result.nodes.push_back(name);

    AccessibilityNode sound;
    sound.id = "sound";
    sound.parentId = "group";
    sound.role = AccessibilityRole::Toggle;
    sound.focusable = true;
    sound.actions = {AccessibilityActionKind::Press, AccessibilityActionKind::Focus,
                     AccessibilityActionKind::SetValue};
    result.nodes.push_back(sound);

    AccessibilityNode board;
    board.id = "board";
    board.parentId = "group";
    board.role = AccessibilityRole::Table;
    board.focusable = true;
    board.actions = {AccessibilityActionKind::Focus};
    board.customActions = {{"Sola oyna", AccessibilityActionKind::MoveLeft}};
    result.nodes.push_back(board);
    return result;
}

AccessibilityAction request(const AccessibilitySnapshot &current, std::string target = "sound",
                            AccessibilityActionKind kind = AccessibilityActionKind::Press,
                            std::string value = {}) {
    return {std::move(target), kind, std::move(value), current.generation, current.roundId};
}

} // namespace

// This target links Accessibility.cpp with this fake factory, never an OS adapter.
namespace app::ui::detail {

std::unique_ptr<AccessibilityPlatform> makeAccessibilityPlatform(AccessibilityEnqueue enqueue) {
    latestPlatform = std::make_shared<PlatformProbe>();
    latestPlatform->enqueue = std::move(enqueue);
    return std::make_unique<FakePlatform>(latestPlatform);
}

} // namespace app::ui::detail

TEST_CASE("Accessibility rejects malformed trees and frame metadata",
          "[accessibility][validation]") {
    const auto valid = snapshot();
    REQUIRE(isValidAccessibilitySnapshot(valid));
    auto invalid = valid;

    SECTION("identifiers must be nonempty and unique") {
        invalid.nodes[1].id.clear();
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
        invalid = valid;
        invalid.nodes.push_back(invalid.nodes.back());
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
    }
    SECTION("parents must exist without cycles") {
        invalid.nodes[1].parentId = "missing";
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
        invalid = valid;
        invalid.nodes[0].parentId = "name";
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
        invalid = valid;
        invalid.nodes[0].parentId = "group";
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
    }
    SECTION("client dimensions and every frame component are bounded") {
        invalid.clientPixelWidth = 0;
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
        invalid = valid;
        invalid.clientPixelHeight = 0;
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
        for (float AccessibilityFrame::*component :
             {&AccessibilityFrame::x, &AccessibilityFrame::y, &AccessibilityFrame::width,
              &AccessibilityFrame::height}) {
            invalid = valid;
            invalid.nodes[0].frame.*component = std::numeric_limits<float>::quiet_NaN();
            REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
            invalid.nodes[0].frame.*component = std::numeric_limits<float>::infinity();
            REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
        }
        invalid = valid;
        invalid.nodes[0].frame.width = -1.f;
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
        invalid = valid;
        invalid.nodes[0].frame.height = -1.f;
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
    }
    SECTION("the native tree has a hard node limit") {
        invalid.nodes.resize(1);
        for (int index = 1; index < 512; ++index) {
            AccessibilityNode node;
            node.id = "node-" + std::to_string(index);
            node.parentId = "group";
            invalid.nodes.push_back(std::move(node));
        }
        REQUIRE(isValidAccessibilitySnapshot(invalid));
        auto extra = invalid.nodes.back();
        extra.id = "node-512";
        invalid.nodes.push_back(std::move(extra));
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
    }
}

TEST_CASE("Accessibility preserves Unicode scalars and rejects malformed UTF-8",
          "[accessibility][validation]") {
    const auto valid = snapshot();
    REQUIRE(isValidAccessibilitySnapshot(valid));
    REQUIRE(acceptsAccessibilityAction(
        valid, request(valid, "name", AccessibilityActionKind::SetValue, "İpek𐐷")));
    REQUIRE(acceptsAccessibilityAction(
        valid, request(valid, "name", AccessibilityActionKind::SetValue, "")));

    for (const auto &bytes :
         {std::string("\x80"), std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
          std::string("\xf4\x90\x80\x80"), std::string("\xf5\x80\x80\x80"), std::string("\xe2\x82"),
          std::string("\xe2\x28\xa1")}) {
        REQUIRE_FALSE(acceptsAccessibilityAction(
            valid, request(valid, "name", AccessibilityActionKind::SetValue, bytes)));
        for (std::string AccessibilitySnapshot::*field :
             {&AccessibilitySnapshot::roundId, &AccessibilitySnapshot::sceneName,
              &AccessibilitySnapshot::announcement}) {
            auto invalid = valid;
            invalid.*field = bytes;
            REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
        }
        for (std::string AccessibilityNode::*field :
             {&AccessibilityNode::id, &AccessibilityNode::name, &AccessibilityNode::value,
              &AccessibilityNode::help, &AccessibilityNode::error}) {
            auto invalid = valid;
            invalid.nodes[1].*field = bytes;
            REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
        }
        auto invalid = valid;
        invalid.nodes[3].customActions[0].name = bytes;
        REQUIRE_FALSE(isValidAccessibilitySnapshot(invalid));
    }
}

TEST_CASE("Accessibility accepts only current enabled declared actions",
          "[accessibility][routing]") {
    auto current = snapshot();
    REQUIRE(acceptsAccessibilityAction(current, request(current)));
    REQUIRE(acceptsAccessibilityAction(current,
                                       request(current, "name", AccessibilityActionKind::Focus)));
    REQUIRE(acceptsAccessibilityAction(
        current, request(current, "board", AccessibilityActionKind::MoveLeft)));
    for (const auto &value : {"true", "false"})
        REQUIRE(acceptsAccessibilityAction(
            current, request(current, "sound", AccessibilityActionKind::SetValue, value)));
    REQUIRE_FALSE(acceptsAccessibilityAction(
        current, request(current, "sound", AccessibilityActionKind::SetValue, "1")));
    REQUIRE_FALSE(acceptsAccessibilityAction(current, request(current, "unknown")));
    REQUIRE_FALSE(acceptsAccessibilityAction(current, request(current, "name")));
    REQUIRE_FALSE(acceptsAccessibilityAction(
        current, request(current, "sound", AccessibilityActionKind::MoveLeft)));
    REQUIRE_FALSE(acceptsAccessibilityAction(
        current, request(current, "group", AccessibilityActionKind::Focus)));

    auto stale = request(current);
    --stale.generation;
    REQUIRE_FALSE(acceptsAccessibilityAction(current, stale));
    stale = request(current);
    stale.roundId = "previous-round";
    REQUIRE_FALSE(acceptsAccessibilityAction(current, stale));
    REQUIRE(acceptsAccessibilityAction(
        current,
        request(current, "name", AccessibilityActionKind::SetValue, std::string(4096, 'a'))));
    REQUIRE_FALSE(acceptsAccessibilityAction(
        current,
        request(current, "name", AccessibilityActionKind::SetValue, std::string(4097, 'a'))));

    current.nodes[2].actions = {AccessibilityActionKind::Press};
    REQUIRE_FALSE(acceptsAccessibilityAction(
        current, request(current, "sound", AccessibilityActionKind::SetValue, "true")));
    current.nodes[2].enabled = false;
    REQUIRE_FALSE(acceptsAccessibilityAction(current, request(current)));
    REQUIRE_FALSE(acceptsAccessibilityAction(
        current, request(current, "sound", AccessibilityActionKind::Focus)));
    current.nodes[1].editable = false;
    REQUIRE_FALSE(acceptsAccessibilityAction(
        current, request(current, "name", AccessibilityActionKind::SetValue, "İpek")));
}

TEST_CASE("Accessibility publication is idempotent and callbacks see the committed snapshot",
          "[accessibility][bridge]") {
    AccessibilityBridge bridge;
    const auto platform = latestPlatform;
    const auto current = snapshot();
    int nativeHandle = 0;
    REQUIRE(bridge.attach(&nativeHandle));
    REQUIRE(bridge.publish(current));
    REQUIRE(bridge.publish(current));
    REQUIRE(platform->publications == 1);

    auto changed = current;
    ++changed.generation;
    changed.nodes[1].value = "İpek";
    bool acceptedDuringPublish = false;
    platform->duringPublish = [&] {
        acceptedDuringPublish =
            platform->enqueue(request(changed, "name", AccessibilityActionKind::SetValue, "Çağrı"));
    };
    REQUIRE(bridge.publish(changed));
    REQUIRE(acceptedDuringPublish);
    REQUIRE(platform->lastSnapshot == changed);
    platform->duringPublish = {};

    auto malformed = changed;
    malformed.nodes[1].parentId = "missing";
    REQUIRE_FALSE(bridge.publish(malformed));
    REQUIRE(platform->publications == 2);
    const auto queued = bridge.takeActions();
    REQUIRE(queued.size() == 1);
    REQUIRE(queued.front().value == "Çağrı");
    REQUIRE(bridge.takeActions().empty());
}

TEST_CASE("Accessibility restores native focus without republishing unchanged semantics",
          "[accessibility][bridge][focus]") {
    AccessibilityBridge bridge;
    const auto platform = latestPlatform;
    const auto current = snapshot();
    int nativeHandle = 0;
    REQUIRE_FALSE(bridge.publish(current));
    REQUIRE(platform->focusRefreshes == 0);
    REQUIRE(bridge.attach(&nativeHandle));
    REQUIRE(bridge.publish(current));
    REQUIRE(platform->ownsNativeFocus);
    REQUIRE(platform->focusRefreshes == 1);
    REQUIRE(platform->publications == 1);

    // A native click can change first-responder ownership without changing any
    // scene node. The next frame must reconnect focus without a tree notification.
    platform->ownsNativeFocus = false;
    REQUIRE(bridge.publish(current));
    REQUIRE(platform->ownsNativeFocus);
    REQUIRE(platform->focusRefreshes == 2);
    REQUIRE(platform->publications == 1);

    auto changed = current;
    ++changed.generation;
    changed.roundId = "next-round";
    bool acceptedDuringRefresh = false;
    platform->duringRefresh = [&] {
        // Both operations lock the queue; a native refresh under that lock would
        // deadlock. Fresh actions must already see the committed identity.
        REQUIRE(bridge.attached());
        REQUIRE_FALSE(platform->enqueue(request(current)));
        acceptedDuringRefresh =
            platform->enqueue(request(changed, "name", AccessibilityActionKind::SetValue, "Çağrı"));
    };
    REQUIRE(bridge.publish(changed));
    REQUIRE(acceptedDuringRefresh);
    REQUIRE(platform->focusRefreshes == 3);
    REQUIRE(platform->publications == 2);
    const auto queued = bridge.takeActions();
    REQUIRE(queued.size() == 1);
    REQUIRE(queued.front().generation == changed.generation);
    REQUIRE(queued.front().roundId == changed.roundId);
    platform->duringRefresh = {};

    auto invalid = changed;
    invalid.nodes[1].parentId = "missing";
    platform->ownsNativeFocus = false;
    REQUIRE_FALSE(bridge.publish(invalid));
    REQUIRE_FALSE(platform->ownsNativeFocus);
    REQUIRE(platform->focusRefreshes == 3);
    REQUIRE(platform->publications == 2);

    bridge.detach();
    REQUIRE_FALSE(bridge.publish(changed));
    REQUIRE_FALSE(platform->ownsNativeFocus);
    REQUIRE(platform->focusRefreshes == 3);
    REQUIRE(platform->publications == 2);
}

TEST_CASE("Accessibility bounds the native queue and preserves FIFO order",
          "[accessibility][bridge]") {
    AccessibilityBridge bridge;
    const auto platform = latestPlatform;
    const auto current = snapshot();
    int nativeHandle = 0;
    REQUIRE(bridge.attach(&nativeHandle));
    REQUIRE(bridge.publish(current));
    for (int index = 0; index < 64; ++index)
        REQUIRE(platform->enqueue(
            request(current, "name", AccessibilityActionKind::SetValue, std::to_string(index))));
    REQUIRE_FALSE(platform->enqueue(request(current)));
    const auto queued = bridge.takeActions();
    REQUIRE(queued.size() == 64);
    for (std::size_t index = 0; index < queued.size(); ++index) {
        REQUIRE(queued[index].value == std::to_string(index));
        REQUIRE(queued[index].generation == current.generation);
        REQUIRE(queued[index].roundId == current.roundId);
    }
    REQUIRE(bridge.takeActions().empty());
    REQUIRE(platform->enqueue(request(current)));
    REQUIRE(bridge.takeActions().size() == 1);
}

TEST_CASE("Accessibility prunes queued requests at scene round and capability changes",
          "[accessibility][bridge]") {
    AccessibilityBridge bridge;
    const auto platform = latestPlatform;
    auto current = snapshot();
    int nativeHandle = 0;
    REQUIRE(bridge.attach(&nativeHandle));
    REQUIRE(bridge.publish(current));

    const auto priorScene = request(current);
    REQUIRE(platform->enqueue(priorScene));
    ++current.generation;
    REQUIRE(bridge.publish(current));
    REQUIRE(bridge.takeActions().empty());
    REQUIRE_FALSE(platform->enqueue(priorScene));

    const auto priorRound = request(current);
    REQUIRE(platform->enqueue(priorRound));
    current.roundId = "new-round";
    REQUIRE(bridge.publish(current));
    REQUIRE(bridge.takeActions().empty());
    REQUIRE_FALSE(platform->enqueue(priorRound));

    const auto move = request(current, "board", AccessibilityActionKind::MoveLeft);
    REQUIRE(platform->enqueue(move));
    current.nodes[3].customActions.clear();
    REQUIRE(bridge.publish(current));
    REQUIRE(bridge.takeActions().empty());
    REQUIRE_FALSE(platform->enqueue(move));

    REQUIRE(platform->enqueue(request(current)));
    current.nodes[2].enabled = false;
    REQUIRE(bridge.publish(current));
    REQUIRE(bridge.takeActions().empty());
    REQUIRE_FALSE(platform->enqueue(request(current)));
}

TEST_CASE("Accessibility detaches safely while native callbacks remain retained",
          "[accessibility][bridge]") {
    detail::AccessibilityEnqueue retained;
    std::shared_ptr<PlatformProbe> platform;
    const auto current = snapshot();
    {
        AccessibilityBridge bridge;
        platform = latestPlatform;
        retained = platform->enqueue;
        int nativeHandle = 0;
        REQUIRE(bridge.supported());
        REQUIRE_FALSE(bridge.attach(nullptr));
        REQUIRE_FALSE(bridge.attached());
        REQUIRE_FALSE(bridge.publish(current));
        REQUIRE_FALSE(retained(request(current)));
        platform->acceptsAttach = false;
        REQUIRE_FALSE(bridge.attach(&nativeHandle));
        REQUIRE_FALSE(bridge.attached());
        platform->acceptsAttach = true;
        REQUIRE(bridge.attach(&nativeHandle));
        REQUIRE(bridge.publish(current));
        REQUIRE(retained(request(current)));

        bool acceptedDuringDetach = true;
        platform->duringDetach = [&] { acceptedDuringDetach = retained(request(current)); };
        bridge.detach();
        REQUIRE_FALSE(acceptedDuringDetach);
        platform->duringDetach = {};
        REQUIRE_FALSE(bridge.attached());
        REQUIRE_FALSE(platform->attached);
        REQUIRE(bridge.takeActions().empty());
        REQUIRE_FALSE(retained(request(current)));
        bridge.detach();

        REQUIRE(bridge.attach(&nativeHandle));
        REQUIRE_FALSE(retained(request(current)));
        REQUIRE(bridge.publish(current));
        REQUIRE(retained(request(current)));
    }
    REQUIRE(platform->destructions == 1);
    REQUIRE_FALSE(retained(request(current)));
}

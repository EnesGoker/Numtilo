#include "app/ui/Accessibility.hpp"

namespace app::ui::detail {
namespace {
class UnsupportedAccessibility final : public AccessibilityPlatform {
  public:
    bool supported() const override {
        return false;
    }
    bool attach(void *) override {
        return false;
    }
    void detach() override {
    }
    void publish(const AccessibilitySnapshot &) override {
    }
};
} // namespace

std::unique_ptr<AccessibilityPlatform> makeAccessibilityPlatform(AccessibilityEnqueue) {
    return std::make_unique<UnsupportedAccessibility>();
}
} // namespace app::ui::detail

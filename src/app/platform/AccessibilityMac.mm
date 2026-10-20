#include "app/ui/Accessibility.hpp"

#import <AppKit/AppKit.h>

#include <algorithm>

using app::ui::AccessibilityAction;
using app::ui::AccessibilityActionKind;
using app::ui::AccessibilityNode;
using app::ui::AccessibilityRole;
using app::ui::AccessibilitySnapshot;

namespace {
NSString *nativeString(const std::string &value) {
    NSString *result = [[NSString alloc] initWithBytes:value.data()
                                                length:value.size()
                                              encoding:NSUTF8StringEncoding];
    return result ? result : @"";
}

NSString *nativeLabel(const AccessibilityNode &node) {
    // Static text captions carry their complete value in the spoken label.
    // Table cells expose their contents through a native static-text child.
    if (node.role == AccessibilityRole::StaticText && !node.value.empty())
        return nativeString(node.name.empty() ? node.value : node.name + ": " + node.value);
    return nativeString(node.name);
}

bool scalarBoundary(NSString *value, NSUInteger index) {
    if (index > value.length)
        return false;
    if (index == 0 || index == value.length)
        return true;
    const unichar before = [value characterAtIndex:index - 1];
    const unichar after = [value characterAtIndex:index];
    return !(before >= 0xd800 && before <= 0xdbff && after >= 0xdc00 && after <= 0xdfff);
}

bool scalarRange(NSString *value, NSRange range) {
    return range.location <= value.length && range.length <= value.length - range.location &&
           scalarBoundary(value, range.location) &&
           scalarBoundary(value, range.location + range.length);
}

NSAccessibilityRole nativeRole(AccessibilityRole role) {
    switch (role) {
    case AccessibilityRole::Group:
        return NSAccessibilityGroupRole;
    case AccessibilityRole::StaticText:
        return NSAccessibilityStaticTextRole;
    case AccessibilityRole::Button:
        return NSAccessibilityButtonRole;
    case AccessibilityRole::TextField:
        return NSAccessibilityTextFieldRole;
    case AccessibilityRole::Toggle:
        return NSAccessibilityCheckBoxRole;
    case AccessibilityRole::Table:
        return NSAccessibilityTableRole;
    case AccessibilityRole::Row:
        return NSAccessibilityRowRole;
    case AccessibilityRole::Cell:
        return NSAccessibilityCellRole;
    }
    return NSAccessibilityGroupRole;
}
} // namespace

@class NumtiloAXElement;
@class NumtiloAXColumn;
@class NumtiloAXCellText;

@interface NumtiloAXHostView : NSView {
  @public
    AccessibilitySnapshot snapshot;
    app::ui::detail::AccessibilityEnqueue enqueue;
}
@property(nonatomic, strong) NSMutableDictionary<NSString *, NumtiloAXElement *> *elements;
@property(nonatomic, copy) NSArray<NumtiloAXElement *> *roots;
- (NSRect)screenFrame:(const app::ui::AccessibilityFrame &)frame;
- (void)publish:(const AccessibilitySnapshot &)value;
- (void)invalidate;
@end

@interface NumtiloAXElement : NSAccessibilityElement {
  @public
    AccessibilityNode node;
    std::uint64_t generation;
    std::string roundId;
    BOOL active;
    NSRange selection;
}
@property(nonatomic, weak) NumtiloAXHostView *owner;
@property(nonatomic, weak) id parentObject;
@property(nonatomic, copy) NSArray<NumtiloAXElement *> *childElements;
@property(nonatomic, copy) NSArray<NSAccessibilityCustomAction *> *nodeActions;
@property(nonatomic, copy) NSArray<NumtiloAXColumn *> *columnElements;
@property(nonatomic, strong) NumtiloAXCellText *cellText;
- (BOOL)enqueueKind:(AccessibilityActionKind)kind value:(NSString *)value;
- (void)updateNode:(const AccessibilityNode &)value;
@end

@interface NumtiloAXColumn : NSAccessibilityElement
@property(nonatomic, weak) NumtiloAXElement *table;
@property(nonatomic) NSInteger index;
@end

@interface NumtiloAXCellText : NSAccessibilityElement <NSAccessibilityStaticText>
@property(nonatomic, weak) NumtiloAXElement *cell;
@end

@implementation NumtiloAXCellText
- (BOOL)isAccessibilityElement {
    NumtiloAXElement *cell = self.cell;
    return cell && cell->active && cell.owner != nil && cell->node.role == AccessibilityRole::Cell;
}
- (NSString *)accessibilityRole {
    return NSAccessibilityStaticTextRole;
}
- (NSString *)accessibilityIdentifier {
    return [self isAccessibilityElement] ? nativeString(self.cell->node.id + "-value") : @"";
}
- (NSString *)accessibilityLabel {
    return [self isAccessibilityElement] ? nativeString(self.cell->node.value) : @"";
}
- (NSString *)accessibilityTitle {
    return [self accessibilityLabel];
}
- (id)accessibilityValue {
    return [self accessibilityLabel];
}
- (NSString *)accessibilityValueDescription {
    return [self accessibilityLabel];
}
- (BOOL)isAccessibilityEnabled {
    return [self isAccessibilityElement] && self.cell->node.enabled;
}
- (BOOL)isAccessibilityFocused {
    return NO;
}
- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector {
    if (selector == @selector(setAccessibilityFocused:) ||
        selector == @selector(setAccessibilityValue:) ||
        selector == @selector(setAccessibilitySelectedText:) ||
        selector == @selector(setAccessibilitySelectedTextRange:) ||
        selector == @selector(accessibilityPerformPress))
        return NO;
    return [super isAccessibilitySelectorAllowed:selector];
}
- (id)accessibilityParent {
    return [self isAccessibilityElement] ? self.cell : nil;
}
- (id)accessibilityWindow {
    return [self isAccessibilityElement] ? self.cell.owner.window : nil;
}
- (id)accessibilityTopLevelUIElement {
    return [self accessibilityWindow];
}
- (NSRect)accessibilityFrame {
    return [self isAccessibilityElement] ? self.cell.accessibilityFrame : NSZeroRect;
}
- (NSRect)accessibilityFrameInParentSpace {
    const NSRect frame = [self accessibilityFrame];
    return NSMakeRect(0, 0, frame.size.width, frame.size.height);
}
- (NSArray *)accessibilityChildren {
    return @[];
}
- (NSArray *)accessibilityVisibleChildren {
    return @[];
}
- (NSArray *)accessibilityChildrenInNavigationOrder {
    return @[];
}
- (NSArray<NSAccessibilityCustomAction *> *)accessibilityCustomActions {
    return @[];
}
- (id)accessibilityHitTest:(NSPoint)point {
    (void)point;
    return [self isAccessibilityElement] ? self : nil;
}
@end

@implementation NumtiloAXColumn
- (BOOL)isAccessibilityElement {
    NumtiloAXElement *table = self.table;
    return table && table->active && table.owner != nil;
}
- (NSString *)accessibilityRole {
    return NSAccessibilityColumnRole;
}
- (NSString *)accessibilityLabel {
    return [NSString stringWithFormat:@"Sütun %ld", static_cast<long>(self.index + 1)];
}
- (NSInteger)accessibilityIndex {
    return self.index;
}
- (id)accessibilityParent {
    return self.table;
}
- (id)accessibilityWindow {
    return self.table.owner.window;
}
- (id)accessibilityTopLevelUIElement {
    return self.table.owner.window;
}
- (NSArray *)accessibilityChildren {
    NSMutableArray *cells = [NSMutableArray array];
    for (NumtiloAXElement *row in self.table.childElements)
        for (NumtiloAXElement *cell in row.childElements)
            if (cell->node.role == AccessibilityRole::Cell && cell->node.column == self.index)
                [cells addObject:cell];
    return cells;
}
- (NSArray *)accessibilityVisibleChildren {
    return [self accessibilityChildren];
}
- (NSRect)accessibilityFrame {
    NSRect frame = NSZeroRect;
    bool first = true;
    for (NumtiloAXElement *cell in [self accessibilityChildren]) {
        frame = first ? cell.accessibilityFrame : NSUnionRect(frame, cell.accessibilityFrame);
        first = false;
    }
    return frame;
}
@end

@implementation NumtiloAXElement

- (void)updateNode:(const AccessibilityNode &)value {
    if (node.value != value.value)
        selection = NSMakeRange(nativeString(value.value).length, 0);
    node = value;
    if (node.role == AccessibilityRole::Cell) {
        if (!self.cellText) {
            self.cellText = [[NumtiloAXCellText alloc] init];
            self.cellText.cell = self;
        }
    } else
        self.cellText = nil;
    if (node.role == AccessibilityRole::Table &&
        self.columnElements.count != static_cast<NSUInteger>(std::max(0, node.columnCount))) {
        NSMutableArray<NumtiloAXColumn *> *columns = [NSMutableArray array];
        for (int i = 0; i < node.columnCount; ++i) {
            NumtiloAXColumn *column = [[NumtiloAXColumn alloc] init];
            column.table = self;
            column.index = i;
            [columns addObject:column];
        }
        self.columnElements = columns;
    } else if (node.role != AccessibilityRole::Table)
        self.columnElements = @[];
    NSMutableArray<NSAccessibilityCustomAction *> *actions = [NSMutableArray array];
    for (const auto &custom : node.customActions) {
        __weak NumtiloAXElement *weakSelf = self;
        const auto kind = custom.kind;
        [actions addObject:[[NSAccessibilityCustomAction alloc]
                               initWithName:nativeString(custom.name)
                                    handler:^BOOL {
                                      NumtiloAXElement *element = weakSelf;
                                      return element && [element enqueueKind:kind value:@""];
                                    }]];
    }
    self.nodeActions = actions;
}

- (BOOL)enqueueKind:(AccessibilityActionKind)kind value:(NSString *)value {
    NumtiloAXHostView *host = self.owner;
    if (!active || !host || !node.enabled)
        return NO;
    NSData *bytes = [value dataUsingEncoding:NSUTF8StringEncoding];
    if (!bytes)
        return NO;
    AccessibilityAction action;
    action.targetId = node.id;
    action.kind = kind;
    if (bytes.length != 0)
        action.value.assign(static_cast<const char *>(bytes.bytes), bytes.length);
    action.generation = generation;
    action.roundId = roundId;
    return host->enqueue && host->enqueue(std::move(action));
}

- (BOOL)isAccessibilityElement {
    return active && self.owner != nil;
}
- (NSString *)accessibilityIdentifier {
    return nativeString(node.id);
}
- (NSString *)accessibilityRole {
    return nativeRole(node.role);
}
- (NSString *)accessibilityLabel {
    return nativeLabel(node);
}
- (NSString *)accessibilityTitle {
    return nativeLabel(node);
}
- (NSString *)accessibilityHelp {
    if (node.error.empty())
        return nativeString(node.help);
    return nativeString(node.help + (node.help.empty() ? "" : ". ") + node.error);
}
- (id)accessibilityValue {
    if (node.role == AccessibilityRole::Toggle)
        return @(node.checked);
    return nativeString(node.value);
}
- (NSString *)accessibilityValueDescription {
    if (node.role == AccessibilityRole::Toggle)
        return node.checked ? @"Açık" : @"Kapalı";
    return nativeString(node.value);
}
- (BOOL)isAccessibilityEnabled {
    return active && node.enabled;
}
- (BOOL)isAccessibilityRequired {
    return node.required;
}
- (BOOL)isAccessibilityFocused {
    return active && node.focused;
}
- (void)setAccessibilityFocused:(BOOL)focused {
    if (focused)
        [self enqueueKind:AccessibilityActionKind::Focus value:@""];
}
- (void)setAccessibilityValue:(id)value {
    if (node.role == AccessibilityRole::TextField && node.editable &&
        [value isKindOfClass:[NSString class]]) {
        [self enqueueKind:AccessibilityActionKind::SetValue value:value];
    } else if (node.role == AccessibilityRole::Toggle) {
        NSString *desired = nil;
        if ([value isKindOfClass:[NSNumber class]]) {
            const double number = [value doubleValue];
            if (number == 0.0 || number == 1.0)
                desired = number == 1.0 ? @"true" : @"false";
        } else if ([value isKindOfClass:[NSString class]]) {
            NSString *lower = [value lowercaseString];
            if ([lower isEqualToString:@"1"] || [lower isEqualToString:@"true"])
                desired = @"true";
            else if ([lower isEqualToString:@"0"] || [lower isEqualToString:@"false"])
                desired = @"false";
        }
        if (desired)
            [self enqueueKind:AccessibilityActionKind::SetValue value:desired];
    }
}
- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector {
    if (selector == @selector(setAccessibilityValue:)) {
        const bool declared = std::find(node.actions.begin(), node.actions.end(),
                                        AccessibilityActionKind::SetValue) != node.actions.end();
        return active && node.enabled && declared &&
               ((node.role == AccessibilityRole::TextField && node.editable) ||
                node.role == AccessibilityRole::Toggle);
    }
    if (selector == @selector(setAccessibilitySelectedText:) ||
        selector == @selector(setAccessibilitySelectedTextRange:)) {
        return active && node.enabled && node.editable && node.role == AccessibilityRole::TextField;
    }
    if (selector == @selector(setAccessibilityFocused:))
        return active && node.focusable;
    if (selector == @selector(accessibilityPerformPress)) {
        return active && node.enabled &&
               std::find(node.actions.begin(), node.actions.end(),
                         AccessibilityActionKind::Press) != node.actions.end();
    }
    return [super isAccessibilitySelectorAllowed:selector];
}
- (BOOL)accessibilityPerformPress {
    return [self enqueueKind:AccessibilityActionKind::Press value:@""];
}
- (NSRect)accessibilityFrame {
    NumtiloAXHostView *host = self.owner;
    return host ? [host screenFrame:node.frame] : NSZeroRect;
}
- (NSRect)accessibilityFrameInParentSpace {
    NSRect frame = [self accessibilityFrame];
    id parent = self.parentObject;
    if ([parent respondsToSelector:@selector(accessibilityFrame)]) {
        NSRect parentFrame = [parent accessibilityFrame];
        frame.origin.x -= parentFrame.origin.x;
        frame.origin.y -= parentFrame.origin.y;
    }
    return frame;
}
- (id)accessibilityParent {
    return self.parentObject;
}
- (id)accessibilityWindow {
    return self.owner.window;
}
- (id)accessibilityTopLevelUIElement {
    return self.owner.window;
}
- (NSArray *)accessibilityChildren {
    if (node.role == AccessibilityRole::Cell && self.cellText)
        return @[ self.cellText ];
    return self.childElements ? self.childElements : @[];
}
- (NSArray *)accessibilityVisibleChildren {
    return [self accessibilityChildren];
}
- (NSArray *)accessibilityChildrenInNavigationOrder {
    return [self accessibilityChildren];
}
- (NSArray<NSAccessibilityCustomAction *> *)accessibilityCustomActions {
    return self.nodeActions;
}
- (id)accessibilityHitTest:(NSPoint)point {
    for (NSAccessibilityElement *child in [[self accessibilityChildren] reverseObjectEnumerator]) {
        if (NSPointInRect(point, child.accessibilityFrame))
            return [child accessibilityHitTest:point];
    }
    return self;
}

- (NSInteger)accessibilityRowCount {
    return node.rowCount;
}
- (NSInteger)accessibilityColumnCount {
    return node.columnCount;
}
- (NSArray *)accessibilityRows {
    NSMutableArray *rows = [NSMutableArray array];
    for (NumtiloAXElement *child in self.childElements)
        if (child->node.role == AccessibilityRole::Row)
            [rows addObject:child];
    return rows;
}
- (NSArray *)accessibilityVisibleRows {
    return [self accessibilityRows];
}
- (NSArray *)accessibilityColumns {
    return self.columnElements ? self.columnElements : @[];
}
- (NSArray *)accessibilityVisibleColumns {
    return [self accessibilityColumns];
}
- (NSInteger)accessibilityIndex {
    return node.role == AccessibilityRole::Row ? node.row : node.column;
}
- (NSInteger)accessibilityDisclosureLevel {
    return 0;
}
- (NSArray *)accessibilityVisibleCells {
    NSMutableArray *cells = [NSMutableArray array];
    for (NumtiloAXElement *row in [self accessibilityRows])
        for (NumtiloAXElement *cell in row.childElements)
            if (cell->node.role == AccessibilityRole::Cell)
                [cells addObject:cell];
    return cells;
}
- (NSRange)accessibilityRowIndexRange {
    return node.row >= 0 ? NSMakeRange(static_cast<NSUInteger>(node.row), 1)
                         : NSMakeRange(NSNotFound, 0);
}
- (NSRange)accessibilityColumnIndexRange {
    return node.column >= 0 ? NSMakeRange(static_cast<NSUInteger>(node.column), 1)
                            : NSMakeRange(NSNotFound, 0);
}
- (id)accessibilityCellForColumn:(NSInteger)column row:(NSInteger)row {
    for (NumtiloAXElement *rowElement in [self accessibilityRows])
        for (NumtiloAXElement *cell in rowElement.childElements)
            if (cell->node.row == row && cell->node.column == column)
                return cell;
    return nil;
}

- (NSInteger)accessibilityNumberOfCharacters {
    return nativeString(node.value).length;
}
- (NSRange)accessibilityVisibleCharacterRange {
    return NSMakeRange(0, nativeString(node.value).length);
}
- (NSRange)accessibilitySelectedTextRange {
    NSString *value = nativeString(node.value);
    return scalarRange(value, selection) ? selection : NSMakeRange(value.length, 0);
}
- (void)setAccessibilitySelectedTextRange:(NSRange)range {
    if (node.role == AccessibilityRole::TextField && node.editable &&
        scalarRange(nativeString(node.value), range) &&
        [self enqueueKind:AccessibilityActionKind::Focus value:@""]) {
        selection = range;
    }
}
- (NSString *)accessibilitySelectedText {
    return [nativeString(node.value) substringWithRange:[self accessibilitySelectedTextRange]];
}
- (void)setAccessibilitySelectedText:(NSString *)value {
    if (!node.editable || ![value isKindOfClass:[NSString class]])
        return;
    NSString *replacement = [nativeString(node.value)
        stringByReplacingCharactersInRange:[self accessibilitySelectedTextRange]
                                withString:value];
    [self enqueueKind:AccessibilityActionKind::SetValue value:replacement];
}
- (NSString *)accessibilityStringForRange:(NSRange)range {
    NSString *value = nativeString(node.value);
    return scalarRange(value, range) ? [value substringWithRange:range] : nil;
}
- (NSAttributedString *)accessibilityAttributedStringForRange:(NSRange)range {
    NSString *value = [self accessibilityStringForRange:range];
    return value ? [[NSAttributedString alloc] initWithString:value] : nil;
}
- (NSInteger)accessibilityLineForIndex:(NSInteger)index {
    return index >= 0 && index <= [self accessibilityNumberOfCharacters] ? 0 : NSNotFound;
}
- (NSRange)accessibilityRangeForLine:(NSInteger)line {
    return line == 0 ? [self accessibilityVisibleCharacterRange] : NSMakeRange(NSNotFound, 0);
}
- (NSInteger)accessibilityInsertionPointLineNumber {
    return 0;
}
- (NSRange)accessibilityRangeForIndex:(NSInteger)index {
    NSString *value = nativeString(node.value);
    if (index < 0 || static_cast<NSUInteger>(index) > value.length)
        return NSMakeRange(NSNotFound, 0);
    if (static_cast<NSUInteger>(index) == value.length)
        return NSMakeRange(value.length, 0);
    return [value rangeOfComposedCharacterSequenceAtIndex:static_cast<NSUInteger>(index)];
}
- (NSRange)accessibilityStyleRangeForIndex:(NSInteger)index {
    return index >= 0 && index <= [self accessibilityNumberOfCharacters]
               ? [self accessibilityVisibleCharacterRange]
               : NSMakeRange(NSNotFound, 0);
}
- (NSRect)accessibilityFrameForRange:(NSRange)range {
    return [self accessibilityStringForRange:range] ? [self accessibilityFrame] : NSZeroRect;
}
@end

@implementation NumtiloAXHostView
- (BOOL)isOpaque {
    return NO;
}
- (BOOL)isFlipped {
    return NO;
}
- (NSView *)hitTest:(NSPoint)point {
    (void)point;
    return nil;
}
- (BOOL)acceptsFirstResponder {
    // AppKit narrows its accessibility focus search through the first responder.
    // Inherited key handling continues up the responder chain to the SFML view.
    return YES;
}
- (BOOL)isAccessibilityElement {
    return YES;
}
- (NSString *)accessibilityRole {
    return NSAccessibilityGroupRole;
}
- (NSString *)accessibilityIdentifier {
    return @"numtilo.semantic-content";
}
- (NSString *)accessibilityLabel {
    return nativeString(snapshot.sceneName);
}
- (NSArray *)accessibilityChildren {
    return self.roots ? self.roots : @[];
}
- (NSArray *)accessibilityChildrenInNavigationOrder {
    return [self accessibilityChildren];
}
- (id)accessibilityParent {
    return NSAccessibilityUnignoredAncestor(self.superview);
}
- (id)accessibilityWindow {
    return self.window;
}
- (id)accessibilityTopLevelUIElement {
    return self.window;
}
- (id)accessibilityFocusedUIElement {
    for (NumtiloAXElement *element in self.elements.allValues)
        if (element->node.focused)
            return element;
    return self;
}
- (id)accessibilityHitTest:(NSPoint)point {
    for (NumtiloAXElement *element in [self.roots reverseObjectEnumerator])
        if (NSPointInRect(point, element.accessibilityFrame))
            return [element accessibilityHitTest:point];
    return self;
}
- (NSRect)screenFrame:(const app::ui::AccessibilityFrame &)frame {
    if (!self.window || snapshot.clientPixelWidth == 0 || snapshot.clientPixelHeight == 0)
        return NSZeroRect;
    const NSRect bounds = self.bounds;
    const CGFloat sx = bounds.size.width / snapshot.clientPixelWidth;
    const CGFloat sy = bounds.size.height / snapshot.clientPixelHeight;
    const NSRect local =
        NSMakeRect(bounds.origin.x + frame.x * sx,
                   bounds.origin.y + bounds.size.height - (frame.y + frame.height) * sy,
                   frame.width * sx, frame.height * sy);
    return [self.window convertRectToScreen:[self convertRect:local toView:nil]];
}
- (void)invalidate {
    for (NumtiloAXElement *element in self.elements.allValues) {
        element->active = NO;
        element.owner = nil;
        element.parentObject = nil;
        element.childElements = @[];
        element.nodeActions = @[];
        element.columnElements = @[];
        element.cellText = nil;
    }
    self.roots = @[];
    self.elements = [NSMutableDictionary dictionary];
}
- (void)publish:(const AccessibilitySnapshot &)value {
    const bool boundary =
        snapshot.generation != value.generation || snapshot.roundId != value.roundId;
    bool layoutChanged = boundary || snapshot.nodes.size() != value.nodes.size();
    if (!layoutChanged)
        for (std::size_t i = 0; i < value.nodes.size(); ++i) {
            const auto &before = snapshot.nodes[i];
            const auto &after = value.nodes[i];
            if (before.id != after.id || before.parentId != after.parentId ||
                before.role != after.role || before.frame != after.frame ||
                before.actions != after.actions || before.customActions != after.customActions ||
                before.enabled != after.enabled) {
                layoutChanged = true;
                break;
            }
        }
    const auto previous = snapshot;
    if (boundary)
        [self invalidate];
    snapshot = value;
    NSMutableSet<NSString *> *present = [NSMutableSet set];
    NSMutableArray<NumtiloAXElement *> *changed = [NSMutableArray array];
    NumtiloAXElement *focused = nil;
    for (const auto &data : snapshot.nodes) {
        NSString *identifier = nativeString(data.id);
        [present addObject:identifier];
        NumtiloAXElement *element = self.elements[identifier];
        if (!element) {
            element = [[NumtiloAXElement alloc] init];
            element->generation = snapshot.generation;
            element->roundId = snapshot.roundId;
            element->active = YES;
            element.owner = self;
            self.elements[identifier] = element;
        } else if (element->node.value != data.value || element->node.checked != data.checked ||
                   element->node.error != data.error) {
            [changed addObject:element];
        }
        const bool gainedFocus = data.focused && !element->node.focused;
        [element updateNode:data];
        element.childElements = @[];
        if (gainedFocus || (boundary && data.focused))
            focused = element;
    }
    for (NSString *identifier in [self.elements.allKeys copy]) {
        if (![present containsObject:identifier]) {
            NumtiloAXElement *element = self.elements[identifier];
            element->active = NO;
            element.owner = nil;
            element.parentObject = nil;
            element.childElements = @[];
            element.nodeActions = @[];
            element.columnElements = @[];
            element.cellText = nil;
            [self.elements removeObjectForKey:identifier];
        }
    }
    NSMutableArray<NumtiloAXElement *> *roots = [NSMutableArray array];
    for (const auto &data : snapshot.nodes) {
        NumtiloAXElement *element = self.elements[nativeString(data.id)];
        NumtiloAXElement *parent =
            data.parentId.empty() ? nil : self.elements[nativeString(data.parentId)];
        element.parentObject = parent ? static_cast<id>(parent) : static_cast<id>(self);
        if (parent)
            parent.childElements = [parent.childElements arrayByAddingObject:element];
        else
            [roots addObject:element];
    }
    self.roots = roots;
    if (layoutChanged)
        NSAccessibilityPostNotification(self, NSAccessibilityLayoutChangedNotification);
    if (focused)
        NSAccessibilityPostNotification(focused,
                                        NSAccessibilityFocusedUIElementChangedNotification);
    // Large board changes get one group notification; individual values are bounded.
    if (changed.count <= 8) {
        for (NumtiloAXElement *element in changed)
            NSAccessibilityPostNotification(element, NSAccessibilityValueChangedNotification);
    } else
        NSAccessibilityPostNotification(self, NSAccessibilityValueChangedNotification);
    if (!snapshot.announcement.empty() && snapshot.announcement != previous.announcement) {
        NSAccessibilityPostNotificationWithUserInfo(
            NSApp, NSAccessibilityAnnouncementRequestedNotification, @{
                NSAccessibilityAnnouncementKey : nativeString(snapshot.announcement),
                NSAccessibilityPriorityKey : @(NSAccessibilityPriorityMedium)
            });
    }
}
@end

namespace app::ui::detail {
namespace {
class MacAccessibility final : public AccessibilityPlatform {
  public:
    explicit MacAccessibility(AccessibilityEnqueue enqueue) : enqueue_(std::move(enqueue)) {
    }
    ~MacAccessibility() override {
        detach();
    }
    bool supported() const override {
        return true;
    }
    bool attach(void *nativeWindow) override {
        if (![NSThread isMainThread] || nativeWindow == nullptr)
            return false;
        NSWindow *window = (__bridge NSWindow *)nativeWindow;
        if (![window isKindOfClass:[NSWindow class]] || window.contentView == nil)
            return false;
        window_ = window;
        oldModal_ = window.accessibilityModal;
        previousResponder_ = window.firstResponder;
        renderer_ = window.contentView;
        host_ = [[NumtiloAXHostView alloc] initWithFrame:window.contentView.bounds];
        host_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        host_.elements = [NSMutableDictionary dictionary];
        host_.roots = @[];
        host_->enqueue = enqueue_;
        [window.contentView addSubview:host_];
        [window makeFirstResponder:host_];
        return true;
    }
    void detach() override {
        NumtiloAXHostView *host = host_;
        NSWindow *window = window_;
        const BOOL oldModal = oldModal_;
        NSResponder *previousResponder = previousResponder_;
        if (!host)
            return;
        const auto detach = ^{
          if (window.firstResponder == host)
              [window makeFirstResponder:previousResponder];
          [host invalidate];
          [host removeFromSuperview];
          if (window)
              window.accessibilityModal = oldModal;
        };
        if ([NSThread isMainThread])
            detach();
        else
            dispatch_sync(dispatch_get_main_queue(), detach);
        host_ = nil;
        window_ = nil;
        previousResponder_ = nil;
        renderer_ = nil;
    }
    void refreshFocus() override {
        if (![NSThread isMainThread] || !host_ || !window_)
            return;
        // A native mouse press can return first-responder ownership to SFML.
        // Reconnect semantic focus without taking it from unrelated responders.
        NSResponder *responder = window_.firstResponder;
        if (responder && (responder == renderer_ || responder == previousResponder_))
            [window_ makeFirstResponder:host_];
    }
    void publish(const AccessibilitySnapshot &snapshot) override {
        if (![NSThread isMainThread] || !host_)
            return;
        window_.accessibilityModal = snapshot.modal;
        [host_ publish:snapshot];
    }

  private:
    AccessibilityEnqueue enqueue_;
    __weak NSWindow *window_ = nil;
    __strong NumtiloAXHostView *host_ = nil;
    __weak NSResponder *previousResponder_ = nil;
    __weak NSView *renderer_ = nil;
    BOOL oldModal_ = NO;
};
} // namespace
std::unique_ptr<AccessibilityPlatform> makeAccessibilityPlatform(AccessibilityEnqueue enqueue) {
    return std::make_unique<MacAccessibility>(std::move(enqueue));
}
} // namespace app::ui::detail

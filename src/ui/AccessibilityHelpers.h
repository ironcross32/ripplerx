#pragma once

// Shared screen reader / keyboard accessibility helpers for the RipplerX UI.

#include <JuceHeader.h>
#include <optional>
#include "../Globals.h"

namespace a11y
{

//==============================================================================
// Keyboard map shared by every value widget (knobs, pitch fields, curves, bars)

enum class EditAction
{
    None,
    Increase,
    Decrease,
    IncreaseLarge,
    DecreaseLarge,
    ToMax,
    ToMin,
    ToDefault,
    TypeIn,
    Menu,
};

struct EditKey
{
    EditAction action = EditAction::None;
    bool fine = false;      // Ctrl held: smaller steps
    bool velocity = false;  // Shift held: edit the velocity amount
};

// Windows "context menu" (Apps) key
static constexpr int contextMenuKeyCode = 93;

inline EditKey getEditKey(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto code = key.getKeyCode();
    EditKey result;

    if (mods.isAltDown())
        return result; // leave Alt shortcuts to the host

    if ((code == juce::KeyPress::F10Key && mods.isShiftDown()) || code == contextMenuKeyCode) {
        result.action = EditAction::Menu;
        return result;
    }

    result.fine = mods.isCtrlDown();
    result.velocity = mods.isShiftDown();

    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey)
        result.action = EditAction::Increase;
    else if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey)
        result.action = EditAction::Decrease;
    else if (code == juce::KeyPress::pageUpKey)
        result.action = EditAction::IncreaseLarge;
    else if (code == juce::KeyPress::pageDownKey)
        result.action = EditAction::DecreaseLarge;
    else if (code == juce::KeyPress::homeKey)
        result.action = EditAction::ToMax;
    else if (code == juce::KeyPress::endKey)
        result.action = EditAction::ToMin;
    else if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
        result.action = EditAction::ToDefault;
    else if (code == juce::KeyPress::returnKey || code == juce::KeyPress::F2Key)
        result.action = EditAction::TypeIn;

    return result;
}

//==============================================================================
// Interface implemented by every custom value widget. Used by the accessibility
// handler, the keyboard handling and the type-in editor.

class ValueWidget
{
public:
    virtual ~ValueWidget() = default;

    virtual juce::String getValueTitle() const = 0;        // e.g. "Resonator A Decay"
    virtual juce::String getValueText() const = 0;         // e.g. "1.00 s"
    virtual double getNormalisedValue() const = 0;         // 0..1
    virtual void setNormalisedValue(double v) = 0;         // 0..1, wraps a change gesture
    virtual bool setValueFromText(const juce::String& text) = 0; // false if text can't be parsed
    virtual void showAccessibleMenu() = 0;
};

//==============================================================================
// Implemented by the editor so widgets can open a type-in prompt without
// depending on the editor's header.

class TypeInHost
{
public:
    virtual ~TypeInHost() = default;
    virtual void showTypeIn(juce::Component& target, ValueWidget& widget) = 0;
};

inline void openTypeIn(juce::Component& c, ValueWidget& w)
{
    if (auto* host = c.findParentComponentOfClass<TypeInHost>())
        host->showTypeIn(c, w);
}

//==============================================================================
// Extracts the first number found in typed text ("lp 2.5k" -> 2.5, "-6 dB" -> -6).

inline std::optional<double> parseNumber(const juce::String& text)
{
    auto t = text.trim();
    for (int i = 0; i < t.length(); ++i) {
        auto c = t[i];
        bool startsNumber = juce::CharacterFunctions::isDigit(c)
            || ((c == '-' || c == '+' || c == '.') && i + 1 < t.length()
                && (juce::CharacterFunctions::isDigit(t[i + 1]) || t[i + 1] == '.'));
        if (startsNumber)
            return t.substring(i).getDoubleValue();
    }
    return std::nullopt;
}

//==============================================================================

inline void notifyValue(juce::Component& c)
{
    if (auto* h = c.getAccessibilityHandler())
        h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
}

inline void notifyTitle(juce::Component& c)
{
    if (auto* h = c.getAccessibilityHandler())
        h->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
}

// Gives keyboard focus back to a control once a popup menu has closed.
// JUCE usually still has the control focused, so grabbing focus would be a no-op
// and screen readers would never hear that focus left the menu; cycle it instead.
inline void refocusAsync(juce::Component& c)
{
    juce::Component::SafePointer<juce::Component> safe(&c);
    juce::MessageManager::callAsync([safe] {
        if (safe == nullptr || !safe->isShowing())
            return;
        if (safe->hasKeyboardFocus(false))
            safe->giveAwayKeyboardFocus();
        safe->grabKeyboardFocus();
    });
}

// Speaks text through the user's screen reader. On Windows this raises a UIA
// notification event from `source` (or the focused component) instead of using
// JUCE's SAPI voice. Defined in AccessibilityAnnounce.cpp.
void announce(const juce::String& text, juce::Component* source = nullptr);

//==============================================================================
// Group announcements: when keyboard focus moves into a different SectionGroup,
// the newly focused control's accessible title becomes "<Group>, <Title>", so the
// screen reader says the group name, a short pause, then the control and value.
//
// Value widgets (Rotary, Pitch, TensionCtrl) add the prefix in their handler's
// getTitle(). JUCE's own controls read Component::getTitle(), so for those the
// title itself is swapped while they hold focus and restored afterwards.

namespace groupfocus
{
    struct State
    {
        juce::Component::SafePointer<juce::Component> lastGroup; // group of the last focused grouped control
        juce::Component::SafePointer<juce::Component> prefixed;  // control currently carrying the prefix
        juce::String prefix;
        juce::String originalTitle;
        bool retitled = false;
    };

    inline State& state()
    {
        static State s;
        return s;
    }

    inline void restoreTitle()
    {
        auto& s = state();
        if (s.retitled && s.prefixed != nullptr)
            s.prefixed->setTitle(s.originalTitle);
        s.prefixed = nullptr;
        s.retitled = false;
        s.prefix.clear();
        s.originalTitle.clear();
    }

    inline void applyPrefix(juce::Component& c, const juce::String& groupName)
    {
        restoreTitle();
        auto& s = state();
        s.prefixed = &c;
        s.prefix = groupName;

        if (dynamic_cast<ValueWidget*>(&c) == nullptr) {
            s.originalTitle = c.getTitle();
            auto base = s.originalTitle;
            if (base.isEmpty())
                if (auto* b = dynamic_cast<juce::Button*>(&c))
                    base = b->getButtonText();
            s.retitled = true;
            c.setTitle(groupName + ", " + base);
        }
    }

    // Used by custom accessibility handlers instead of retitling the component.
    inline juce::String decorateTitle(const juce::Component& c, const juce::String& title)
    {
        auto& s = state();
        if (s.prefixed.getComponent() == &c && !s.retitled && c.hasKeyboardFocus(false))
            return s.prefix + ", " + title;
        return title;
    }

    // Call from a juce::FocusChangeListener: drops the prefix once focus moves on,
    // so returning to the control within the same group doesn't repeat the group.
    inline void focusChanged(juce::Component* focused)
    {
        auto& s = state();
        if (s.prefixed != nullptr && s.prefixed.getComponent() != focused)
            restoreTitle();
    }
}

inline void drawFocusRing(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour = juce::Colour(globals::COLOR_ACTIVE))
{
    g.setColour(colour);
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.5f);
}

//==============================================================================
// Accessibility handler for custom value widgets: a slider with a text value.

class ValueWidgetHandler : public juce::AccessibilityHandler
{
public:
    template <typename WidgetComponent>
    explicit ValueWidgetHandler(WidgetComponent& c)
        : ValueWidgetHandler(static_cast<juce::Component&>(c), static_cast<ValueWidget&>(c))
    {}

    ValueWidgetHandler(juce::Component& comp, ValueWidget& w)
        : juce::AccessibilityHandler(comp,
            juce::AccessibilityRole::slider,
            juce::AccessibilityActions()
                .addAction(juce::AccessibilityActionType::showMenu, [&w] { w.showAccessibleMenu(); }),
            Interfaces { std::make_unique<Value>(w) })
        , widget(w)
    {}

    juce::String getTitle() const override { return groupfocus::decorateTitle(getComponent(), widget.getValueTitle()); }

    juce::String getHelp() const override
    {
        if (auto* tt = dynamic_cast<juce::TooltipClient*>(const_cast<juce::Component*>(&getComponent())))
            return tt->getTooltip();
        return {};
    }

private:
    class Value : public juce::AccessibilityValueInterface
    {
    public:
        explicit Value(ValueWidget& w) : widget(w) {}

        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return widget.getNormalisedValue(); }
        void setValue(double v) override { widget.setNormalisedValue(juce::jlimit(0.0, 1.0, v)); }
        juce::String getCurrentValueAsString() const override { return widget.getValueText(); }
        void setValueAsString(const juce::String& s) override { widget.setValueFromText(s); }
        AccessibleValueRange getRange() const override { return { { 0.0, 1.0 }, 0.01 }; }

    private:
        ValueWidget& widget;
    };

    ValueWidget& widget;
};

//==============================================================================
// A button that opens a choice menu and reports the current choice as its value.
// Up/Down step through the choices without opening the menu.

class MenuButton : public juce::TextButton
{
public:
    std::function<juce::String()> getValueText;
    std::function<void(int)> jog;

    MenuButton() { setWantsKeyboardFocus(true); }

    bool keyPressed(const juce::KeyPress& key) override
    {
        const auto code = key.getKeyCode();
        const auto mods = key.getModifiers();

        if (code == juce::KeyPress::returnKey || code == juce::KeyPress::spaceKey
            || code == contextMenuKeyCode || (code == juce::KeyPress::F10Key && mods.isShiftDown()))
        {
            if (onClick) onClick();
            return true;
        }
        if (jog && !mods.isAnyModifierKeyDown()) {
            if (code == juce::KeyPress::upKey || code == juce::KeyPress::leftKey) { jog(-1); return true; }
            if (code == juce::KeyPress::downKey || code == juce::KeyPress::rightKey) { jog(1); return true; }
        }
        return false;
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<Handler>(*this);
    }

private:
    class Handler : public juce::AccessibilityHandler
    {
    public:
        explicit Handler(MenuButton& b)
            : juce::AccessibilityHandler(b,
                juce::AccessibilityRole::button,
                juce::AccessibilityActions()
                    .addAction(juce::AccessibilityActionType::press, [&b] { if (b.onClick) b.onClick(); })
                    .addAction(juce::AccessibilityActionType::showMenu, [&b] { if (b.onClick) b.onClick(); }),
                Interfaces { std::make_unique<Value>(b) })
            , button(b)
        {}

        juce::String getHelp() const override { return button.getTooltip(); }

    private:
        class Value : public juce::AccessibilityTextValueInterface
        {
        public:
            explicit Value(MenuButton& b) : button(b) {}
            bool isReadOnly() const override { return true; }
            juce::String getCurrentValueAsString() const override
            {
                return button.getValueText ? button.getValueText() : juce::String();
            }
            void setValueAsString(const juce::String&) override {}
        private:
            MenuButton& button;
        };

        MenuButton& button;
    };
};

//==============================================================================
// Horizontal bar slider (noise OSC, mallet key tracking) with the shared key map
// and percent value text.

class BarSlider : public juce::Slider
{
public:
    BarSlider() { setWantsKeyboardFocus(true); }

    juce::String getTextFromValue(double v) override
    {
        auto range = getMaximum() - getMinimum();
        auto pct = range > 0.0 ? (v - getMinimum()) / range * 100.0 : 0.0;
        return juce::String(pct, pct < 10.0 && pct != 0.0 ? 1 : 0) + " %";
    }

    double getValueFromText(const juce::String& text) override
    {
        auto pct = text.retainCharacters("0123456789.-").getDoubleValue();
        return getMinimum() + juce::jlimit(0.0, 100.0, pct) / 100.0 * (getMaximum() - getMinimum());
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        auto ek = getEditKey(key);
        if (ek.action == EditAction::None || ek.action == EditAction::TypeIn || ek.action == EditAction::Menu)
            return false;

        const auto min = getMinimum(), max = getMaximum();
        const auto span = max - min;
        const auto step = span * (ek.fine ? 0.001 : 0.01);
        double v = getValue();

        switch (ek.action) {
            case EditAction::Increase: v += step; break;
            case EditAction::Decrease: v -= step; break;
            case EditAction::IncreaseLarge: v += span * 0.1; break;
            case EditAction::DecreaseLarge: v -= span * 0.1; break;
            case EditAction::ToMax: v = max; break;
            case EditAction::ToMin: v = min; break;
            case EditAction::ToDefault: v = getDoubleClickReturnValue(); break;
            default: break;
        }

        juce::Slider::ScopedDragNotification drag(*this);
        setValue(juce::jlimit(min, max, v), juce::sendNotificationSync);
        return true;
    }

    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }

    void paintOverChildren(juce::Graphics& g) override
    {
        if (hasKeyboardFocus(false))
            drawFocusRing(g, getLocalBounds().toFloat());
    }
};

//==============================================================================
// Transparent full-size container that groups a section's controls for
// screen readers ("Resonator A grouping") and for Tab order.

class SectionGroup : public juce::Component
{
public:
    explicit SectionGroup(const juce::String& title)
    {
        setTitle(title);
        setInterceptsMouseClicks(false, true);
        // A focus container (not a keyboard focus container) so that screen readers
        // see the group, while Tab still flows from one section to the next.
        setFocusContainerType(FocusContainerType::focusContainer);
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group);
    }

    // First control that Tab would reach in this group, or nullptr.
    juce::Component* getFirstControl()
    {
        juce::KeyboardFocusTraverser traverser;
        for (auto* c : traverser.getAllComponents(this))
            if (c->getWantsKeyboardFocus() && c->isShowing() && c->isEnabled())
                return c;
        return nullptr;
    }

    void focusOfChildComponentChanged(FocusChangeType) override
    {
        auto* focused = juce::Component::getCurrentlyFocusedComponent();
        if (focused == nullptr)
            return;

        auto& s = groupfocus::state();

        if (hasKeyboardFocus(true)) {
            // Focus entered this group with no grouped control focused before it
            // (e.g. the first focus after opening). Usually already handled below.
            if (s.lastGroup.getComponent() != this) {
                s.lastGroup = this;
                groupfocus::applyPrefix(*focused, getTitle());
            }
            return;
        }

        // Focus left this group. JUCE has already made the new control the focused
        // component but hasn't told the screen reader yet, so prefix it now.
        if (auto* group = focused->findParentComponentOfClass<SectionGroup>()) {
            if (group != this && s.lastGroup.getComponent() != group) {
                s.lastGroup = group;
                groupfocus::applyPrefix(*focused, group->getTitle());
            }
        }
    }
};

} // namespace a11y

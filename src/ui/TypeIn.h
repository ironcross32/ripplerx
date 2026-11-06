#pragma once

#include <JuceHeader.h>
#include "AccessibilityHelpers.h"

// Overlay that lets the user type an exact value for a knob or pitch field.
// Enter applies, Escape (or clicking outside the box) cancels. Keyboard focus
// returns to the control that opened it.
class TypeIn : public juce::Component
{
public:
    TypeIn();

    void show(juce::Component& target, a11y::ValueWidget& widget);
    void close();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    void apply();
    juce::Rectangle<int> getBoxBounds() const;

    juce::Label heading;
    juce::Label current;
    juce::TextEditor editor;

    juce::Component::SafePointer<juce::Component> target;
    a11y::ValueWidget* widget = nullptr;
};

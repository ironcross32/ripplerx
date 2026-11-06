#include "TypeIn.h"
#include "../Globals.h"

TypeIn::TypeIn()
{
    setFocusContainerType(FocusContainerType::keyboardFocusContainer);
    setVisible(false);

    addAndMakeVisible(heading);
    heading.setJustificationType(juce::Justification::centred);
    heading.setFont(juce::FontOptions(16.0f));

    addAndMakeVisible(current);
    current.setJustificationType(juce::Justification::centred);
    current.setFont(juce::FontOptions(14.0f));

    addAndMakeVisible(editor);
    editor.setTitle("New value");
    editor.setJustification(juce::Justification::centred);
    editor.setFont(juce::FontOptions(16.0f));
    editor.setSelectAllWhenFocused(true);
    editor.onReturnKey = [this] { apply(); };
    editor.onEscapeKey = [this] { close(); };
}

void TypeIn::show(juce::Component& targetComponent, a11y::ValueWidget& valueWidget)
{
    target = &targetComponent;
    widget = &valueWidget;

    auto title = widget->getValueTitle();
    setTitle("Enter value for " + title);
    heading.setText(title, juce::dontSendNotification);
    current.setText("Current: " + widget->getValueText(), juce::dontSendNotification);
    editor.setText(widget->getValueText(), false);

    auto bg = juce::Colour(globals::COLOR_BACKGROUND);
    auto fg = juce::Colour(globals::COLOR_NEUTRAL);
    heading.setColour(juce::Label::textColourId, fg);
    current.setColour(juce::Label::textColourId, fg);
    editor.setColour(juce::TextEditor::backgroundColourId, bg);
    editor.setColour(juce::TextEditor::textColourId, fg);
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(globals::COLOR_ACTIVE));
    editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(globals::COLOR_ACTIVE));
    editor.setColour(juce::CaretComponent::caretColourId, fg);

    if (auto* parent = getParentComponent())
        setBounds(parent->getLocalBounds());

    setVisible(true);
    toFront(false);
    editor.grabKeyboardFocus();
    editor.selectAll();
}

void TypeIn::close()
{
    if (!isVisible())
        return;

    setVisible(false);
    widget = nullptr;

    if (target != nullptr && target->isShowing())
        target->grabKeyboardFocus();
    target = nullptr;
}

void TypeIn::apply()
{
    if (target == nullptr || widget == nullptr) {
        close();
        return;
    }

    if (!widget->setValueFromText(editor.getText())) {
        a11y::announce("Invalid value");
        editor.selectAll();
        return;
    }

    close();
}

juce::Rectangle<int> TypeIn::getBoxBounds() const
{
    return getLocalBounds().withSizeKeepingCentre(260, 110);
}

void TypeIn::resized()
{
    auto box = getBoxBounds().reduced(10);
    heading.setBounds(box.removeFromTop(24));
    current.setBounds(box.removeFromTop(22));
    box.removeFromTop(6);
    editor.setBounds(box.removeFromTop(28));
}

void TypeIn::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.3f));
    auto box = getBoxBounds().toFloat();
    g.setColour(juce::Colour(globals::COLOR_BACKGROUND));
    g.fillRoundedRectangle(box, 4.0f);
    g.setColour(juce::Colour(globals::COLOR_ACTIVE));
    g.drawRoundedRectangle(box.reduced(0.5f), 4.0f, 1.0f);
}

void TypeIn::mouseDown(const juce::MouseEvent& e)
{
    if (!getBoxBounds().contains(e.getPosition()))
        close();
}

std::unique_ptr<juce::AccessibilityHandler> TypeIn::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::dialogWindow);
}

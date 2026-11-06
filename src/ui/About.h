#pragma once

#include <JuceHeader.h>

class RipplerXAudioProcessor;

class About : public juce::Component {
public:
    About();
    ~About() override {}

    HyperlinkButton githublink;
    TextButton closeButton;
    TextEditor helpText;

    // called when the overlay closes, so the editor can restore keyboard focus
    std::function<void()> onClose;

    void show();
    void hide();

    void mouseDown(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void paint(Graphics& g) override;
    void resized() override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    static juce::String getHelpText();
};

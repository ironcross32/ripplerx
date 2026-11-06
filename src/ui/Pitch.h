#pragma once

#include <JuceHeader.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "AccessibilityHelpers.h"

class RipplerXAudioProcessor;

class Pitch : public juce::Component, public a11y::ValueWidget, private juce::AudioProcessorValueTreeState::Listener {
public:
    Pitch(RipplerXAudioProcessor& p, juce::String name, juce::String coarseParamId, juce::String fineParamId);
    ~Pitch() override;
    void paint(juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void applyChange(float change);

    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // a11y::ValueWidget
    juce::String getValueTitle() const override;
    juce::String getValueText() const override;
    double getNormalisedValue() const override;
    void setNormalisedValue(double v) override;
    bool setValueFromText(const juce::String& text) override;
    void showAccessibleMenu() override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

protected:
    juce::String coarse_param_id;
    juce::String fine_param_id;
    RipplerXAudioProcessor& audioProcessor;

private:
    bool hasFine() const { return fine_param_id.isNotEmpty(); }
    // total value in cents (or plain units when there is no fine param)
    int getTotal() const;
    void setTotal(int total);
    juce::Range<int> getTotalRange() const;
    void resetToDefault();

    bool is_coarse = false;
    float cur_coarse = 0.0f;
    float cur_fine = 0.0f;
    float pitch = 0.0f;
    juce::Point<int> last_mouse_pos;
    juce::Point<int> start_mouse_pos;
};

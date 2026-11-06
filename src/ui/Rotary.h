#pragma once

#include <JuceHeader.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>
#include "AccessibilityHelpers.h"

class RipplerXAudioProcessor;

enum LabelFormat {
    Hz,
    dB,
    Percent,
    float1,
    float2_100,
    millis,
    seconds2f,
    ABMix,
    FilterLPHP,
    PitchSemis,
};

class Rotary : public juce::SettableTooltipClient, public juce::Component, public a11y::ValueWidget, private juce::AudioProcessorValueTreeState::Listener {
public:
    Rotary(RipplerXAudioProcessor& p, juce::String paramId, juce::String name, LabelFormat format, juce::String velId = "", bool isSymmetric = false);
    ~Rotary() override;
    void paint(juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // call when the processor's velocity map mode changes
    void velMapChanged();

    // a11y::ValueWidget
    juce::String getValueTitle() const override;
    juce::String getValueText() const override;
    double getNormalisedValue() const override;
    void setNormalisedValue(double v) override;
    bool setValueFromText(const juce::String& text) override;
    void showAccessibleMenu() override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    // formats a plain (denormalised) value the way the knob label shows it
    juce::String formatValue(float plainValue) const;
    static juce::String formatVelocity(float velValue);

protected:
    juce::String paramId;
    juce::String velId;
    juce::String name;
    LabelFormat format;
    RipplerXAudioProcessor& audioProcessor;

private:
    bool editingVel = false;
    bool isSymmetric;
    float deg130 = 130.0f * juce::MathConstants<float>::pi / 180.0f;
    void draw_rotary_slider(juce::Graphics& g, float slider_pos);
    void draw_vel_arc(juce::Graphics& g, float slider_pos, float vel_pos) const;
    void draw_label(juce::Graphics& g, float slider_val, float vel_val);

    // true when keyboard / screen reader edits target the velocity amount
    bool targetsVelocity(bool shiftHeld = false) const;
    juce::RangedAudioParameter* getTargetParam(bool shiftHeld = false) const;
    std::optional<float> parseValue(const juce::String& text) const;
    void stepValue(juce::RangedAudioParameter* param, float normDelta);
    void setParamNormalised(juce::RangedAudioParameter* param, float norm);

    float pixels_per_percent{100.0f};
    float cur_normed_value{0.0f};
    juce::Point<int> last_mouse_position;
    juce::Point<int> start_mouse_pos;
    bool mouse_down = false;
    bool mouse_down_shift = false;
};

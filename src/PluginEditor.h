/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/Rotary.h"
#include "ui/CustomLookAndFeel.h"
#include "ui/Meter.h"
#include "ui/Pitch.h"
#include "ui/TensionCtrl.h"
#include "ui/About.h"
#include "ui/AccessibilityHelpers.h"
#include "ui/TypeIn.h"

using namespace globals;
//==============================================================================
/**
*/
class RipplerXAudioProcessorEditor : public juce::AudioProcessorEditor, public a11y::TypeInHost, private juce::AudioProcessorValueTreeState::Listener, private juce::FocusChangeListener
{
public:
    RipplerXAudioProcessorEditor (RipplerXAudioProcessor&);
    ~RipplerXAudioProcessorEditor() override;

    //==============================================================================
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void toggleUIComponents ();
    void loadTheme();
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void repaintVelSliders ();
    void showSettingsMenu();
    void showMalletMenu();
    void showModelMenu(bool AorB);
    void showPartialsMenu(bool AorB);
    void drawGear(Graphics& g, Rectangle<int> bounds, float radius, int segs, Colour color, Colour background);

    // a11y::TypeInHost
    void showTypeIn(juce::Component& target, a11y::ValueWidget& widget) override;

private:
    RipplerXAudioProcessor& audioProcessor;
    CustomLookAndFeel* customLookAndFeel = nullptr;

    // Screen reader sections, these also define the Tab order between sections
    a11y::SectionGroup headerGroup { "Header" };
    a11y::SectionGroup noiseGroup { "Noise" };
    a11y::SectionGroup malletGroup { "Mallet" };
    a11y::SectionGroup resAGroup { "Resonator A" };
    a11y::SectionGroup resBGroup { "Resonator B" };
    a11y::SectionGroup mixGroup { "Mix and Output" };

    // Choice menus in the order they are displayed: { menu item id, label }
    struct MenuChoice { int id; const char* label; };
    static const std::vector<MenuChoice> modelChoices;
    static const std::vector<MenuChoice> partialsChoices;
    static const std::vector<MenuChoice> malletChoices;
    void jogChoice(const juce::String& paramId, const std::vector<MenuChoice>& choices, int direction);
    void setupMenuButton(a11y::MenuButton& button, const juce::String& title, const juce::String& paramId);
    void velMapChanged();
    bool jumpToGroup(int direction); // Alt+, / Alt+. : first control of the previous / next section
    void globalFocusChanged(juce::Component* focusedComponent) override;
    std::vector<Rotary*> getRotaries();

    Label noiseLabel;
    Label envelopeLabel;
    Label malletLabel;
    Label malletSubLabel;
    a11y::MenuButton malletButton;
    std::unique_ptr<About> about;

    // Top bar
    ImageButton logo;
    ImageButton sun;
    ImageButton moon;
    Label polyLabel;
    ComboBox polyMenu;
    TextButton velButton;
    ComboBox presetMenu;
    TextButton settingsBtn;
#if defined(DEBUG)
    juce::TextButton presetExport;
#endif

    // Noise
    ComboBox noiseFilterMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noiseFilterAttachment;
    a11y::BarSlider noiseOSC;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseDCAttachment;
    std::unique_ptr<Rotary> noiseMix;
    std::unique_ptr<Rotary> noiseRes;
    std::unique_ptr<Rotary> noiseFreq;
    std::unique_ptr<Rotary> noiseQ;
    std::unique_ptr<Rotary> noiseA;
    std::unique_ptr<Rotary> noiseD;
    std::unique_ptr<Rotary> noiseS;
    std::unique_ptr<Rotary> noiseR;
    std::unique_ptr<TensionCtrl> noiseATen;
    std::unique_ptr<TensionCtrl> noiseDTen;
    std::unique_ptr<TensionCtrl> noiseRTen;

    // Mallet
    std::unique_ptr<Rotary> malletMix;
    std::unique_ptr<Rotary> malletRes;
    std::unique_ptr<Rotary> malletStiff;
    std::unique_ptr<Rotary> malletPitch;
    std::unique_ptr<Rotary> malletFilter;
    a11y::BarSlider ktrackSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ktrackAttachment;

    // Res A
    Label aLabel;
    ImageButton aOn;
    ComboBox aModel;
    a11y::MenuButton aModelBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aModelAttachment;
    ComboBox aPartials;
    a11y::MenuButton aPartialsBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aPartialsAttachment;
    std::unique_ptr<Rotary> aDecay;
    std::unique_ptr<Rotary> aDamp;
    std::unique_ptr<Rotary> aTone;
    std::unique_ptr<Rotary> aHit;
    std::unique_ptr<Rotary> aRel;
    std::unique_ptr<Rotary> aInharm;
    std::unique_ptr<Rotary> aRatio;
    std::unique_ptr<Rotary> aCut;
    std::unique_ptr<Rotary> aRadius;

    // Res B
    Label bLabel;
    ImageButton bOn;
    ComboBox bModel;
    a11y::MenuButton bModelBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bModelAttachment;
    ComboBox bPartials;
    a11y::MenuButton bPartialsBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bPartialsAttachment;
    std::unique_ptr<Rotary> bDecay;
    std::unique_ptr<Rotary> bDamp;
    std::unique_ptr<Rotary> bTone;
    std::unique_ptr<Rotary> bHit;
    std::unique_ptr<Rotary> bRel;
    std::unique_ptr<Rotary> bInharm;
    std::unique_ptr<Rotary> bRatio;
    std::unique_ptr<Rotary> bCut;
    std::unique_ptr<Rotary> bRadius;

    // Coupling
    ComboBox couple;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> coupleAttachment;
    std::unique_ptr<Rotary> abMix;
    std::unique_ptr<Rotary> abSplit;
    std::unique_ptr<Pitch>aPitch;
    std::unique_ptr<Pitch>bPitch;
    Label pitchLabel;
    Label bendLabel;
    std::unique_ptr<Pitch>bendPitch;
    std::unique_ptr<Rotary> gain;
    std::unique_ptr<Meter> meter;

    TooltipWindow tooltipWindow;
    juce::MidiKeyboardComponent keyboardComponent;
    TypeIn typeIn;

    static constexpr const char* patchExtension = "*.ripx";
    static constexpr const char* audioExtension = "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3";
    static constexpr const char* exportWindowTitle = "Export patch";
    static constexpr const char* importWindowTitle = "Import patch";
    static constexpr const char* importAudioTitle = "Import audio file";
    std::unique_ptr<juce::FileChooser> mFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RipplerXAudioProcessorEditor)
};

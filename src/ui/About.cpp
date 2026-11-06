#include "About.h"

About::About()
{
	setFocusContainerType(FocusContainerType::keyboardFocusContainer);
	setWantsKeyboardFocus(false);

	addAndMakeVisible(helpText);
	helpText.setTitle("Help");
	helpText.setMultiLine(true, false);
	helpText.setReadOnly(true);
	helpText.setCaretVisible(false);
	helpText.setScrollbarsShown(false);
	helpText.setFont(FontOptions(16.0f));
	helpText.setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
	helpText.setColour(TextEditor::outlineColourId, Colours::transparentBlack);
	helpText.setColour(TextEditor::focusedOutlineColourId, Colours::white.withAlpha(0.4f));
	helpText.setColour(TextEditor::textColourId, Colours::white);
	helpText.setText(getHelpText(), false);
	helpText.setInterceptsMouseClicks(false, false); // clicking anywhere still closes the overlay
	helpText.onEscapeKey = [this] { hide(); };

	addAndMakeVisible(githublink);
	githublink.setURL(URL("https://github.com/tiagolr/ripplerx"));
	githublink.setButtonText("github.com/tiagolr/ripplerx");
	githublink.setTitle("Open github.com/tiagolr/ripplerx");

	addAndMakeVisible(closeButton);
	closeButton.setButtonText("Close");
	closeButton.onClick = [this] { hide(); };

	helpText.setExplicitFocusOrder(1);
	githublink.setExplicitFocusOrder(2);
	closeButton.setExplicitFocusOrder(3);
}

juce::String About::getHelpText()
{
	return "- Ctrl for fine slider adjustments.\n"
		"- Shift to adjust velocity mapping.\n"
		"- Double click to reset sliders.\n"
		"- Keyboard: arrows adjust, Ctrl+arrows fine, Page Up/Down coarse,\n"
		"  Home/End max/min, Delete resets, Enter types a value.\n"
		"- Rippler supports Audio In:\n"
		"  Depending on the DAW create an audio routing into the synth.\n"
		"  Play the audio and play notes, the sound should excite the resonators.\n"
		"  Check the github link for more details.";
}

void About::show()
{
	setVisible(true);
	toFront(false);
	helpText.grabKeyboardFocus();
}

void About::hide()
{
	if (!isVisible())
		return;
	setVisible(false);
	if (onClose)
		onClose();
}

void About::mouseDown(const juce::MouseEvent& e)
{
	(void)e;
	hide();
};

bool About::keyPressed(const juce::KeyPress& key)
{
	if (key == KeyPress::escapeKey) {
		hide();
		return true;
	}
	return false;
}

void About::resized()
{
	auto bounds = getLocalBounds().reduced(50).reduced(20, 20);
	bounds.removeFromTop(35 + 25 + 22);
	githublink.setBounds(bounds.removeFromTop(22));
	bounds.removeFromTop(30);
	closeButton.setBounds(bounds.removeFromBottom(25).withSizeKeepingCentre(80, 25));
	helpText.setBounds(bounds);
}

void About::paint(Graphics& g)
{
	auto bounds = getLocalBounds().reduced(50);
	g.setColour(Colour(0xdd000000));
	g.fillRect(bounds);

	bounds.reduce(20,20);
	g.setColour(Colours::white);
	g.setFont(FontOptions(30.f));
	g.drawText("RipplerX", bounds.removeFromTop(35), Justification::centred);
	g.setFont(FontOptions(20.f));
	g.drawText(std::string("v") + PROJECT_VERSION, bounds.removeFromTop(25), Justification::centred);
	g.setFont(FontOptions(16.0f));
	g.drawText("Copyright (C) Tilr 2025", bounds.removeFromTop(22), Justification::centred);
};

std::unique_ptr<juce::AccessibilityHandler> About::createAccessibilityHandler()
{
	struct Handler : public juce::AccessibilityHandler
	{
		explicit Handler(About& a) : juce::AccessibilityHandler(a, juce::AccessibilityRole::dialogWindow) {}
		juce::String getTitle() const override { return juce::String("About RipplerX v") + PROJECT_VERSION; }
		juce::String getDescription() const override { return "Copyright (C) Tilr 2025"; }
	};
	return std::make_unique<Handler>(*this);
}

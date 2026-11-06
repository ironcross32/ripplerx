#include "TensionCtrl.h"
#include "../PluginProcessor.h"
#include "../Globals.h"

TensionCtrl::TensionCtrl(RipplerXAudioProcessor& p, juce::String paramId, bool invert) 
    : juce::Component()
    , audioProcessor(p)
    , paramId(paramId)
    , invert(invert)
{
    setWantsKeyboardFocus(true);
    audioProcessor.params.addParameterListener(paramId, this);
}

TensionCtrl::~TensionCtrl()
{
    audioProcessor.params.removeParameterListener(paramId, this);
}

void TensionCtrl::parameterChanged(const juce::String& parameterID, float newValue)
{
    (void)parameterID;
    (void)newValue;
    juce::Component::SafePointer<TensionCtrl> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
        if (safeThis == nullptr) return;
        safeThis->repaint();
        a11y::notifyValue(*safeThis);
    });
}

void TensionCtrl::mouseEnter(const MouseEvent& e)
{
    (void)e;
    mouseOver = true;
    repaint();
};

void TensionCtrl::mouseExit(const MouseEvent& e)
{
    (void)e;
    mouseOver = false;
    repaint();
};


void TensionCtrl::paint(juce::Graphics& g) 
{
    auto bounds = getLocalBounds();
    
    g.setColour(Colours::black.withAlpha(0.1f));
    g.fillRect(bounds);

    g.setColour(Colour(globals::COLOR_NEUTRAL_LIGHT));
    auto val = audioProcessor.params.getRawParameterValue(paramId)->load();
    val = (val + 1.f) * 0.5f; // normalize between 1 and 0
    auto bl = bounds.getBottomLeft().toFloat();
    auto br = bounds.getBottomRight().toFloat();
    auto tl = bounds.getTopLeft().toFloat();
    auto tr = bounds.getTopRight().toFloat();

    Path path;

    if (invert) {
        float cx = bl.x + (tr.x - tl.x) * (1.f - val);
        float cy = br.y - (br.y - tl.y) * (1.f - val);
        path.startNewSubPath(tl.x, tl.y);
        path.quadraticTo(cx, cy, br.x, br.y);
    }
    else {
        float cx = tl.x + (br.x - tl.x) * val;
        float cy = tl.y + (br.y - tl.y) * val;
        path.startNewSubPath(bl.x, bl.y);
        path.quadraticTo(cx, cy, tr.x, tr.y);
    }
    
    g.strokePath(path, PathStrokeType(1.0f));

    if (hasKeyboardFocus(false)) {
        g.setColour(Colour(globals::COLOR_ACTIVE));
        g.drawRect(bounds, 1);
    }
}

void TensionCtrl::mouseDown(const juce::MouseEvent& e)
{
    e.source.enableUnboundedMouseMovement(true);
    mouse_down = true;
    mouse_down_shift = e.mods.isShiftDown();
    auto param = audioProcessor.params.getParameter(paramId);
    auto cur_val = param->getValue();
    cur_normed_value = cur_val;
    last_mouse_position = e.getPosition();
    setMouseCursor(MouseCursor::NoCursor);
    start_mouse_pos = Desktop::getInstance().getMousePosition();
    repaint();
    param->beginChangeGesture();
}

void TensionCtrl::mouseUp(const juce::MouseEvent& e) {
    mouse_down = false;
    setMouseCursor(MouseCursor::NormalCursor);
    e.source.enableUnboundedMouseMovement(false);
    Desktop::getInstance().setMousePosition(start_mouse_pos);
    repaint();
    auto param = audioProcessor.params.getParameter(paramId);
    param->endChangeGesture();
}

void TensionCtrl::mouseDoubleClick(const juce::MouseEvent& e) {
    (void)e;
    auto param = audioProcessor.params.getParameter(paramId);
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->getDefaultValue());
    param->endChangeGesture();
}

void TensionCtrl::mouseDrag(const juce::MouseEvent& e) {
    auto change = e.getPosition() - last_mouse_position;
    last_mouse_position = e.getPosition();
    auto speed = (e.mods.isCtrlDown() ? 40.0f : 4.0f) * pixels_per_percent;
    auto slider_change = float(change.getX() - change.getY()) / speed;
    cur_normed_value -= slider_change;
    auto param = audioProcessor.params.getParameter(paramId);
    param->setValueNotifyingHost(cur_normed_value);
}
//==============================================================================
// Keyboard and screen reader support

static void setTensionNormalised(juce::RangedAudioParameter* param, float norm)
{
    param->beginChangeGesture();
    param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
    param->endChangeGesture();
}

bool TensionCtrl::keyPressed(const juce::KeyPress& key)
{
    auto ek = a11y::getEditKey(key);
    auto param = audioProcessor.params.getParameter(paramId);
    auto norm = param->getValue();
    // the range is -1..1, so 0.005 normalised is 1 %
    auto step = ek.fine ? 0.0005f : 0.005f;

    switch (ek.action) {
        case a11y::EditAction::Increase: setTensionNormalised(param, norm + step); break;
        case a11y::EditAction::Decrease: setTensionNormalised(param, norm - step); break;
        case a11y::EditAction::IncreaseLarge: setTensionNormalised(param, norm + 0.05f); break;
        case a11y::EditAction::DecreaseLarge: setTensionNormalised(param, norm - 0.05f); break;
        case a11y::EditAction::ToMax: setTensionNormalised(param, 1.0f); break;
        case a11y::EditAction::ToMin: setTensionNormalised(param, 0.0f); break;
        case a11y::EditAction::ToDefault: setTensionNormalised(param, param->getDefaultValue()); break;
        case a11y::EditAction::TypeIn: a11y::openTypeIn(*this, *this); break;
        case a11y::EditAction::Menu: showAccessibleMenu(); break;
        case a11y::EditAction::None: return false;
    }
    return true;
}

juce::String TensionCtrl::getValueTitle() const
{
    return getTitle().isNotEmpty() ? getTitle() : juce::String("Curve");
}

juce::String TensionCtrl::getValueText() const
{
    auto val = audioProcessor.params.getRawParameterValue(paramId)->load();
    return juce::String((int)std::round(val * 100.0f)) + " %";
}

double TensionCtrl::getNormalisedValue() const
{
    return audioProcessor.params.getParameter(paramId)->getValue();
}

void TensionCtrl::setNormalisedValue(double v)
{
    setTensionNormalised(audioProcessor.params.getParameter(paramId), (float)v);
}

bool TensionCtrl::setValueFromText(const juce::String& text)
{
    auto number = a11y::parseNumber(text);
    if (!number)
        return false;
    auto param = audioProcessor.params.getParameter(paramId);
    auto plain = juce::jlimit(-1.0f, 1.0f, (float)(*number / 100.0));
    setTensionNormalised(param, param->convertTo0to1(plain));
    return true;
}

void TensionCtrl::showAccessibleMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader(getValueTitle());
    menu.addItem(1, "Enter value...");
    menu.addItem(2, "Reset to default");

    juce::Component::SafePointer<TensionCtrl> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [safeThis](int result) {
            if (safeThis == nullptr) return;
            if (result != 1) a11y::refocusAsync(*safeThis); // 1 opens the type-in, which takes focus
            if (result == 0) return;
            if (result == 1) a11y::openTypeIn(*safeThis, *safeThis);
            if (result == 2) {
                auto param = safeThis->audioProcessor.params.getParameter(safeThis->paramId);
                setTensionNormalised(param, param->getDefaultValue());
            }
        });
}

std::unique_ptr<juce::AccessibilityHandler> TensionCtrl::createAccessibilityHandler()
{
    return std::make_unique<a11y::ValueWidgetHandler>(*this);
}

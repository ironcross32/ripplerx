#include "Pitch.h"
#include "../PluginProcessor.h"
#include "../Globals.h"

Pitch::Pitch(RipplerXAudioProcessor& p, juce::String name, juce::String coarse_param_id, juce::String fine_param_id)
    : juce::Component()
    , audioProcessor(p)
    , coarse_param_id(coarse_param_id)
    , fine_param_id(fine_param_id)
{
    setName(name);
    setWantsKeyboardFocus(true);
    audioProcessor.params.addParameterListener(coarse_param_id, this);
    if (fine_param_id.isNotEmpty()) {
        audioProcessor.params.addParameterListener(fine_param_id, this);
    }
}

Pitch::~Pitch()
{
    audioProcessor.params.removeParameterListener(coarse_param_id, this);
    if (fine_param_id.isNotEmpty()) {
        audioProcessor.params.removeParameterListener(fine_param_id, this);
    }
}

void Pitch::parameterChanged(const juce::String& parameterID, float newValue) 
{
    (void)parameterID;
    (void)newValue;
    juce::Component::SafePointer<Pitch> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
        if (safeThis == nullptr) return;
        safeThis->repaint();
        a11y::notifyValue(*safeThis);
    });
}

void Pitch::paint(juce::Graphics& g) {
    g.fillAll(Colour(globals::COLOR_BACKGROUND));

    auto param = audioProcessor.params.getParameter(coarse_param_id);
    auto norm = param->getValue();
    auto coarse_val = param->convertFrom0to1(norm);

    float fine_val = 0.f;
    if (fine_param_id.isNotEmpty()) {
        param = audioProcessor.params.getParameter(fine_param_id);
        norm = param->getValue();
        fine_val = param->convertFrom0to1(norm);
    }

    auto isDark = audioProcessor.darkTheme;
    g.setColour(Colour(globals::COLOR_ACTIVE));
    g.fillRoundedRectangle(0.f, 0.f, (float)getWidth(), (float)getHeight(), 2.f);
    g.setFont(FontOptions(15.0f));
    g.setColour(isDark ? Colour(globals::COLOR_BACKGROUND).darker(0.7f) : Colours::white);
    auto sign = coarse_val < 0 || fine_val < 0 ? -1.f : 1.f;
    std::stringstream ss;

    if (fine_param_id.isNotEmpty()) {
        ss << sign * fabs(coarse_val) << "." << std::setw(2) << std::setfill('0') << fabs(fine_val);
        auto text = ss.str();
        g.drawText(text, 0, 0, getWidth() - 15, getHeight(), Justification::centredRight, false);
    }
    else {
        ss << sign * fabs(coarse_val);
        auto text = ss.str();
        g.drawText(text, 0, 0, getWidth(), getHeight(), Justification::centred, false);
    }

    if (hasKeyboardFocus(false))
        a11y::drawFocusRing(g, getLocalBounds().toFloat(), Colour(globals::COLOR_NEUTRAL));
}

void Pitch::mouseDown(const juce::MouseEvent& e) 
{
    e.source.enableUnboundedMouseMovement(true);
    is_coarse = fine_param_id.isEmpty() || e.getMouseDownX() < getWidth() / 2.f;
    last_mouse_pos = e.getPosition();
    auto param = audioProcessor.params.getParameter(coarse_param_id);
    auto norm = param->getValue();
    auto val = param->convertFrom0to1(norm);
    cur_coarse = val;
    if (fine_param_id.isNotEmpty()) {
        param = audioProcessor.params.getParameter(fine_param_id);
        norm = param->getValue();
        val = param->convertFrom0to1(norm);
        cur_fine = val;
    }
    setMouseCursor(MouseCursor::NoCursor);
    start_mouse_pos = Desktop::getInstance().getMousePosition();
}

void Pitch::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    is_coarse = fine_param_id.isEmpty() || event.getMouseDownX() < getWidth() / 2.f;
    auto slider_change = wheel.deltaY > 0 ? 1.0f : wheel.deltaY < 0 ? -1.0f : 0;
    applyChange(slider_change);
}

void Pitch::mouseUp(const juce::MouseEvent& e) {
    setMouseCursor(MouseCursor::NormalCursor);
    e.source.enableUnboundedMouseMovement(false);
    Desktop::getInstance().setMousePosition(start_mouse_pos);
}

void Pitch::mouseDoubleClick(const juce::MouseEvent& e) {
    (void)e;
    auto param = audioProcessor.params.getParameter(is_coarse ? coarse_param_id : fine_param_id);
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->getDefaultValue());
    param->endChangeGesture();
}

void Pitch::mouseDrag(const juce::MouseEvent& e) {
    auto change = e.getPosition() - last_mouse_pos;
    last_mouse_pos = e.getPosition();
    auto speed = (e.mods.isCtrlDown() ? 40.0f : 4.0f) * 10.0f;
    auto slider_change = float(-change.getY()) / speed;
    applyChange(slider_change);
}

void Pitch::applyChange(float change)
{
    if (is_coarse)
        cur_coarse += change;
    else
        cur_fine += change;

    if (fine_param_id.isEmpty()) {
        auto param = audioProcessor.params.getParameter(coarse_param_id);
        auto norm = param->convertTo0to1(cur_coarse);
        param->beginChangeGesture();
        param->setValueNotifyingHost(norm);
        param->endChangeGesture();
        return;
    }

    if (!is_coarse && (
        (cur_fine >= 100.f) || 
        (cur_fine <= -100.f) || 
        (std::trunc(cur_coarse) < 0.f && cur_fine >= 1.f) || 
        (std::trunc(cur_coarse) > 0.f && cur_fine < 0))) 
    {
        cur_coarse = std::trunc(cur_coarse);
        if (cur_fine >= 100.f) {
            cur_coarse += 1.f;
            cur_fine = 0.f;
        }
        else if (cur_fine <= -100.f) {
            cur_coarse -= 1.f;
            cur_fine = 0.f;
        }
        else if (cur_coarse > 0.f) {
            cur_coarse += cur_fine >= 100.f ? 1.f : -1.f;
            cur_fine = cur_fine < 0.f ? 99.f : 0.f;
        } else {
            cur_coarse += cur_fine <= -100.f ? -1.f : 1.f;
            cur_fine = cur_fine < -99.f ? 0.f : -99.f;
        }

        last_mouse_pos = getMouseXYRelative();
        auto param = audioProcessor.params.getParameter(coarse_param_id);
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(cur_coarse));
        param->endChangeGesture();
    }

    auto param = audioProcessor.params.getParameter(is_coarse ? coarse_param_id : fine_param_id);
    auto norm = param->convertTo0to1(std::trunc(is_coarse ? cur_coarse : cur_fine));
    param->beginChangeGesture();
    param->setValueNotifyingHost(norm);
    param->endChangeGesture();
}
//==============================================================================
// Keyboard and screen reader support. The value is handled as a single number:
// total cents (coarse * 100 + fine) for tuning fields, or plain semitones for the
// bend range field.

int Pitch::getTotal() const
{
    auto coarse = (int)std::round(audioProcessor.params.getParameter(coarse_param_id)->convertFrom0to1(
        audioProcessor.params.getParameter(coarse_param_id)->getValue()));
    if (!hasFine())
        return coarse;
    auto fineParam = audioProcessor.params.getParameter(fine_param_id);
    auto fine = (int)std::round(fineParam->convertFrom0to1(fineParam->getValue()));
    return coarse * 100 + fine;
}

juce::Range<int> Pitch::getTotalRange() const
{
    const auto& coarseRange = audioProcessor.params.getParameter(coarse_param_id)->getNormalisableRange();
    if (!hasFine())
        return { (int)coarseRange.start, (int)coarseRange.end };
    const auto& fineRange = audioProcessor.params.getParameter(fine_param_id)->getNormalisableRange();
    return { (int)coarseRange.start * 100 + (int)fineRange.start, (int)coarseRange.end * 100 + (int)fineRange.end };
}

void Pitch::setTotal(int total)
{
    auto range = getTotalRange();
    total = juce::jlimit(range.getStart(), range.getEnd(), total);

    auto setParam = [this](const juce::String& id, float plain) {
        auto param = audioProcessor.params.getParameter(id);
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(plain));
        param->endChangeGesture();
    };

    if (!hasFine()) {
        setParam(coarse_param_id, (float)total);
        return;
    }

    // coarse and fine share the same sign, e.g. -325 cents -> -3 coarse, -25 fine
    int coarse = total / 100;
    int fine = total - coarse * 100;
    setParam(coarse_param_id, (float)coarse);
    setParam(fine_param_id, (float)fine);
}

void Pitch::resetToDefault()
{
    for (auto& id : { coarse_param_id, fine_param_id }) {
        if (id.isEmpty()) continue;
        auto param = audioProcessor.params.getParameter(id);
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->getDefaultValue());
        param->endChangeGesture();
    }
}

bool Pitch::keyPressed(const juce::KeyPress& key)
{
    auto ek = a11y::getEditKey(key);
    const int unit = hasFine() ? 100 : 1;
    const int fine = hasFine() && ek.fine ? 1 : unit;
    const int large = hasFine() ? 1200 : 6;
    auto total = getTotal();
    auto range = getTotalRange();

    switch (ek.action) {
        case a11y::EditAction::Increase: setTotal(total + fine); break;
        case a11y::EditAction::Decrease: setTotal(total - fine); break;
        case a11y::EditAction::IncreaseLarge: setTotal(total + large); break;
        case a11y::EditAction::DecreaseLarge: setTotal(total - large); break;
        case a11y::EditAction::ToMax: setTotal(range.getEnd()); break;
        case a11y::EditAction::ToMin: setTotal(range.getStart()); break;
        case a11y::EditAction::ToDefault: resetToDefault(); break;
        case a11y::EditAction::TypeIn: a11y::openTypeIn(*this, *this); break;
        case a11y::EditAction::Menu: showAccessibleMenu(); break;
        case a11y::EditAction::None: return false;
    }
    return true;
}

juce::String Pitch::getValueTitle() const
{
    return getTitle().isNotEmpty() ? getTitle() : getName();
}

juce::String Pitch::getValueText() const
{
    auto total = getTotal();
    if (!hasFine())
        return juce::String(total) + (std::abs(total) == 1 ? " semitone" : " semitones");

    if (total == 0)
        return "0 semitones";

    auto semis = std::abs(total) / 100;
    auto cents = std::abs(total) % 100;
    juce::String text = total < 0 ? "-" : "";
    if (semis != 0)
        text << semis << (semis == 1 ? " semitone" : " semitones");
    if (cents != 0)
        text << (semis != 0 ? " " : "") << cents << (cents == 1 ? " cent" : " cents");
    return text;
}

double Pitch::getNormalisedValue() const
{
    auto range = getTotalRange();
    return range.getLength() > 0 ? double(getTotal() - range.getStart()) / range.getLength() : 0.0;
}

void Pitch::setNormalisedValue(double v)
{
    auto range = getTotalRange();
    setTotal(range.getStart() + (int)std::round(v * range.getLength()));
}

bool Pitch::setValueFromText(const juce::String& rawText)
{
    auto text = rawText.toLowerCase();
    auto number = a11y::parseNumber(text);
    if (!number)
        return false;

    if (!hasFine()) {
        setTotal((int)std::round(*number));
        return true;
    }

    // "25 cents" alone sets cents, "3 semitones 25 cents" sets both,
    // otherwise the number is decimal semitones (3.25 = 3 semitones 25 cents)
    const bool hasCents = text.contains("cent") || text.contains("ct");
    const bool hasSemis = text.contains("semi") || text.contains("st");
    if (hasCents && !hasSemis) {
        setTotal((int)std::round(*number));
    }
    else if (hasCents && hasSemis) {
        auto semis = (int)*number;
        auto rest = text.fromFirstOccurrenceOf("semi", false, false).fromFirstOccurrenceOf(" ", false, false);
        auto cents = a11y::parseNumber(rest);
        auto c = cents ? (int)std::round(std::abs(*cents)) : 0;
        setTotal(semis * 100 + (*number < 0 ? -c : c));
    }
    else {
        setTotal((int)std::round(*number * 100.0));
    }
    return true;
}

void Pitch::showAccessibleMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader(getValueTitle());
    menu.addItem(1, "Enter value...");
    menu.addItem(2, "Reset to default");

    juce::Component::SafePointer<Pitch> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [safeThis](int result) {
            if (safeThis == nullptr) return;
            if (result != 1) a11y::refocusAsync(*safeThis); // 1 opens the type-in, which takes focus
            if (result == 0) return;
            if (result == 1) a11y::openTypeIn(*safeThis, *safeThis);
            if (result == 2) safeThis->resetToDefault();
        });
}

std::unique_ptr<juce::AccessibilityHandler> Pitch::createAccessibilityHandler()
{
    return std::make_unique<a11y::ValueWidgetHandler>(*this);
}

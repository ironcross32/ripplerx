#include "Rotary.h"
#include "../PluginProcessor.h"
#include "../Globals.h"

Rotary::Rotary(RipplerXAudioProcessor& p, juce::String paramId, juce::String name, LabelFormat format, juce::String velId, bool isSymmetric)
    : juce::SettableTooltipClient()
    , juce::Component()
    , audioProcessor(p)
    , paramId(paramId)
    , name(name)
    , format(format)
    , velId(velId)
    , isSymmetric(isSymmetric)
{
    setName(name);
    setWantsKeyboardFocus(true);
    audioProcessor.params.addParameterListener(paramId, this);
    if (velId.isNotEmpty()) {
        audioProcessor.params.addParameterListener(velId, this);
    }
}

Rotary::~Rotary()
{
    audioProcessor.params.removeParameterListener(paramId, this);
    if (velId.isNotEmpty()) {
        audioProcessor.params.removeParameterListener(velId, this);
    }
}

void Rotary::parameterChanged(const juce::String& parameterID, float newValue)
{
    (void)parameterID;
    (void)newValue;
    juce::Component::SafePointer<Rotary> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
        if (safeThis == nullptr) return;
        safeThis->repaint();
        a11y::notifyValue(*safeThis);
    });
}

void Rotary::paint(juce::Graphics& g) {
    //g.fillAll(Colour(globals::COLOR_BACKGROUND));

    auto param = audioProcessor.params.getParameter(paramId);
    auto normValue = param->getValue();
    auto value = param->convertFrom0to1(normValue);

    draw_rotary_slider(g, normValue);

    auto velVal = -2.0f;
    if (!velId.isEmpty()) {
        auto velParam = audioProcessor.params.getParameter(velId);
        velVal = velParam->convertFrom0to1(velParam->getValue());
        draw_vel_arc(g, normValue, velVal);
    }

    draw_label(g, value, velVal);

    if (hasKeyboardFocus(false))
        a11y::drawFocusRing(g, getLocalBounds().toFloat());
}

juce::String Rotary::formatVelocity(float vel_val)
{
    return juce::String((int)std::round(vel_val * 100)) + " %";
}

juce::String Rotary::formatValue(float slider_val) const
{
    switch (format) {
        case LabelFormat::Percent:
            return juce::String((int)std::round(slider_val * 100)) + " %";
        case LabelFormat::millis:
            if (slider_val < 1000.0f)
                return juce::String((int)slider_val) + " ms";
            return juce::String(slider_val / 1000.f, 2) + " s";
        case LabelFormat::Hz:
            if (slider_val >= 1000.f)
                return juce::String(slider_val / 1000.f, 1) + " kHz";
            return juce::String((int)std::round(slider_val)) + " Hz";
        case LabelFormat::float1:
            return juce::String(slider_val, 1);
        case LabelFormat::float2_100:
            return juce::String(slider_val * 100, 2);
        case LabelFormat::seconds2f:
            return juce::String(slider_val, 2) + " s";
        case LabelFormat::ABMix:
            return juce::String((int)((1 - slider_val) * 100)) + ":" + juce::String((int)(slider_val * 100));
        case LabelFormat::dB:
            return juce::String((int)slider_val) + " dB";
        case LabelFormat::FilterLPHP: {
            if (slider_val == 0.0f)
                return "Off";
            double freq = 20.0 * std::pow(1000, slider_val < 0.0 ? 1 + slider_val : slider_val); // map 1..0 to 20..20000
            juce::String text = slider_val < 0.0 ? "LP " : "HP ";
            if (freq >= 1000.0)
                text << juce::String(freq / 1000.0, 1) << " kHz";
            else
                text << juce::String((int)std::round(freq)) << " Hz";
            return text;
        }
        case LabelFormat::PitchSemis:
            return juce::String((int)std::round(slider_val)) + " Sem";
    }
    return juce::String(slider_val);
}

void Rotary::draw_label(juce::Graphics& g, float slider_val, float vel_val)
{
    auto text = name;
    bool showValue = mouse_down || hasKeyboardFocus(false);
    if (showValue) {
        bool showVel = mouse_down ? (mouse_down_shift || audioProcessor.velMap) : audioProcessor.velMap;
        if (showVel && vel_val > -2.0f)
            text = formatVelocity(vel_val);
        else
            text = formatValue(slider_val);
    }

    bool isShowingFilter = showValue && format == LabelFormat::FilterLPHP;
    g.setColour(juce::Colour(globals::COLOR_NEUTRAL));
    g.setFont(isShowingFilter ? 14.f : 15.0f);
    g.drawText(text, 0, getHeight() - 16, getWidth(), 16, juce::Justification::centred, true);
    //g.drawRect(getLocalBounds());
}

void Rotary::mouseDown(const juce::MouseEvent& e)
{
    e.source.enableUnboundedMouseMovement(true);
    mouse_down = true;
    mouse_down_shift = e.mods.isShiftDown();
    editingVel = (mouse_down_shift || audioProcessor.velMap) && velId.isNotEmpty();
    auto param = audioProcessor.params.getParameter(editingVel ? velId : paramId);
    auto cur_val = param->getValue();
    cur_normed_value = cur_val;
    last_mouse_position = e.getPosition();
    setMouseCursor(MouseCursor::NoCursor);
    start_mouse_pos = Desktop::getInstance().getMousePosition();
    repaint();
    param->beginChangeGesture();
}

void Rotary::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isLeftButtonDown() || event.mods.isRightButtonDown()) {
        return; // prevent crash, param is already mutating
    }
    auto speed = (event.mods.isCtrlDown() ? 0.01f : 0.05f);
    auto slider_change = wheel.deltaY > 0 ? speed : wheel.deltaY < 0 ? -speed : 0;
    auto param = audioProcessor.params.getParameter(editingVel ? velId : paramId);
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->getValue() + slider_change);
    while (wheel.deltaY > 0.0f && param->getValue() == 0.0f) { // FIX wheel not working when value is zero, first step takes more than 0.05%
        slider_change += 0.05f;
        param->setValueNotifyingHost(param->getValue() + slider_change);
    }
    param->endChangeGesture();
}

void Rotary::mouseUp(const juce::MouseEvent& e) {
    mouse_down = false;
    setMouseCursor(MouseCursor::NormalCursor);
    e.source.enableUnboundedMouseMovement(false);
    Desktop::getInstance().setMousePosition(start_mouse_pos);
    repaint();
    auto param = audioProcessor.params.getParameter(editingVel ? velId : paramId);
    param->endChangeGesture();
}

void Rotary::mouseDoubleClick(const juce::MouseEvent& e) {
    auto param = audioProcessor.params.getParameter((e.mods.isShiftDown() || audioProcessor.velMap) && velId.isNotEmpty() ? velId : paramId);
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->getDefaultValue());
    param->endChangeGesture();
}

void Rotary::mouseDrag(const juce::MouseEvent& e) {
    auto change = e.getPosition() - last_mouse_position;
    last_mouse_position = e.getPosition();
    auto speed = (e.mods.isCtrlDown() ? 40.0f : 4.0f) * pixels_per_percent;
    auto slider_change = float(change.getX() - change.getY()) / speed;
    cur_normed_value += slider_change;
    auto param = audioProcessor.params.getParameter((mouse_down_shift || audioProcessor.velMap) && velId.isNotEmpty() ? velId : paramId);

    if (format == LabelFormat::PitchSemis && !e.mods.isCtrlDown()) {
        // snap values for pitch knob
        auto val = param->convertFrom0to1(cur_normed_value);
        param->setValueNotifyingHost(param->convertTo0to1(std::round(val)));
    }
    else {
        param->setValueNotifyingHost(cur_normed_value);
    }
}

//==============================================================================
// Keyboard and screen reader support

bool Rotary::targetsVelocity(bool shiftHeld) const
{
    return velId.isNotEmpty() && (shiftHeld || audioProcessor.velMap);
}

juce::RangedAudioParameter* Rotary::getTargetParam(bool shiftHeld) const
{
    return audioProcessor.params.getParameter(targetsVelocity(shiftHeld) ? velId : paramId);
}

void Rotary::setParamNormalised(juce::RangedAudioParameter* param, float norm)
{
    param->beginChangeGesture();
    param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
    param->endChangeGesture();
}

// steps the parameter by normDelta, making sure the snapped value actually moves
// (params with a large interval and skew may not change for small steps)
void Rotary::stepValue(juce::RangedAudioParameter* param, float normDelta)
{
    const auto& range = param->getNormalisableRange();
    const auto current = param->getValue();
    const auto currentPlain = range.convertFrom0to1(current);
    auto target = current;

    for (int i = 0; i < 1000; ++i) {
        target = juce::jlimit(0.0f, 1.0f, target + normDelta);
        auto plain = range.snapToLegalValue(range.convertFrom0to1(target));
        if (plain != currentPlain || target <= 0.0f || target >= 1.0f)
            break;
    }

    setParamNormalised(param, target);
}

bool Rotary::keyPressed(const juce::KeyPress& key)
{
    auto ek = a11y::getEditKey(key);
    if (ek.action == a11y::EditAction::None)
        return false;

    if (ek.action == a11y::EditAction::Menu) {
        showAccessibleMenu();
        return true;
    }
    if (ek.action == a11y::EditAction::TypeIn) {
        a11y::openTypeIn(*this, *this);
        return true;
    }

    const bool vel = targetsVelocity(ek.velocity);
    auto param = getTargetParam(ek.velocity);
    const auto& range = param->getNormalisableRange();

    // step sizes in normalised units
    float step = ek.fine ? 0.001f : 0.01f;
    float largeStep = 0.1f;
    if (vel) { // velocity params are -1..1, step 1% of that
        step *= 0.5f;
        largeStep *= 0.5f;
    }

    if (!vel && format == LabelFormat::PitchSemis) {
        // whole semitones, Ctrl for tenths, PageUp/Down an octave
        auto plain = range.convertFrom0to1(param->getValue());
        float delta = 0.0f;
        switch (ek.action) {
            case a11y::EditAction::Increase: delta = ek.fine ? 0.1f : 1.0f; break;
            case a11y::EditAction::Decrease: delta = ek.fine ? -0.1f : -1.0f; break;
            case a11y::EditAction::IncreaseLarge: delta = 12.0f; break;
            case a11y::EditAction::DecreaseLarge: delta = -12.0f; break;
            default: break;
        }
        if (delta != 0.0f) {
            auto newPlain = ek.fine ? plain + delta : std::round(plain + delta);
            setParamNormalised(param, range.convertTo0to1(juce::jlimit(range.start, range.end, newPlain)));
            return true;
        }
    }

    switch (ek.action) {
        case a11y::EditAction::Increase: stepValue(param, step); break;
        case a11y::EditAction::Decrease: stepValue(param, -step); break;
        case a11y::EditAction::IncreaseLarge: stepValue(param, largeStep); break;
        case a11y::EditAction::DecreaseLarge: stepValue(param, -largeStep); break;
        case a11y::EditAction::ToMax: setParamNormalised(param, 1.0f); break;
        case a11y::EditAction::ToMin: setParamNormalised(param, 0.0f); break;
        case a11y::EditAction::ToDefault: setParamNormalised(param, param->getDefaultValue()); break;
        default: return false;
    }

    // With the velocity map off, the accessible value is the main param, so a
    // Shift edit of the velocity amount raises no value change; speak it instead.
    if (vel && !audioProcessor.velMap)
        a11y::announce("Velocity " + formatVelocity(param->convertFrom0to1(param->getValue())), this);

    return true;
}

void Rotary::focusGained(FocusChangeType)
{
    repaint();
}

void Rotary::focusLost(FocusChangeType)
{
    repaint();
}

void Rotary::velMapChanged()
{
    repaint();
    a11y::notifyTitle(*this);
    a11y::notifyValue(*this);
}

juce::String Rotary::getValueTitle() const
{
    auto title = getTitle().isNotEmpty() ? getTitle() : name;
    if (targetsVelocity())
        title << " velocity";
    return title;
}

juce::String Rotary::getValueText() const
{
    auto param = getTargetParam();
    auto plain = param->convertFrom0to1(param->getValue());
    if (targetsVelocity())
        return formatVelocity(plain);
    if (format == LabelFormat::ABMix) // "50:50" reads poorly with a screen reader
        return "A " + juce::String((int)((1 - plain) * 100)) + " %, B " + juce::String((int)(plain * 100)) + " %";
    return formatValue(plain);
}

double Rotary::getNormalisedValue() const
{
    return getTargetParam()->getValue();
}

void Rotary::setNormalisedValue(double v)
{
    setParamNormalised(getTargetParam(), (float)v);
}

std::optional<float> Rotary::parseValue(const juce::String& rawText) const
{
    auto text = rawText.trim().toLowerCase();
    if (text.isEmpty())
        return std::nullopt;

    auto number = a11y::parseNumber(text);
    const bool kilo = text.containsChar('k');

    if (targetsVelocity()) {
        if (!number) return std::nullopt;
        return (float)(*number / 100.0);
    }

    switch (format) {
        case LabelFormat::Percent:
        case LabelFormat::float2_100:
            if (!number) return std::nullopt;
            return (float)(*number / 100.0);
        case LabelFormat::millis:
            if (!number) return std::nullopt;
            if (!text.contains("ms") && (text.endsWithChar('s') || text.contains("sec")))
                return (float)(*number * 1000.0);
            return (float)*number;
        case LabelFormat::seconds2f:
            if (!number) return std::nullopt;
            if (text.contains("ms"))
                return (float)(*number / 1000.0);
            return (float)*number;
        case LabelFormat::Hz:
            if (!number) return std::nullopt;
            return (float)(kilo ? *number * 1000.0 : *number);
        case LabelFormat::ABMix: {
            if (text.containsChar('b')) { // "A 30 %, B 70 %"
                auto bVal = a11y::parseNumber(text.fromFirstOccurrenceOf("b", false, false));
                if (!bVal) return std::nullopt;
                return (float)(*bVal / 100.0);
            }
            if (text.containsChar(':')) {
                auto b = a11y::parseNumber(text.fromFirstOccurrenceOf(":", false, false));
                if (!b) return std::nullopt;
                return (float)(*b / 100.0);
            }
            if (!number) return std::nullopt;
            return (float)(*number / 100.0);
        }
        case LabelFormat::FilterLPHP: {
            if (text.contains("off"))
                return 0.0f;
            if (!number) return std::nullopt;
            const bool lp = text.contains("lp") || text.contains("low");
            const bool hp = text.contains("hp") || text.contains("high");
            if (!lp && !hp) {
                // a bare number is taken as the raw -1..1 position
                if (*number >= -1.0 && *number <= 1.0) return (float)*number;
                return std::nullopt;
            }
            auto freq = juce::jlimit(20.0, 20000.0, kilo ? *number * 1000.0 : *number);
            auto pos = std::log(freq / 20.0) / std::log(1000.0); // inverse of 20 * 1000^pos
            return (float)(lp ? pos - 1.0 : pos);
        }
        case LabelFormat::dB:
        case LabelFormat::float1:
        case LabelFormat::PitchSemis:
            if (!number) return std::nullopt;
            return (float)*number;
    }
    return std::nullopt;
}

bool Rotary::setValueFromText(const juce::String& text)
{
    auto param = getTargetParam();
    auto plain = parseValue(text);
    if (!plain)
        return false;

    const auto& range = param->getNormalisableRange();
    auto value = juce::jlimit(range.start, range.end, *plain);
    setParamNormalised(param, range.convertTo0to1(value));
    return true;
}

void Rotary::showAccessibleMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader(getValueTitle());
    menu.addItem(1, "Enter value...");
    menu.addItem(2, "Reset to default");
    if (velId.isNotEmpty())
        menu.addItem(3, "Reset velocity amount");

    juce::Component::SafePointer<Rotary> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [safeThis](int result) {
            if (safeThis == nullptr) return;
            if (result != 1) a11y::refocusAsync(*safeThis); // 1 opens the type-in, which takes focus
            if (result == 0) return;
            if (result == 1) {
                a11y::openTypeIn(*safeThis, *safeThis);
            }
            else if (result == 2) {
                auto param = safeThis->getTargetParam();
                safeThis->setParamNormalised(param, param->getDefaultValue());
            }
            else if (result == 3) {
                auto param = safeThis->audioProcessor.params.getParameter(safeThis->velId);
                safeThis->setParamNormalised(param, param->getDefaultValue());
            }
        });
}

std::unique_ptr<juce::AccessibilityHandler> Rotary::createAccessibilityHandler()
{
    return std::make_unique<a11y::ValueWidgetHandler>(*this);
}

void Rotary::draw_rotary_slider(juce::Graphics& g, float slider_pos) {
    auto bounds = getBounds();
    const float radius = 16.0f;
    const float angle = -deg130 + slider_pos * (deg130 - -deg130);

    juce::DropShadow shadow(Colour(audioProcessor.darkTheme ? 0xff000000 : 0xff888888), 15, {4, 4}); // (Color, Blur Radius, Offset)
    juce::Path circlePath;
    circlePath.addEllipse(bounds.getWidth()/2.0f-radius, bounds.getHeight()/2.0f-radius-4.0f, radius*2.0f, radius*2.0f); // x, y, width, height
    shadow.drawForPath(g, circlePath);
    g.setColour(Colour(globals::COLOR_BACKGROUND));
    g.fillPath(circlePath);

    g.setColour(Colour(globals::COLOR_ACTIVE));
    if ((isSymmetric && slider_pos != 0.5f) || (!isSymmetric && slider_pos)) {
        juce::Path arc;
        arc.addCentredArc(bounds.getWidth() / 2.0f, bounds.getHeight() / 2.0f - 4.0f, radius + 2.0f, radius + 2.0f, 0, isSymmetric ? 0 : -deg130, angle, true);
        g.strokePath(arc, PathStrokeType(2.0, PathStrokeType::JointStyle::curved, PathStrokeType::rounded));
    }

    g.setColour(Colour(globals::COLOR_NEUTRAL));
    juce::Path p;
    p.addLineSegment (juce::Line<float>(0.0f, -5.0f, 0.0f, -radius + 5.0f), 0.1f);
    juce::PathStrokeType(3.0f, PathStrokeType::JointStyle::curved, PathStrokeType::rounded).createStrokedPath(p, p);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated(bounds.getWidth() / 2.0f, bounds.getHeight() / 2.0f - 4.0f));

    if (velId.isNotEmpty() && audioProcessor.velMap) {
        g.setColour(Colour(globals::COLOR_VEL).withAlpha(0.5f));
        g.fillEllipse(bounds.getWidth()/2.0f-radius, bounds.getHeight()/2.0f-radius-4.0f, radius*2.0f, radius*2.0f);
    }
}

void Rotary::draw_vel_arc(juce::Graphics& g, float slider_pos, float vel_val) const {
    auto bounds = getBounds();
    const float radius = bounds.getHeight() / 2.0f - 20.0f;
    const float slider_angle = -deg130 + slider_pos * (deg130 - -deg130);
    const float vel_angle = fmax(-deg130, fmin(deg130, slider_angle + vel_val * (deg130 - -deg130)));
    g.setColour(Colour(globals::COLOR_VEL));

    if ((vel_val > 0.0f && slider_angle < deg130) || (vel_val < 0.0f && slider_angle > -deg130)) {
        juce::Path arc;
        arc.addCentredArc(bounds.getWidth() / 2.0f, bounds.getHeight() / 2.0f - 4.0f, radius + 8.0f, radius + 8.0f, 0, slider_angle, vel_angle, true);
        g.strokePath(arc, PathStrokeType(2.0, PathStrokeType::JointStyle::curved, PathStrokeType::rounded));
    }
}

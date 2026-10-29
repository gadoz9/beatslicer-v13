#include "PluginEditor.h"
#include <cstring>

using namespace bsui;

namespace
{
const juce::Colour kBgTop(0xff15171b), kBgBottom(0xff0e1013), kPanelEdge(0xff565d6a);
const juce::Colour kMetalTop(0xff434954), kMetalBottom(0xff2b2f37);
const juce::Colour kField(0xff1b1e23), kFieldEdge(0xff4c535f), kText(0xffe8ecf2), kDim(0xffb3bbc8);

juce::Font ui(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }

void paintMetal(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent)
{
    g.setGradientFill(juce::ColourGradient(kMetalTop, 0.0f, r.getY(), kMetalBottom, 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle(r, 10.0f);
    g.saveState();
    juce::Path clip;
    clip.addRoundedRectangle(r, 10.0f);
    g.reduceClipRegion(clip);
    g.setColour(juce::Colours::white.withAlpha(0.022f));
    for (float y = r.getY() + 1.0f; y < r.getBottom(); y += 2.0f) g.fillRect(r.getX(), y, r.getWidth(), 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRect(r.getX(), r.getY(), r.getWidth(), 26.0f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(r.getX(), r.getY() + 26.0f, r.getWidth(), 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.fillRect(r.getX(), r.getY() + 27.0f, r.getWidth(), 1.0f);
    g.restoreState();
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(r, 10.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawHorizontalLine((int) r.getY() + 1, r.getX() + 9.0f, r.getRight() - 9.0f);
    g.setColour(accent);
    g.setFont(ui(11.0f, true).withExtraKerningFactor(0.16f));
    g.drawText(title, 12, 6, (int) r.getWidth() - 24, 16, juce::Justification::centredLeft);
}
} // namespace

// ============================================================================ look and feel
LookAndFeel::LookAndFeel()
{
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff2f333b));
    setColour(juce::PopupMenu::textColourId, kText);
    setColour(juce::PopupMenu::headerTextColourId, orange());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, orange());
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff1a1208));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd5dbe5));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, kDim);
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d0db));
    setColour(juce::TextButton::textColourOnId, juce::Colour(0xff1a1208));
    setColour(juce::TextEditor::backgroundColourId, kField);
    setColour(juce::TextEditor::textColourId, kText);
    setColour(juce::TextEditor::outlineColourId, kFieldEdge);
    setColour(juce::AlertWindow::backgroundColourId, juce::Colour(0xff2f333b));
    setColour(juce::AlertWindow::textColourId, kText);
}
juce::Font LookAndFeel::getLabelFont(juce::Label&) { return ui(10.5f); }
juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox&) { return ui(12.0f); }
juce::Font LookAndFeel::getPopupMenuFont() { return ui(13.0f); }
juce::Font LookAndFeel::getTextButtonFont(juce::TextButton& b, int)
{
    if (b.getProperties().contains("fontSize")) return ui((float) b.getProperties()["fontSize"], true); // v13: PICK ALL under LOOP
    return ui(b.getProperties().contains("small") ? 11.0f : 13.0f, b.getProperties().contains("small"));
}
// v13: an orange button with dark text (CLEAR, PICK ALL)
static void makeSolid(juce::TextButton& b)
{
    b.getProperties().set("solid", true);
    b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff1a1208));
    b.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff1a1208));
}

// v10: "Default  -  Reset Plugin" - the word Default in the plugin's orange, the rest white. No tick.
static const char* kDefaultItem = "Default  -  Reset Plugin";
void LookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                    bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                                    const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator || text != kDefaultItem)
    {
        LookAndFeel_V4::drawPopupMenuItem(g, area, isSeparator, isActive, isHighlighted, isTicked, hasSubMenu, text, shortcut, icon, textColour);
        return;
    }
    LookAndFeel_V4::drawPopupMenuItem(g, area, false, isActive, isHighlighted, false, false, {}, {}, nullptr, textColour); // background only
    auto r = area.reduced(1);
    r.reduce(juce::jmin(5, area.getWidth() / 20), 0);
    auto font = getPopupMenuFont();
    const float maxH = (float) r.getHeight() / 1.3f;
    if (font.getHeight() > maxH) font.setHeight(maxH);
    r.removeFromLeft(juce::roundToInt(maxH));
    const juce::String first = "Default", rest = juce::String(kDefaultItem).substring(first.length());
    const auto hiText = findColour(juce::PopupMenu::highlightedTextColourId);
    g.setFont(font.boldened());
    g.setColour(isHighlighted ? hiText : orange());
    g.drawText(first, r, juce::Justification::centredLeft, false);
    const int w = font.boldened().getStringWidth(first);
    g.setFont(font);
    g.setColour(isHighlighted ? hiText : findColour(juce::PopupMenu::textColourId));
    g.drawText(rest, r.withTrimmedLeft(w), juce::Justification::centredLeft, false);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto area = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.0f);
    const float d = juce::jmin(area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float trackR = d * 0.5f - 2.0f;
    const bool hot = s.isMouseOverOrDragging();
    juce::Path track;
    track.addCentredArc(c.x, c.y, trackR, trackR, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xff15181c));
    g.strokePath(track, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float angle = start + pos * (end - start);
    float a0 = start, a1 = angle;
    if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
    {
        a0 = start + (float) s.valueToProportionOfLength(0.0) * (end - start);
        if (a1 < a0) std::swap(a0, a1);
    }
    if (a1 - a0 > 0.01f)
    {
        juce::Path val;
        val.addCentredArc(c.x, c.y, trackR, trackR, 0.0f, a0, a1, true);
        g.setColour(hot ? accentOf(s).brighter(0.25f) : accentOf(s));
        g.strokePath(val, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float bodyR = trackR - 6.0f;
    if (s.getProperties().contains("monster"))
    {
        // v8: BALAGAN - a little monster: teeth around the ring, a dark red body and a light fang as the pointer
        const auto red = LookAndFeel::balagan();
        const int teeth = 16;
        for (int k = 0; k < teeth; ++k)
        {
            const float a = (float) k / (float) teeth * juce::MathConstants<float>::twoPi;
            const float w = 0.13f, r0 = trackR + 3.0f, r1 = trackR + (k % 2 == 0 ? 8.5f : 6.0f);
            juce::Path t;
            t.startNewSubPath(c.x + std::sin(a - w) * r0, c.y - std::cos(a - w) * r0);
            t.lineTo(c.x + std::sin(a) * r1, c.y - std::cos(a) * r1);
            t.lineTo(c.x + std::sin(a + w) * r0, c.y - std::cos(a + w) * r0);
            t.closeSubPath();
            const bool lit = a >= 0.0f && (std::fmod(a - start + juce::MathConstants<float>::twoPi * 2.0f, juce::MathConstants<float>::twoPi) <= angle - start);
            g.setColour(lit ? red.brighter(hot ? 0.3f : 0.0f) : juce::Colour(0xff5a1620));
            g.fillPath(t);
        }
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffb3263a), c.x - bodyR * 0.45f, c.y - bodyR * 0.55f,
                                               juce::Colour(0xff230a0f), c.x + bodyR * 0.8f, c.y + bodyR * 0.9f, true));
        g.fillEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);
        // two small eyes that look where the knob points
        const float sn = std::sin(angle), cs = std::cos(angle);
        for (float side : { -1.0f, 1.0f })
        {
            const float ex = c.x + side * bodyR * 0.36f, ey = c.y - bodyR * 0.22f;
            g.setColour(juce::Colour(0xffffe1b0));
            g.fillEllipse(ex - 3.6f, ey - 3.6f, 7.2f, 7.2f);
            g.setColour(juce::Colour(0xff14080a));
            g.fillEllipse(ex + sn * 1.5f - 1.8f, ey - cs * 1.5f - 1.8f, 3.6f, 3.6f);
        }
        {
            // v10: a mouth with teeth under the eyes. Low Balagan = almost shut, turning it up opens it wide.
            const float open = juce::jlimit(0.0f, 1.0f, pos);
            const float L = c.x - bodyR * 0.50f, R = c.x + bodyR * 0.50f, my = c.y + bodyR * 0.20f;
            const float h = bodyR * (0.03f + 0.85f * open);
            const float up = h * 0.25f, down = h * 1.25f; // the top lip rises a little, the jaw drops
            auto topY = [&](float t) { return my - 2.0f * t * (1.0f - t) * up; };
            auto botY = [&](float t) { return my + 2.0f * t * (1.0f - t) * down; };
            juce::Path mouth;
            mouth.startNewSubPath(L, my);
            mouth.quadraticTo(c.x, my - up, R, my);
            mouth.quadraticTo(c.x, my + down, L, my);
            mouth.closeSubPath();
            g.setColour(juce::Colour(0xff12030a));
            g.fillPath(mouth);
            if (open > 0.25f)
            {
                // a dark red throat that shows once the mouth is open
                g.setColour(juce::Colour(0xff5c0d1a).withAlpha(juce::jmin(1.0f, (open - 0.25f) * 2.0f)));
                g.fillEllipse(c.x - bodyR * 0.2f, my + h * 0.35f, bodyR * 0.4f, h * 0.45f);
            }
            g.setColour(juce::Colour(0xfff4ede2));
            const float tw = (R - L) * 0.085f;
            for (float t : { 0.2f, 0.4f, 0.6f, 0.8f }) // upper teeth hang down
            {
                const float x = L + t * (R - L), y = topY(t), gap = botY(t) - y;
                const float th = juce::jlimit(1.5f, 5.0f, gap * 0.45f);
                juce::Path tooth;
                tooth.addTriangle(x - tw, y - 0.5f, x + tw, y - 0.5f, x, y + th);
                g.fillPath(tooth);
            }
            for (float t : { 0.3f, 0.5f, 0.7f }) // lower teeth point up
            {
                const float x = L + t * (R - L), y = botY(t), gap = y - topY(t);
                const float th = juce::jlimit(1.5f, 4.5f, gap * 0.4f);
                juce::Path tooth;
                tooth.addTriangle(x - tw, y + 0.5f, x + tw, y + 0.5f, x, y - th);
                g.fillPath(tooth);
            }
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.strokePath(mouth, juce::PathStrokeType(1.0f));
        }
        juce::Path fang;
        const float px = -cs, py = -sn; // perpendicular
        fang.startNewSubPath(c.x + sn * bodyR * 0.45f + px * 2.6f, c.y - cs * bodyR * 0.45f + py * 2.6f);
        fang.lineTo(c.x + sn * bodyR * 1.02f, c.y - cs * bodyR * 1.02f);
        fang.lineTo(c.x + sn * bodyR * 0.45f - px * 2.6f, c.y - cs * bodyR * 0.45f - py * 2.6f);
        fang.closeSubPath();
        g.setColour(juce::Colour(0xfff4ede2));
        g.fillPath(fang);
        return;
    }
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffa2a9b6), c.x - bodyR * 0.45f, c.y - bodyR * 0.55f,
                                           juce::Colour(0xff2a2e35), c.x + bodyR * 0.8f, c.y + bodyR * 0.9f, true));
    g.fillEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawEllipse(c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(hot ? 0.35f : 0.18f));
    g.drawEllipse(c.x - bodyR + 1.5f, c.y - bodyR + 1.5f, bodyR * 2.0f - 3.0f, bodyR * 2.0f - 3.0f, 0.8f);
    const float sn = std::sin(angle), cs = std::cos(angle);
    g.setColour(juce::Colour(0xff14161a));
    g.drawLine(c.x + sn * bodyR * 0.35f, c.y - cs * bodyR * 0.35f, c.x + sn * bodyR * 0.9f, c.y - cs * bodyR * 0.9f, 2.2f);
}

void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    const juce::Rectangle<float> track((float) x + 2.0f, cy - 3.0f, (float) w - 4.0f, 6.0f);
    g.setColour(kField);
    g.fillRoundedRectangle(track, 3.0f);
    g.setColour(accentOf(s));
    g.fillRoundedRectangle(track.withRight(juce::jlimit(track.getX(), track.getRight(), pos)), 3.0f);
    g.setColour(juce::Colours::white);
    g.fillEllipse(pos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) w, (float) h).reduced(0.5f);
    g.setColour(box.isMouseOver() ? kField.brighter(0.15f) : kField);
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(kFieldEdge);
    g.drawRoundedRectangle(r, 5.0f, 1.0f);
}
void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    g.setColour(on ? accentOf(b) : (hl ? kField.brighter(0.15f) : kField));
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(on ? accentOf(b) : kFieldEdge);
    g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.0f);
    g.setColour(on ? juce::Colour(0xff1a1208) : kDim);
    g.setFont(ui(10.0f, true));
    g.drawText(b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}
void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hl, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool small = b.getProperties().contains("small");
    const bool on = b.getToggleState() || b.getProperties().contains("solid"); // v13: CLEAR / PICK ALL are always orange, like a lit PLAY
    juce::Colour c = on ? accentOf(b) : (small ? kField : juce::Colour(0xff252932));
    if (hl) c = c.brighter(0.12f);
    if (down) c = c.brighter(0.25f);
    g.setColour(c);
    g.fillRoundedRectangle(r, small ? 5.0f : 6.0f);
    if (small)
    {
        g.setColour(on ? accentOf(b) : kFieldEdge);
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
    }
}

// ============================================================================ panel + knob + choice row
static void tagAccent(juce::Component& c, juce::Colour col)
{
    c.getProperties().set("accent", (int) col.getARGB());
    for (auto* ch : c.getChildren()) tagAccent(*ch, col);
}
void Panel::setAccent(juce::Colour col) { accent = col; tagAccent(*this, col); repaint(); }
void Panel::paint(juce::Graphics& g) { paintMetal(g, getLocalBounds().toFloat().reduced(0.5f), title, accent); }
void Panel::resized()
{
    // v10: the knobs of a panel sit in equal slots across its width (in a wider panel they spread out, same size)
    const int right = getWidth() - 9;
    int sum = 0;
    for (auto& it : items) sum += it.w;
    if (!items.empty() && 9 + sum <= right + 1)
    {
        const float slot = (float) (right - 9) / (float) items.size();
        for (size_t k = 0; k < items.size(); ++k)
        {
            const float cx = 9.0f + slot * ((float) k + 0.5f);
            items[k].comp->setBounds(juce::roundToInt(cx - (float) items[k].w * 0.5f), 34, items[k].w, items[k].h);
        }
        return;
    }
    int x = 9, y = 34, rowH = 0;
    for (auto& it : items)
    {
        if (x + it.w > right + 1) { y += rowH; x = 9; rowH = 0; }
        it.comp->setBounds(x, y, it.w, it.h);
        x += it.w;
        rowH = juce::jmax(rowH, it.h);
    }
}

// v6: relative drag - up = more. 235 px for the whole range (v5: 200 px), Shift = 0.15x of that
void KnobSlider::mouseDown(const juce::MouseEvent& e)
{
    juce::Slider::mouseDown(e); // starts the host gesture, double-click reset still works
    ours = isEnabled() && e.mods.isLeftButtonDown() && !e.mods.isPopupMenu();
    lastY = e.position.y;
    prop = (float) valueToProportionOfLength(getValue());
}
void KnobSlider::mouseDrag(const juce::MouseEvent& e)
{
    if (!ours) { juce::Slider::mouseDrag(e); return; }
    const float dy = lastY - e.position.y;
    lastY = e.position.y;
    prop = juce::jlimit(0.0f, 1.0f, prop + dy / kPixelsFullRange * (e.mods.isShiftDown() ? kShiftFine : 1.0f));
    setValue(proportionOfLengthToValue((double) prop), juce::sendNotificationSync);
}

KnobCell::KnobCell(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& label)
    : state(&apvts), paramId(id)
{
    name.setText(label, juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centred);
    name.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(name);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd5dbe5));
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity(200);
    addAndMakeVisible(slider);
    attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, slider);
    if (auto* p = apvts.getParameter(id)) slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
}
// v10: while a slice is chosen with the SLICE lamp on, the envelope knobs show and change that slice only
void KnobCell::setOverride(float value, std::function<void(float)> onChange)
{
    if (!overridden) attach.reset(); // the slider keeps the range, skew and text of its parameter
    overridden = true;
    slider.onValueChange = nullptr;
    slider.setValue(value, juce::dontSendNotification);
    slider.onValueChange = [this, onChange] { if (onChange) onChange((float) slider.getValue()); };
    repaint();
}
void KnobCell::clearOverride()
{
    if (!overridden) return;
    overridden = false;
    slider.onValueChange = nullptr;
    attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(*state, paramId, slider);
    repaint();
}

void KnobCell::resized()
{
    name.setBounds(0, 0, getWidth(), 13);
    // v12: the BALAGAN monster shows its value in its own box at the side, so it has no number under it
    if (getProperties().contains("noTextBox")) slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    else slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, getWidth(), 14);
    slider.setBounds(0, 13, getWidth(), getHeight() - 13);
}

ChoiceRow::ChoiceRow(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::StringArray& labels, bool isBool,
                     std::vector<int> values)
    : boolMode(isBool), valueOf(std::move(values))
{
    param = apvts.getParameter(id);
    if ((int) valueOf.size() != labels.size()) { valueOf.clear(); for (int i = 0; i < labels.size(); ++i) valueOf.push_back(i); }
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add(new juce::TextButton(labels[i]));
        b->getProperties().set("small", true);
        b->setClickingTogglesState(false);
        addAndMakeVisible(b);
        const int w = ui(11.0f, true).getStringWidth(labels[i]) + 22;
        totalW += w + 3;
        b->onClick = [this, i, b]
        {
            if (param == nullptr) return;
            param->beginChangeGesture();
            if (boolMode) param->setValueNotifyingHost(b->getToggleState() ? 0.0f : 1.0f);
            else param->setValueNotifyingHost(param->convertTo0to1((float) valueOf[(size_t) i]));
            param->endChangeGesture();
            if (onPick) onPick(i);
        };
    }
    attach = std::make_unique<juce::ParameterAttachment>(*param, [this](float v)
    {
        const int idx = boolMode ? (v > 0.5f ? 0 : -1) : juce::roundToInt(v);
        for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState(boolMode ? i == idx : valueOf[(size_t) i] == idx, juce::dontSendNotification);
    });
    attach->sendInitialUpdate();
}
void ChoiceRow::resized()
{
    int x = 0;
    for (auto* b : buttons)
    {
        const int w = ui(11.0f, true).getStringWidth(b->getButtonText()) + 22;
        b->setBounds(x, 0, w, getHeight());
        x += w + 3;
    }
}

// ============================================================================ waveform view
WaveView::WaveView(SlicerProcessor& p) : proc(p) { startTimerHz(20); }

namespace
{
// v11 / v12: LOCK ONE SHOT in the dots - the place of the copy of dot i (the same for drawing and for the mouse)
juce::Rectangle<float> lockCell(juce::Rectangle<float> b, int spb, int i)
{
    const float cellW = (b.getWidth() - 8.0f) / (float) juce::jmax(1, spb);
    const float x0 = b.getX() + 4.0f + cellW * (float) i;
    return { x0 + 1.0f, b.getY() + 15.0f, cellW - 2.0f, b.getHeight() - 30.0f };
}
float lockCellX(const juce::Rectangle<float>& cell, float frac) { return cell.getX() + 1.0f + (cell.getWidth() - 2.0f) * frac; }

// v12: the envelope of a sound that lives between its locators (One Shot, LOCK ONE SHOT, a dot with its own locators):
// Attack starts at the left one, Release ends at the right one, or where Length closes the sound if that comes first.
// t and the times are in seconds; audio = how long the sound between the locators plays. 0 after the end.
float fitEnvAt(double t, double audio, float len, float atk, float dec, float sus, float rel)
{
    const double gate = juce::jmax(1.0e-4, (double) len * audio);
    const double a = juce::jmin((double) atk, gate * 0.5);
    const double r = juce::jmax(0.003, juce::jmin((double) rel, gate - a));
    const double hold = juce::jmax(a, gate - r);
    auto pre = [&](double u)
    {
        if (u < a) return (float) (u / juce::jmax(1.0e-4, a));
        if (u < a + dec) return 1.0f + (sus - 1.0f) * (float) ((u - a) / juce::jmax(1.0e-4f, dec));
        return sus;
    };
    if (t < 0.0 || t > gate) return 0.0f;
    if (t < hold) return pre(t);
    return juce::jmax(0.0f, pre(hold) * (1.0f - (float) ((t - hold) / r)));
}
}

// v12: where the locators of the dots sit on screen. LOCK ONE SHOT: every copy has its two (at the edges when not set).
// The dots mode: only the chosen dots show theirs, on the box their chop comes from.
std::vector<WaveView::TrimHandle> WaveView::trimHandles() const
{
    std::vector<TrimHandle> out;
    auto plan = proc.getPlan();
    if (!plan || !plan->sample || proc.isOneShot()) return out;
    const int spb = juce::jmax(1, plan->stepsPerBar);
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    const int N = plan->sample->size();
    for (int i = 0; i < spb && i < bs::kPatternLen && i < plan->totalSteps(); ++i)
    {
        if (plan->locked)
        {
            const auto cell = lockCell(b, spb, i);
            out.push_back({ i, bs::kDotStart, lockCellX(cell, proc.getDotTrim(i, bs::kDotStart)) });
            out.push_back({ i, bs::kDotEnd, lockCellX(cell, proc.getDotTrim(i, bs::kDotEnd)) });
            continue;
        }
        if (!(proc.getSelectedDots() & (1u << i))) continue;
        int a = 0, e = 0;
        if (!trimRange(i, a, e)) continue;
        for (int w : { bs::kDotStart, bs::kDotEnd })
            out.push_back({ i, w, normToX(((float) a + (float) (e - a) * proc.getDotTrim(i, w)) / (float) N) });
    }
    return out;
}
// the box (on screen, before slip) whose audio the locators of this dot cut
bool WaveView::trimRange(int dot, int& boxA, int& boxB) const
{
    auto plan = proc.getPlan();
    if (!plan || !plan->sample || dot < 0 || dot >= plan->totalSteps()) return false;
    if (plan->locked) { boxA = plan->regionStart; boxB = plan->regionEnd; return true; }
    const int seg = plan->slices[(size_t) dot].seg;
    if (seg < 0 || seg >= (int) plan->segments.size()) return false;
    boxA = plan->segments[(size_t) seg].first;
    boxB = plan->segments[(size_t) seg].second;
    return boxB - boxA > 16;
}

// v6: zoom - closes in on the region between the orange markers (log scale, so every bit of the slider counts)
void WaveView::setZoom(float z)
{
    if (z <= 0.0f) anchorZoomToRegion();
    zoom = juce::jlimit(0.0f, 1.0f, z);
    updateView();
    repaint();
}
void WaveView::anchorZoomToRegion()
{
    anchorLo = proc.apvts.getRawParameterValue("start")->load();
    anchorHi = proc.apvts.getRawParameterValue("end")->load();
    updateView();
}
void WaveView::updateView()
{
    const float m = 0.03f * juce::jmax(0.01f, anchorHi - anchorLo);
    const float lo = juce::jmax(0.0f, anchorLo - m), hi = juce::jmin(1.0f, anchorHi + m);
    const float target = juce::jlimit(0.002f, 1.0f, hi - lo);
    const float span = std::pow(target, zoom);
    const float t = target < 0.9999f ? lo / (1.0f - target) : 0.0f;
    viewLo = juce::jlimit(0.0f, 1.0f - span, t * (1.0f - span));
    viewHi = viewLo + span;
}

void WaveView::rebuildPeaks(const bs::SampleData* sd)
{
    peaks.clear();
    peaksFor = sd;
    peaksLo = viewLo; peaksHi = viewHi;
    if (!sd || sd->size() < 2) return;
    const int cols = juce::jmax(1, getWidth() - 8);
    const double A = (double) viewLo * sd->size(), S = (double) (viewHi - viewLo) * sd->size();
    for (int c = 0; c < cols; ++c)
    {
        const int a = juce::jlimit(0, sd->size() - 1, (int) (A + S * c / cols)), b = juce::jlimit(a + 1, sd->size(), (int) (A + S * (c + 1) / cols));
        float mn = 0.0f, mx = 0.0f;
        const int stride = juce::jmax(1, (b - a) / 64);
        for (int i = a; i < b; i += stride)
        {
            const float v = 0.5f * (sd->L[(size_t) i] + sd->R[(size_t) i]);
            mn = juce::jmin(mn, v); mx = juce::jmax(mx, v);
        }
        peaks.push_back({ mn, mx });
    }
}

void WaveView::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff0f1115));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(juce::Colour(0xff555c69));
    g.drawRoundedRectangle(b, 6.0f, 1.0f);
    const float cy = b.getCentreY();
    g.setColour(juce::Colour(0xff262b33));
    g.drawHorizontalLine((int) cy, b.getX(), b.getRight());

    auto plan = proc.getPlan();
    const bs::SampleData* sd = plan ? plan->sample.get() : nullptr;
    if (sd != peaksFor || (int) peaks.size() != juce::jmax(0, getWidth() - 8) || peaksLo != viewLo || peaksHi != viewHi)
    {
        keep = plan ? plan->sample : nullptr;
        rebuildPeaks(sd);
    }
    if (!sd)
    {
        g.setColour(juce::Colour(0xff8a93a3));
        g.setFont(ui(15.0f));
        g.drawText("Drop an audio file here  (wav / aiff / flac / mp3)  or press LOAD", getLocalBounds(), juce::Justification::centred);
        return;
    }
    const float h = (float) getHeight() * 0.46f;
    float pk = 0.05f;
    for (auto& p : peaks) pk = juce::jmax(pk, std::fabs(p.first), std::fabs(p.second));
    // v5: the full sample is drawn dim; on top every slice is drawn the way it really plays
    // (velocity dot, Length, Attack, Decay, Sustain, Release). The dim part is what you do not hear.
    g.setColour(juce::Colour(0xff4a515e));
    for (size_t i = 0; i < peaks.size(); ++i)
    {
        const float x = 4.0f + (float) i;
        g.drawVerticalLine((int) x, cy - peaks[i].second / pk * h, cy - peaks[i].first / pk * h + 1.0f);
    }

    const float rs = proc.apvts.getRawParameterValue("start")->load(), re = proc.apvts.getRawParameterValue("end")->load();
    const float xs = normToX(rs), xe = normToX(re);
    // v6: everything below is drawn inside the frame (when zoomed in, parts lie outside the view)
    g.saveState();
    g.reduceClipRegion(b.reduced(1.0f).toNearestInt());
    g.setColour(juce::Colour(0xaa0a0b0e));
    g.fillRect(b.getX(), b.getY(), juce::jlimit(0.0f, b.getWidth(), xs - b.getX()), b.getHeight());
    g.fillRect(juce::jmax(b.getX(), xe), b.getY(), juce::jmax(0.0f, b.getRight() - juce::jmax(b.getX(), xe)), b.getHeight());

    const int N = sd->size();
    const int cols = (int) peaks.size();
    auto raw = [this](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
    float pLen = raw("length");
    const float pAtk = raw("attack") * 0.001f, pDec = raw("decay") * 0.001f, pSus = raw("sustain"),
                pRel = raw("release") * 0.001f, pDepth = raw("velocity");
    const double rate = std::pow(2.0, (double) raw("pitch") / 12.0);
    // v10: the envelope a slice draws with - its own (SLICE lamp) or the knobs; `fit` = it lives inside the slice
    struct DrawEnv { float atk, dec, sus, rel; bool fit; };
    const bool envSliceMode = proc.isEnvSlice();
    DrawEnv cur { pAtk, pDec, pSus, pRel, false };
    // envelope of one slice at `sec` seconds after it starts, gate closes at `gateSec`
    auto envAt = [&](double sec, double gateSec)
    {
        auto pre = [&](double t)
        {
            if (t < cur.atk) return (float) (t / juce::jmax(1.0e-4f, cur.atk));
            if (t < cur.atk + cur.dec) return 1.0f + (cur.sus - 1.0f) * (float) ((t - cur.atk) / juce::jmax(1.0e-4f, cur.dec));
            return cur.sus;
        };
        if (sec < gateSec) return pre(sec);
        return juce::jmax(0.0f, pre(gateSec) * (1.0f - (float) ((sec - gateSec) / juce::jmax(1.0e-4f, cur.rel))));
    };
    // draws one slice (source range a..b on screen, audio shifted by `off` samples) shaped by its envelope and level
    auto drawShaped = [&](int a, int bEnd, int off, bool rev, double gateSec, float level, juce::Colour wave, juce::Colour line)
    {
        if (level <= 0.0f || cols <= 0) return;
        const float keepRel = cur.rel;
        if (cur.fit)
        {
            // v10: SLICE - the release ends where the audio of the slice ends (as the engine plays it)
            const double audioSec = (double) juce::jmax(1, bEnd - a) / (sd->sr * rate);
            const double gate = juce::jmin(gateSec, (double) pLen * audioSec); // Length counts inside the slice too
            const double atk = juce::jmin((double) cur.atk, gate * 0.5);
            cur.rel = (float) juce::jmax(0.003, juce::jmin((double) cur.rel, gate - atk));
            gateSec = juce::jmax(atk, gate - (double) cur.rel);
        }
        const double A = (double) viewA() * N, S = (double) viewSpan() * N; // v6: visible part of the sample
        const int c0 = juce::jmax(0, (int) std::ceil(((double) a - A) * cols / S)), c1 = juce::jmin(cols - 1, (int) (((double) bEnd - A) * cols / S));
        if (c1 < c0) return;
        juce::Path envLine;
        bool started = false;
        float gateX = -1.0f;
        {
            const double gs = gateSec * sd->sr * rate; // samples of the source that play before the gate closes
            const double gp = rev ? (double) bEnd - gs : (double) a + gs;
            if (gp > (double) a && gp < (double) bEnd) gateX = 4.0f + (float) ((gp - A) * cols / S);
        }
        for (int c = c0; c <= c1; ++c)
        {
            const int sa = (int) (A + S * c / cols), se = juce::jmax(sa + 1, (int) (A + S * (c + 1) / cols));
            const double mid = 0.5 * (sa + se);
            const double offs = rev ? (double) bEnd - mid : mid - (double) a;
            const double sec = juce::jmax(0.0, offs) / (sd->sr * rate);
            const float e = envAt(sec, gateSec) * level;
            float mn = peaks[(size_t) c].first, mx = peaks[(size_t) c].second;
            if (off != 0)
            {
                mn = mx = 0.0f;
                const int stride = juce::jmax(1, (se - sa) / 48);
                for (int k = juce::jmax(0, sa + off); k < juce::jmin(N, se + off); k += stride)
                {
                    const float v = 0.5f * (sd->L[(size_t) k] + sd->R[(size_t) k]);
                    mn = juce::jmin(mn, v); mx = juce::jmax(mx, v);
                }
            }
            const float x = 4.0f + (float) c;
            if (e > 0.001f)
            {
                g.setColour(wave);
                g.drawVerticalLine((int) x, cy - mx * e / pk * h, cy - mn * e / pk * h + 1.0f);
            }
            const float ly = cy - e * h;
            if (!started) { envLine.startNewSubPath(x, ly); started = true; }
            else envLine.lineTo(x, ly);
        }
        g.setColour(line);
        g.strokePath(envLine, juce::PathStrokeType(1.2f));
        if (gateX > 0.0f)
        {
            // where Length closes the slice (the release starts here)
            g.setColour(line.withAlpha(0.8f));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(gateX, b.getY() + 3.0f, gateX, b.getBottom() - 3.0f), dash, 2, 1.0f);
        }
        cur.rel = keepRel;
    };
    // v10: LOCK ONE SHOT - the locked part gets a soft orange tint and a label, in both modes
    const bool locked = proc.isOneShotLocked();
    auto drawLock = [&]
    {
        if (!locked) return;
        g.setColour(LookAndFeel::orange().withAlpha(0.07f));
        g.fillRect(xs, b.getY() + 1.0f, juce::jmax(0.0f, xe - xs), b.getHeight() - 2.0f);
        g.setColour(LookAndFeel::orange().withAlpha(0.95f));
        g.setFont(ui(9.5f, true));
        g.drawText(juce::String::fromUTF8("LOCKED"), (int) xe - 64, (int) b.getBottom() - 15, 58, 12, juce::Justification::right);
    };
    if (proc.isOneShot())
    {
        // One Shot: no slices, only the region that plays
        g.setColour(LookAndFeel::cyan().withAlpha(0.10f));
        g.fillRect(xs, b.getY() + 1.0f, juce::jmax(0.0f, xe - xs), b.getHeight() - 2.0f);
        {
            const int a = plan->regionStart, e = plan->regionEnd;
            const double gate = (double) (e - a) / (sd->sr * rate); // v13: Length does not cut a one shot - it plays to the right locator
            cur.fit = true; // v12: the envelope lives between the locators (Release ends at the right one)
            const float keepLen = pLen;
            pLen = 1.0f;
            drawShaped(a, e, 0, false, gate, 1.0f, juce::Colour(0xffd5dbe5), LookAndFeel::cyan().withAlpha(0.9f));
            pLen = keepLen;
            cur.fit = false;
        }
        g.setColour(LookAndFeel::cyan());
        g.setFont(ui(10.0f, true));
        g.drawText("ONE SHOT", (int) xs + 8, (int) b.getY() + 4, 80, 12, juce::Justification::left);
        drawLock();
        g.setColour(locked ? LookAndFeel::orange().darker(0.35f) : LookAndFeel::orange());
        g.fillRect(xs - 2.0f, b.getY(), 4.0f, b.getHeight());
        g.fillRect(xe - 2.0f, b.getY(), 4.0f, b.getHeight());
        g.restoreState();
        return;
    }
    if (plan->locked)
    {
        // v11: LOCK ONE SHOT in the dots - one small copy of the locked one shot for every dot of the bar,
        // each drawn the way that dot plays it (its level, Length, envelope, pitch and direction)
        g.setColour(juce::Colour(0xff0f1115));
        g.fillRect(b.reduced(1.0f));
        const int spbL = juce::jmax(1, plan->stepsPerBar);
        const int ra = plan->regionStart, re2 = plan->regionEnd;
        const float cellW = (b.getWidth() - 8.0f) / (float) spbL;
        const int nc = juce::jmax(4, (int) cellW - 4);
        std::vector<std::pair<float, float>> rp((size_t) nc);
        float rpk = 0.02f;
        for (int c = 0; c < nc; ++c)
        {
            const int s0 = ra + (int) ((double) (re2 - ra) * c / nc), s1 = juce::jmax(s0 + 1, ra + (int) ((double) (re2 - ra) * (c + 1) / nc));
            float mn = 0.0f, mx = 0.0f;
            const int stride = juce::jmax(1, (s1 - s0) / 48);
            for (int k = s0; k < s1 && k < N; k += stride)
            {
                const float v = 0.5f * (sd->L[(size_t) k] + sd->R[(size_t) k]);
                mn = juce::jmin(mn, v); mx = juce::jmax(mx, v);
            }
            rp[(size_t) c] = { mn, mx };
            rpk = juce::jmax(rpk, std::fabs(mn), std::fabs(mx));
        }
        const uint32_t chosenDots = proc.getSelectedDots();
        const float top = b.getY() + 15.0f, cellH = b.getHeight() - 30.0f, ccy = top + cellH * 0.5f;
        for (int i = 0; i < spbL && i < plan->totalSteps(); ++i)
        {
            const auto& sl = plan->slices[(size_t) i];
            const auto cell = lockCell(b, spbL, i);
            const auto col = (i & 1) ? LookAndFeel::orange() : LookAndFeel::cyan();
            const bool on = sl.vel > 0.001f, chosen = (chosenDots & (1u << i)) != 0;
            g.setColour(chosen ? juce::Colours::white.withAlpha(0.14f) : col.withAlpha(0.06f));
            g.fillRect(cell);
            // the envelope this dot plays. v12: it lives between the dot's locators (the cell = the whole locked one shot)
            const float atk = proc.getDotEnvValue(i, bs::kDotEnvAtk) * 0.001f, dec = proc.getDotEnvValue(i, bs::kDotEnvDec) * 0.001f,
                        sus = proc.getDotEnvValue(i, bs::kDotEnvSus), rel = proc.getDotEnvValue(i, bs::kDotEnvRel) * 0.001f;
            const double drate = std::pow(2.0, (double) (proc.getDotEnvValue(i, bs::kDotPitch) + sl.semi) / 12.0);
            const double durSec = (double) (re2 - ra) / (sd->sr * drate);
            const float tA = proc.getDotTrim(i, bs::kDotStart), tB = proc.getDotTrim(i, bs::kDotEnd);
            const double audio = (double) (tB - tA) * durSec;
            const float level = on ? bs::sliceLevel(sl.vel, pDepth) : 0.35f;
            const float hh = cellH * 0.46f;
            juce::Path envLine;
            bool started = false;
            for (int c = 0; c < nc; ++c)
            {
                const float x = cell.getX() + 1.0f + (cell.getWidth() - 2.0f) * ((float) c + 0.5f) / (float) nc;
                const float u = ((float) c + 0.5f) / (float) nc; // where in the one shot this column is
                const auto& pk2 = rp[(size_t) c];
                if (u < tA || u > tB)
                {
                    // outside the dot's locators: not heard, drawn dim
                    g.setColour(juce::Colour(0xff3a404b));
                    g.drawVerticalLine((int) x, ccy - pk2.second / rpk * hh * 0.35f, ccy - pk2.first / rpk * hh * 0.35f + 1.0f);
                    continue;
                }
                const double t = (sl.reverse ? (double) (tB - u) : (double) (u - tA)) * durSec; // a reversed dot starts at its right locator
                const float e = fitEnvAt(t, audio, 1.0f, atk, dec, sus, rel) * level; // v13: Length does not cut the locked one shot
                if (e > 0.001f)
                {
                    g.setColour(on ? juce::Colour(0xffd5dbe5) : juce::Colour(0xff4a515e));
                    g.drawVerticalLine((int) x, ccy - pk2.second / rpk * hh * e, ccy - pk2.first / rpk * hh * e + 1.0f);
                }
                const float ly = ccy - e * hh;
                if (!started) { envLine.startNewSubPath(x, ly); started = true; } else envLine.lineTo(x, ly);
            }
            if (on)
            {
                g.setColour(col.withAlpha(0.9f));
                g.strokePath(envLine, juce::PathStrokeType(1.1f));
            }
            {
                // v12: the dot's own locators - drag them to start / end this dot later / earlier
                const bool trimmed = proc.getStepExtras().env[i].trimmed();
                for (float f : { tA, tB })
                {
                    const float lx = lockCellX(cell, f);
                    g.setColour(LookAndFeel::orange().withAlpha(trimmed ? 0.95f : 0.45f));
                    g.fillRect(lx - 1.0f, cell.getY(), 2.0f, cell.getHeight());
                    g.fillRect(f < 0.5f ? lx - 1.0f : lx - 5.0f, cell.getY(), 6.0f, 6.0f);
                }
            }
            g.setColour(chosen ? juce::Colours::white : col.withAlpha(on ? 0.45f : 0.2f));
            g.drawRect(cell, chosen ? 2.0f : 1.0f);
            g.setFont(ui(9.0f, true));
            g.setColour(on ? col : kDim.withAlpha(0.6f));
            g.drawText(juce::String(i + 1), (int) cell.getX() + 2, (int) b.getY() + 2, 20, 11, juce::Justification::left);
            if (proc.dotHasOwnEnv(i) && cellW > 18.0f)
            {
                const juce::Rectangle<float> badge(cell.getRight() - 11.0f, b.getY() + 2.0f, 10.0f, 11.0f);
                g.setColour(LookAndFeel::cyan());
                g.fillRoundedRectangle(badge, 2.5f);
                g.setColour(juce::Colour(0xff0f1115));
                g.setFont(ui(8.5f, true));
                g.drawText("E", badge.toNearestInt(), juce::Justification::centred);
            }
        }
        g.setColour(LookAndFeel::orange().withAlpha(0.95f));
        g.setFont(ui(9.5f, true));
        g.drawText("LOCKED ONE SHOT  -  every dot plays it", (int) b.getRight() - 250, (int) b.getBottom() - 14, 244, 12, juce::Justification::right);
        g.restoreState();
        return;
    }

    // slice rectangles: every source piece gets its own coloured box.
    // A slipped slice keeps its box; the audio drawn inside shows what it really plays.
    const auto& segs = plan->segments;
    // v5: what every source piece really plays: the loudest step that uses it, its gate length and direction
    struct SegUse { float vel = 0.0f; float len = 1.0f; bool rev = false; int step = -1; };
    std::vector<SegUse> use(segs.size());
    for (int si2 = 0; si2 < plan->totalSteps(); ++si2)
    {
        const auto& sl = plan->slices[(size_t) si2];
        if (sl.seg >= 0 && sl.seg < (int) use.size() && sl.vel > use[(size_t) sl.seg].vel)
            use[(size_t) sl.seg] = { sl.vel, sl.len, sl.reverse, si2 % juce::jmax(1, plan->stepsPerBar) };
    }
    const double stepSec = 60.0 / juce::jmax(1.0, proc.getCurrentBpm()) * 4.0 / (double) juce::jmax(1, plan->stepsPerBar);
    for (int si = 0; si < (int) segs.size(); ++si)
    {
        const float x0 = normToX((float) segs[(size_t) si].first / (float) N), x1 = normToX((float) segs[(size_t) si].second / (float) N);
        const auto col = (si & 1) ? LookAndFeel::orange() : LookAndFeel::cyan();
        const bool active = si == slipSeg;
        g.setColour(col.withAlpha(active ? 0.22f : 0.08f));
        g.fillRect(x0, b.getY() + 1.0f, juce::jmax(1.0f, x1 - x0), b.getHeight() - 2.0f);
        const float slip = proc.getSlip(si);
        const int off = (int) (slip * (float) N);
        const auto& u = use[(size_t) si];
        if (off != 0 && x1 - x0 > 2.0f)
        {
            // slipped: hide the original audio and show (dim) what slides into the box
            g.setColour(juce::Colour(0xff0f1115).withAlpha(0.85f));
            g.fillRect(x0 + 1.0f, b.getY() + 1.0f, x1 - x0 - 1.0f, b.getHeight() - 2.0f);
            drawShaped(segs[(size_t) si].first, segs[(size_t) si].second, off, false, 1.0e9, 1.0f, col.withAlpha(0.3f), juce::Colours::transparentBlack);
            g.setColour(col.brighter(0.2f));
            g.setFont(ui(9.0f, true));
            const double ms = (double) off / sd->sr * 1000.0;
            g.drawText(juce::String::formatted("%+.0f ms", -ms), (int) x0 + 3, (int) b.getBottom() - 14, 60, 11, juce::Justification::left);
        }
        // v11: a dot with its own envelope draws its box with it (Length too)
        const bool dotOwn = u.step >= 0 && proc.dotHasOwnEnv(u.step);
        const float segLen = dotOwn ? proc.getDotEnvValue(u.step, bs::kDotEnvLen) : pLen;
        const double gate = (double) segLen * stepSec * (double) u.len;
        // v10: the slice's own envelope (if it has one) and the SLICE lamp shape the picture like the sound
        const bool own = proc.segHasOwnEnv(si) || dotOwn;
        {
            const auto e = proc.getSegEnv(si);
            cur = { e.attackMs * 0.001f, e.decayMs * 0.001f, e.sustain, e.releaseMs * 0.001f, envSliceMode || proc.segHasOwnEnv(si) };
            if (dotOwn)
                cur = { proc.getDotEnvValue(u.step, bs::kDotEnvAtk) * 0.001f, proc.getDotEnvValue(u.step, bs::kDotEnvDec) * 0.001f,
                        proc.getDotEnvValue(u.step, bs::kDotEnvSus), proc.getDotEnvValue(u.step, bs::kDotEnvRel) * 0.001f, cur.fit };
        }
        const bool chosen = envSliceMode && si == proc.getSelectedSeg();
        if (chosen)
        {
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.fillRect(x0, b.getY() + 1.0f, juce::jmax(1.0f, x1 - x0), b.getHeight() - 2.0f);
        }
        int dA = segs[(size_t) si].first, dB = segs[(size_t) si].second;
        if (u.step >= 0 && proc.getStepExtras().env[u.step].trimmed())
        {
            // v12: the dot has its own locators - only that part plays, and the envelope lives between them
            const auto& de = proc.getStepExtras().env[u.step];
            const int la = dB - dA;
            dA = segs[(size_t) si].first + (int) std::lround(la * de.trimA());
            if (de.has(bs::kDotEnd)) dB = segs[(size_t) si].first + (int) std::lround(la * de.trimB());
            cur.fit = true;
        }
        drawShaped(dA, dB, off, u.rev, gate, bs::sliceLevel(u.vel, pDepth),
                   off != 0 ? col.brighter(0.2f) : juce::Colour(0xffd5dbe5), col.withAlpha(0.9f));
        cur = { pAtk, pDec, pSus, pRel, false };
        g.setColour(chosen ? juce::Colours::white : col.withAlpha(active ? 0.9f : 0.35f));
        g.drawRect(x0, b.getY() + 1.0f, juce::jmax(1.0f, x1 - x0), b.getHeight() - 2.0f, chosen ? 2.0f : (active ? 1.5f : 1.0f));
        if (own && x1 - x0 > 12.0f)
        {
            // v10: a small "E" badge = this slice has its own envelope
            const juce::Rectangle<float> badge(x1 - 13.0f, b.getY() + 4.0f, 10.0f, 11.0f);
            g.setColour(LookAndFeel::cyan());
            g.fillRoundedRectangle(badge, 2.5f);
            g.setColour(juce::Colour(0xff0f1115));
            g.setFont(ui(8.5f, true));
            g.drawText("E", badge.toNearestInt(), juce::Justification::centred);
        }
    }
    drawLock();

    // slice starts (first bar only, so the picture stays readable)
    g.setFont(ui(9.0f));
    float lastLabel = -100.0f;
    std::vector<int> drawn;
    for (int i = 0; i < plan->totalSteps() && i < plan->stepsPerBar; ++i)
    {
        const auto& sl = plan->slices[(size_t) i];
        if (sl.vel <= 0.001f) continue;
        const int segStart = sl.seg >= 0 && sl.seg < (int) segs.size() ? segs[(size_t) sl.seg].first : sl.start;
        const float x = normToX((float) segStart / (float) N);
        g.setColour(LookAndFeel::cyan().withAlpha(0.85f));
        g.drawVerticalLine((int) x, b.getY(), b.getBottom());
        if (x - lastLabel > 14.0f)
        {
            g.drawText(juce::String(i + 1), (int) x + 3, (int) b.getY() + 2, 20, 11, juce::Justification::left);
            lastLabel = x;
        }
    }
    // manual markers
    g.setColour(LookAndFeel::orange());
    for (float c : proc.getManualCuts())
    {
        const float x = normToX(c);
        juce::Path t;
        t.addTriangle(x - 4.0f, b.getBottom() - 1.0f, x + 4.0f, b.getBottom() - 1.0f, x, b.getBottom() - 9.0f);
        g.fillPath(t);
    }
    // region handles (darker while LOCK ONE SHOT holds them)
    g.setColour(locked ? LookAndFeel::orange().darker(0.35f) : LookAndFeel::orange());
    g.fillRect(xs - 2.0f, b.getY(), 4.0f, b.getHeight());
    g.fillRect(xe - 2.0f, b.getY(), 4.0f, b.getHeight());
    // v12: the locators of the chosen dots - a thin white line with a tab at the top (drag the tab)
    for (const auto& h : trimHandles())
    {
        const bool set = proc.getStepExtras().env[h.dot].has(h.which);
        g.setColour(juce::Colours::white.withAlpha(set ? 0.95f : 0.6f));
        g.drawVerticalLine((int) h.x, b.getY() + 2.0f, b.getBottom() - 2.0f);
        juce::Path tab;
        const float dir = h.which == bs::kDotStart ? 1.0f : -1.0f;
        tab.addTriangle(h.x, b.getY() + 1.0f, h.x + dir * 9.0f, b.getY() + 1.0f, h.x, b.getY() + 13.0f);
        g.fillPath(tab);
        g.setFont(ui(8.5f, true));
        g.drawText(juce::String(h.dot + 1), (int) (h.x + dir * 3.0f) - (dir < 0 ? 16 : 0), (int) b.getY() + 13, 16, 10,
                   dir > 0 ? juce::Justification::left : juce::Justification::right);
    }
    g.restoreState();
    // v6: zoom readout
    if (zoom > 0.001f)
    {
        g.setColour(LookAndFeel::cyan().withAlpha(0.9f));
        g.setFont(ui(9.5f, true));
        g.drawText(juce::String::formatted("ZOOM x%.1f", 1.0f / viewSpan()), getWidth() - 90, getHeight() - 16, 82, 12, juce::Justification::right);
    }
}

void WaveView::timerCallback()
{
    // v6: while not zoomed the zoom follows the markers; once zoomed the view stays put while markers move
    if (zoom <= 0.0f) anchorZoomToRegion();
    // Ctrl over a slice = slip cursor (two-way arrow)
    const bool slipHover = slipSeg >= 0 || (isMouseOver() && slipKey(juce::ModifierKeys::currentModifiers) && !proc.isOneShot() && !proc.isOneShotLocked()
                                             && segmentAt((float) getMouseXYRelative().x) >= 0);
    bool trimHover = trimDot >= 0; // v12: over a dot's locator
    if (!trimHover && isMouseOver() && !proc.isOneShot())
    {
        const auto mp = getMouseXYRelative().toFloat();
        const bool lockedView = proc.isOneShotLocked();
        for (const auto& h : trimHandles())
            if (std::fabs(h.x - mp.x) < 5.0f && (lockedView || mp.y < 26.0f)) { trimHover = true; break; }
    }
    setMouseCursor(slipHover || trimHover ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    // v13: drawn again only when something it shows has changed (it used to be drawn 20 times a second all the time,
    // which kept the screen busy and could make the playhead stutter)
    const auto sig = signature();
    if (sig != shownSig || dragging != 0 || slipSeg >= 0 || trimDot >= 0) { shownSig = sig; repaint(); }
}

juce::uint64 WaveView::signature() const
{
    juce::uint64 h = 1469598103934665603ull;
    auto mix = [&h](juce::uint64 v) { h ^= v; h *= 1099511628211ull; };
    auto mixF = [&mix](double d) { juce::uint64 b = 0; std::memcpy(&b, &d, sizeof(double)); mix(b); };
    mix((juce::uint64) (juce::pointer_sized_uint) proc.getPlan().get());
    for (auto* prm : proc.getParameters()) mixF((double) prm->getValue());
    mix(proc.getSelectedDots()); mix((juce::uint64) (proc.getSelectedSeg() + 7));
    mixF(proc.getCurrentBpm()); mixF(zoom); mixF(viewLo); mixF(viewHi);
    mix((juce::uint64) getWidth() * 7919u + (juce::uint64) getHeight());
    mix((juce::uint64) (proc.hasLockSnapshot() ? 3 : 5));
    return h;
}

int WaveView::segmentAt(float x) const
{
    auto plan = proc.getPlan();
    if (!plan || !plan->sample) return -1;
    const float n = xToNorm(x) * (float) plan->sample->size();
    for (int i = 0; i < (int) plan->segments.size(); ++i)
        if (n >= (float) plan->segments[(size_t) i].first && n < (float) plan->segments[(size_t) i].second) return i;
    return -1;
}

void WaveView::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Ctrl + double click: the slice goes back to its original audio
    if (slipKey(e.mods) && !proc.isOneShot() && !proc.isOneShotLocked())
    {
        const int si = segmentAt(e.position.x);
        if (si >= 0) proc.resetSlip(si);
        return;
    }
    // v12: double-click on a dot's locator = back to the edge (for every chosen dot when the dot is chosen)
    if (trimMask != 0 && trimWhich >= 0)
    {
        float vals[bs::kPatternLen];
        for (auto& v : vals) v = trimWhich == bs::kDotEnd ? 1.0f : 0.0f;
        proc.setDotTrims(trimMask, trimWhich, vals);
        trimDot = -1;
    }
}

void WaveView::mouseDown(const juce::MouseEvent& e)
{
    slipSeg = -1;
    trimDot = -1; trimWhich = -1; trimMask = 0;
    if (!e.mods.isPopupMenu() && !slipKey(e.mods) && !proc.isOneShot())
    {
        // v12: a dot's locator under the mouse? (LOCK ONE SHOT: anywhere on its line; the dots: on the tab at the top)
        auto plan = proc.getPlan();
        const bool lockedView = plan && plan->locked;
        const auto b = getLocalBounds().toFloat().reduced(0.5f);
        const TrimHandle* best = nullptr;
        float bd = 6.0f;
        const auto hs = trimHandles();
        for (const auto& h : hs)
        {
            const float d = std::fabs(h.x - e.position.x);
            if (!lockedView && e.position.y > b.getY() + 26.0f) continue;
            if (lockedView)
            {
                const auto cell = lockCell(b, juce::jmax(1, plan->stepsPerBar), h.dot);
                if (e.position.y < cell.getY() - 2.0f || e.position.y > cell.getBottom() + 2.0f) continue;
                if (h.which == bs::kDotStart && e.position.x > h.x + 6.0f) continue;
                if (h.which == bs::kDotEnd && e.position.x < h.x - 6.0f) continue;
            }
            if (d < bd) { bd = d; best = &h; }
        }
        if (best != nullptr)
        {
            trimDot = best->dot;
            trimWhich = best->which;
            const uint32_t sel = proc.getSelectedDots();
            trimMask = (sel & (1u << trimDot)) != 0 ? sel : (1u << trimDot);
            for (int i = 0; i < bs::kPatternLen; ++i) trimFrom[(size_t) i] = proc.getDotTrim(i, trimWhich);
            trimStartX = e.position.x;
            return;
        }
    }
    if (proc.isOneShotLocked() && !proc.isOneShot())
    {
        // v11: LOCK ONE SHOT in the dots - a click on a copy chooses that dot (Ctrl = add / take out), again = let go
        auto plan = proc.getPlan();
        if (!plan || !plan->sample || e.mods.isPopupMenu()) return;
        const int spbL = juce::jmax(1, plan->stepsPerBar);
        const float cellW = ((float) getWidth() - 9.0f) / (float) spbL;
        const int i = juce::jlimit(0, juce::jmin(31, spbL - 1), (int) ((e.position.x - 4.5f) / cellW));
        const uint32_t bit = 1u << i, sel = proc.getSelectedDots();
        if (slipKey(e.mods)) proc.setSelectedDots(sel ^ bit);
        else proc.setSelectedDots(sel == bit ? 0u : bit);
        repaint();
        return;
    }
    if (slipKey(e.mods) && !proc.isOneShot())
    {
        slipSeg = segmentAt(e.position.x);
        slipFrom = proc.getSlip(slipSeg);
        slipStartX = e.position.x;
        return;
    }
    const float rs = proc.apvts.getRawParameterValue("start")->load(), re = proc.apvts.getRawParameterValue("end")->load();
    const float x = e.position.x;
    dragging = 0;
    pickedSeg = false;
    const bool locked = proc.isOneShotLocked(); // v10: LOCK ONE SHOT - the markers do not move
    if (!locked && std::fabs(x - normToX(rs)) < 8.0f) dragging = 1;
    else if (!locked && std::fabs(x - normToX(re)) < 8.0f) dragging = 2;
    if (dragging != 0)
    {
        if (auto* p = proc.apvts.getParameter(dragging == 1 ? "start" : "end")) p->beginChangeGesture();
        return;
    }
    if (proc.isEnvSlice() && !proc.isOneShot() && !e.mods.isPopupMenu())
    {
        // v10: SLICE lamp on - a click chooses the slice whose envelope the knobs edit; a second click lets it go
        const int si = segmentAt(x);
        if (si >= 0)
        {
            proc.setSelectedDots(0); // v11: a box and dots are never chosen together
            proc.setSelectedSeg(proc.getSelectedSeg() == si ? -1 : si);
            pickedSeg = true;
            repaint();
            return;
        }
    }
    if ((int) proc.apvts.getRawParameterValue("pick")->load() == 1)
    {
        const float n = xToNorm(x);
        const float tol = 8.0f / juce::jmax(1.0f, (float) getWidth() - 8.0f) * viewSpan();
        bool near = false;
        for (float c : proc.getManualCuts()) if (std::fabs(c - n) < tol) near = true;
        if (near || e.mods.isRightButtonDown()) proc.removeManualCut(n, tol);
        else proc.addManualCut(n);
    }
}

void WaveView::mouseDrag(const juce::MouseEvent& e)
{
    if (trimDot >= 0 && trimWhich >= 0)
    {
        // v12: move the locator of this dot; every chosen dot moves by the same amount (each from where it was)
        auto plan = proc.getPlan();
        if (!plan || !plan->sample) return;
        float span = 1.0f; // screen pixels for the whole chop of the grabbed dot
        if (plan->locked)
            span = lockCell(getLocalBounds().toFloat().reduced(0.5f), juce::jmax(1, plan->stepsPerBar), trimDot).getWidth() - 2.0f;
        else
        {
            int a = 0, bb = 0;
            if (!trimRange(trimDot, a, bb)) return;
            const int N = plan->sample->size();
            span = normToX((float) bb / (float) N) - normToX((float) a / (float) N);
        }
        const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
        const float delta = (e.position.x - trimStartX) / juce::jmax(4.0f, span) * fine;
        float vals[bs::kPatternLen];
        for (int i = 0; i < bs::kPatternLen; ++i) vals[i] = trimFrom[(size_t) i] + delta;
        proc.setDotTrims(trimMask, trimWhich, vals);
        repaint();
        return;
    }
    if (slipSeg >= 0)
    {
        // the wave follows the mouse: drag right = earlier audio comes into the slice
        const float dn = (e.position.x - slipStartX) / juce::jmax(1.0f, (float) getWidth() - 8.0f) * viewSpan();
        const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;
        proc.setSlip(slipSeg, slipFrom - dn * fine);
        return;
    }
    if (dragging == 0) return;
    float n = xToNorm(e.position.x);
    const float rs = proc.apvts.getRawParameterValue("start")->load(), re = proc.apvts.getRawParameterValue("end")->load();
    if (dragging == 1) n = juce::jmin(n, re - 0.01f); else n = juce::jmax(n, rs + 0.01f);
    if (auto* p = proc.apvts.getParameter(dragging == 1 ? "start" : "end")) p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(0.0f, 1.0f, n)));
}

void WaveView::mouseUp(const juce::MouseEvent&)
{
    slipSeg = -1;
    trimDot = -1; // v12 (trimMask / trimWhich stay for a double-click on the same locator)
    if (dragging != 0)
        if (auto* p = proc.apvts.getParameter(dragging == 1 ? "start" : "end")) p->endChangeGesture();
    dragging = 0;
}

// ============================================================================ beat bar
BeatBar::BeatBar(SlicerProcessor& p) : proc(p) { startTimerHz(30); }

float BeatBar::stepX(int i, int spb) const
{
    const float w = (float) getWidth() - 16.0f;
    const float swing = proc.apvts.getRawParameterValue("swing")->load();
    const float shift = (i & 1) ? swing * 0.33f / (float) spb : 0.0f;
    return 8.0f + ((float) i / (float) spb + shift) * w + 0.5f * w / (float) spb;
}

int BeatBar::stepAt(float x) const
{
    const int spb = bs::stepsPerBarFor((int) proc.apvts.getRawParameterValue("grid")->load());
    int best = 0; float bd = 1.0e9f;
    for (int i = 0; i < spb; ++i)
    {
        const float d = std::fabs(stepX(i, spb) - x);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void BeatBar::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff0f1115));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(juce::Colour(0xff555c69));
    g.drawRoundedRectangle(b, 6.0f, 1.0f);
    if (proc.isOneShot())
    {
        g.setColour(kDim);
        g.setFont(ui(12.0f));
        g.drawText("ONE SHOT  -  every key plays the sample once. All knobs shape it, no slicing and no rhythm.", getLocalBounds(),
                   juce::Justification::centred);
        return;
    }
    const int spb = bs::stepsPerBarFor((int) proc.apvts.getRawParameterValue("grid")->load());
    const float w = (float) getWidth() - 16.0f;
    const int beats = 4;
    const auto& ex = proc.getStepExtras();
    g.setFont(ui(10.0f, true));
    for (int k = 0; k < beats; ++k)
    {
        const float x = 8.0f + w * (float) k / (float) beats;
        g.setColour(juce::Colour(0xff5d6675));
        g.drawVerticalLine((int) x, 5.0f, b.getBottom() - 5.0f);
        g.setColour(juce::Colour(0xffcbd2de));
        g.drawText(juce::String(k + 1), (int) x + 4, (int) b.getBottom() - 13, 14, 12, juce::Justification::left);
    }
    // v13: the step that is heard right now (follows the sound, smooth on every screen frame)
    int heardTotal = 1;
    const double heard = proc.getHeardStep(heardTotal);
    const int live = heard >= 0.0 ? (int) heard : -1;
    const int liveInBar = live >= 0 ? live % spb : -1;
    const int cy = (int) (b.getY() + 22.0f);
    if (liveInBar < 0)
    {
        // v6: nothing plays - the playhead waits quietly at the start
        const float x = stepX(0, spb);
        g.setColour(LookAndFeel::cyan().withAlpha(0.13f));
        g.fillRoundedRectangle(x - 9.0f, (float) cy - 17.0f, 18.0f, 34.0f, 5.0f);
    }
    // v10: the selection is for one grid; a new grid starts with nothing selected
    const int gridNow = (int) proc.apvts.getRawParameterValue("grid")->load();
    if (gridNow != selGrid) { if (selGrid >= 0) proc.setSelectedDots(0); selGrid = gridNow; }
    const uint32_t selMask = proc.getSelectedDots(); // v11: the selection lives in the processor (the envelope knobs follow it)
    for (int i = 0; i < spb; ++i)
    {
        const float x = stepX(i, spb);
        const float v = proc.getStepVel(i);
        if (selMask & (1u << i))
        {
            // v10: a selected dot gets a light frame (also when it is off)
            const float hw = juce::jlimit(7.0f, 12.0f, (w / (float) spb) * 0.5f - 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.fillRoundedRectangle(x - hw, (float) cy - 15.0f, hw * 2.0f, 30.0f, 4.0f);
            g.setColour(juce::Colour(0xffe8ecf2).withAlpha(0.85f));
            g.drawRoundedRectangle(x - hw, (float) cy - 15.0f, hw * 2.0f, 30.0f, 4.0f, 1.2f);
        }
        if (i == liveInBar)
        {
            g.setColour(LookAndFeel::cyan().withAlpha(0.35f));
            g.fillRoundedRectangle(x - 9.0f, (float) cy - 17.0f, 18.0f, 34.0f, 5.0f);
        }
        if (v > 0.001f)
        {
            // v5: the size follows the level in dB (full dot = 0 dB = the original level)
            const float r = 3.0f + 7.0f * juce::jlimit(0.0f, 1.0f, 1.0f - bs::dotToDb(v) / bs::kDotMinDb);
            // long slice: a bar that reaches over the next steps
            if (ex.len[i] > 1)
            {
                g.setColour(LookAndFeel::orange().withAlpha(0.45f));
                const float x2 = stepX(juce::jmin(spb - 1, i + ex.len[i] - 1), spb) + (i + ex.len[i] - 1 >= spb ? 8.0f : 0.0f);
                g.fillRoundedRectangle(x, (float) cy - 3.0f, juce::jmax(4.0f, x2 - x), 6.0f, 3.0f);
            }
            // v8: a BALAGAN dot is red
            g.setColour(ex.bal[i] ? LookAndFeel::balagan() : (ex.rep[i] ? LookAndFeel::cyan() : LookAndFeel::orange()));
            g.fillEllipse(x - r, (float) cy - r, r * 2.0f, r * 2.0f);
            if (std::fabs(ex.pan[i]) > 0.005f)
            {
                // v8: panning - a black line on the dot, angled like a knob pointer (up = centre, left / right = pan)
                const float a = ex.pan[i] * juce::MathConstants<float>::pi * 0.75f;
                const float len = juce::jmax(3.5f, r * 0.95f);
                g.setColour(juce::Colour(0xff0d0f12));
                g.drawLine(x, (float) cy, x + std::sin(a) * len, (float) cy - std::cos(a) * len, r > 6.0f ? 2.0f : 1.5f);
            }
            g.setFont(ui(8.5f, true));
            if (std::abs(ex.semi[i]) > 0.01f)
            {
                g.setColour(LookAndFeel::cyan());
                g.drawText(juce::String::formatted("%+d", (int) std::round(ex.semi[i])), (int) x - 12, 0, 24, 10, juce::Justification::centred);
            }
            // v7: the letters sit in a short column right under their own dot (up to three):
            // R = reverse, W = wah, C = plays a copied chop
            juce::StringArray letters;
            if (ex.rev[i]) letters.add("R");
            if (ex.wah[i]) letters.add("W");
            if (ex.src[i] > 0 && (int) ex.src[i] != i + 1) letters.add("C");
            if (ex.env[i].any()) letters.add("E"); // v11: the dot has its own envelope
            g.setColour(LookAndFeel::cyan());
            g.setFont(ui(9.0f, true));
            for (int li = 0; li < letters.size() && li < 4; ++li)
                g.drawText(letters[li], (int) x - 8, cy + 11 + li * 9, 16, 9, juce::Justification::centred);
        }
        else
        {
            g.setColour(juce::Colour(0xff6b7280));
            g.drawEllipse(x - 2.5f, (float) cy - 2.5f, 5.0f, 5.0f, 1.0f);
        }
    }
    // v10: the selection box while it is dragged
    if (boxing)
    {
        const auto r = juce::Rectangle<float>(boxFrom, boxTo);
        g.setColour(LookAndFeel::cyan().withAlpha(0.12f));
        g.fillRect(r);
        g.setColour(LookAndFeel::cyan().withAlpha(0.8f));
        g.drawRect(r, 1.0f);
    }
    // v5: level readout while a dot is dragged
    if (downStep >= 0 && dragged && downStep < spb)
    {
        const float depth = proc.apvts.getRawParameterValue("velocity")->load();
        juce::String t = juce::String(dragDb, 1) + " dB";
        if (depth < 0.995f) t << "  (" << juce::String(dragDb * depth, 1) << " dB)";
        g.setFont(ui(10.5f, true));
        const float tw = (float) ui(10.5f, true).getStringWidth(t) + 12.0f;
        const float x = stepX(downStep, spb);
        float bx = x + 13.0f;
        if (bx + tw > b.getRight() - 4.0f) bx = x - 13.0f - tw;
        const juce::Rectangle<float> box(bx, (float) cy - 9.0f, tw, 18.0f);
        g.setColour(juce::Colour(0xf01b1e23));
        g.fillRoundedRectangle(box, 4.0f);
        g.setColour(LookAndFeel::orange());
        g.drawRoundedRectangle(box, 4.0f, 1.0f);
        g.setColour(kText);
        g.drawText(t, box.toNearestInt(), juce::Justification::centred);
    }
}

namespace
{
// v8: the panning slider lives only inside the right-click menu of a dot
class PanMenuItem : public juce::PopupMenu::CustomComponent
{
public:
    // v12: mask = the dots the slider moves (the clicked dot, or every chosen dot when the clicked one is chosen)
    PanMenuItem(SlicerProcessor& p, int step, uint32_t dots, juce::LookAndFeel& laf) : juce::PopupMenu::CustomComponent(false), proc(p), i(step), mask(dots)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(-100.0, 100.0, 1.0);
        slider.setValue(std::round(p.getStepExtras().pan[i] * 100.0f), juce::dontSendNotification);
        slider.setDoubleClickReturnValue(true, 0.0);
        slider.setLookAndFeel(&laf);
        slider.getProperties().set("accent", (int) LookAndFeel::orange().getARGB());
        slider.onValueChange = [this]
        {
            const float pan = (float) slider.getValue() / 100.0f;
            proc.editDots(mask, [pan](int k, float&, bs::StepExtras& ex) { ex.pan[k] = std::fabs(pan) < 0.005f ? 0.0f : juce::jlimit(-1.0f, 1.0f, pan); });
            repaint();
        };
        addAndMakeVisible(slider);
    }
    ~PanMenuItem() override { slider.setLookAndFeel(nullptr); }
    void getIdealSize(int& w, int& h) override { w = 240; h = 48; }
    void resized() override { slider.setBounds(24, 24, getWidth() - 48, 18); }
    void paint(juce::Graphics& g) override
    {
        const int v = juce::roundToInt(slider.getValue());
        g.setColour(kText);
        g.setFont(ui(11.5f, true));
        const juce::String t = v == 0 ? juce::String("Pan  C") : juce::String("Pan  ") + (v < 0 ? "L " : "R ") + juce::String(std::abs(v)) + " %";
        g.drawText(t, 0, 3, getWidth(), 16, juce::Justification::centred);
        g.setColour(kDim);
        g.setFont(ui(10.5f, true));
        g.drawText("L", 6, 24, 14, 18, juce::Justification::centred);
        g.drawText("R", getWidth() - 20, 24, 14, 18, juce::Justification::centred);
        g.setColour(kDim.withAlpha(0.5f));
        g.drawVerticalLine(getWidth() / 2, 22.0f, 26.0f);
    }

private:
    SlicerProcessor& proc;
    int i;
    uint32_t mask;
    juce::Slider slider;
};
} // namespace

void BeatBar::showStepMenu(int i)
{
    const auto& ex = proc.getStepExtras();
    // v12: right-click on a chosen dot = every choice goes to every chosen dot (Copy keeps copying from this dot)
    const int spbM = bs::stepsPerBarFor((int) proc.apvts.getRawParameterValue("grid")->load());
    uint32_t mask = 1u << i;
    const uint32_t sel = proc.getSelectedDots() & (spbM >= 32 ? 0xffffffffu : ((1u << spbM) - 1u));
    if ((sel & mask) != 0) mask = sel;
    juce::String head = "Step " + juce::String(i + 1);
    if (mask != (1u << i))
    {
        juce::StringArray nums;
        for (int k = 0; k < spbM && k < 32; ++k) if (mask & (1u << k)) nums.add(juce::String(k + 1));
        head = juce::String(nums.size()) + " chosen dots (" + (nums.size() <= 6 ? nums.joinIntoString(",") : nums[0] + "..." + nums[nums.size() - 1]) + ")";
    }
    juce::PopupMenu m, pitch, len;
    m.addSectionHeader(head);
    for (int st : { 12, 7, 5, 3, 0, -3, -5, -7, -12 })
        pitch.addItem(100 + st + 24, st == 0 ? juce::String("0") : juce::String::formatted("%+d st", st), true, (int) std::round(ex.semi[i]) == st);
    m.addSubMenu("Pitch", pitch);
    m.addItem(2, "Reverse", true, ex.rev[i] != 0);
    m.addItem(3, "Repeat previous slice (stutter)", i > 0, ex.rep[i] != 0);
    for (int L : { 1, 2, 4 }) len.addItem(10 + L, L == 1 ? juce::String("Normal") : "x" + juce::String(L) + " (long)", true, juce::jmax(1, (int) ex.len[i]) == L);
    m.addSubMenu("Length", len);
    m.addItem(5, "Wah  (W - amount from the WAH knob)", true, ex.wah[i] != 0); // v6
    m.addItem(8, "Balagan  (red dot - speed from the BALAGAN knob)", true, ex.bal[i] != 0); // v8
    {
        // v8: Panning - the slider opens inside the menu and is gone when the menu closes
        juce::PopupMenu pan;
        pan.addCustomItem(-1, std::make_unique<PanMenuItem>(proc, i, mask, getLookAndFeel()), nullptr, "Panning");
        pan.addSeparator();
        pan.addItem(9, "Centre", std::fabs(ex.pan[i]) > 0.005f);
        const juce::String now = std::fabs(ex.pan[i]) < 0.005f ? juce::String("C")
                                 : (ex.pan[i] < 0.0f ? "L " : "R ") + juce::String(juce::roundToInt(std::fabs(ex.pan[i]) * 100.0f)) + "%";
        m.addSubMenu("Panning  (" + now + ")", pan);
    }
    {
        // v7: the next dot / every dot plays exactly this chop
        const int spb = bs::stepsPerBarFor((int) proc.apvts.getRawParameterValue("grid")->load());
        m.addSeparator();
        m.addItem(6, "Copy to next dot", i + 1 < spb);
        m.addItem(7, "Copy to all dots");
    }
    m.addSeparator();
    m.addItem(4, "Clear step extras");
    juce::Component::SafePointer<BeatBar> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options(), [safe, i, mask](int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc;
        const auto& e = p.getStepExtras();
        // a toggle follows the clicked dot: it turns on (or off) for every dot it goes to
        const unsigned char revOn = e.rev[i] == 0, repOn = e.rep[i] == 0, wahOn = e.wah[i] == 0, balOn = e.bal[i] == 0;
        using Fn = std::function<void(int, float&, bs::StepExtras&)>;
        Fn fn;
        if (r >= 100) { const float st = (float) (r - 124); fn = [st](int k, float&, bs::StepExtras& x) { x.semi[k] = st; }; }
        else if (r == 2) fn = [revOn](int k, float&, bs::StepExtras& x) { x.rev[k] = revOn; };
        else if (r == 3) fn = [repOn](int k, float&, bs::StepExtras& x) { x.rep[k] = k > 0 ? repOn : 0; };
        else if (r >= 10 && r < 20) { const int L = r - 10; fn = [L](int k, float&, bs::StepExtras& x) { x.len[k] = (unsigned char) L; }; }
        else if (r == 5) fn = [wahOn](int k, float&, bs::StepExtras& x) { x.wah[k] = wahOn; };
        else if (r == 8) fn = [balOn](int k, float&, bs::StepExtras& x) { x.bal[k] = balOn; }; // v8
        else if (r == 9) fn = [](int k, float&, bs::StepExtras& x) { x.pan[k] = 0.0f; };
        else if (r == 6) { p.copyStepToNext(i); return; }
        else if (r == 7) { p.copyStepToAll(i); return; }
        else if (r == 4)
            fn = [](int k, float&, bs::StepExtras& x)
            {
                x.semi[k] = 0.0f; x.rev[k] = 0; x.rep[k] = 0; x.len[k] = 1; x.wah[k] = 0; x.src[k] = 0; x.bal[k] = 0; x.pan[k] = 0.0f;
                x.env[k] = bs::DotEnv(); // v11 envelope, v12 own knobs + own locators
            };
        if (fn) p.editDots(mask, fn);
    });
}

// v10: is the mouse on the dot of this step (or close enough to it)? Otherwise it is an empty spot.
bool BeatBar::onDot(juce::Point<float> p, int step) const
{
    const int spb = bs::stepsPerBarFor((int) proc.apvts.getRawParameterValue("grid")->load());
    const float half = juce::jlimit(6.0f, 12.0f, ((float) getWidth() - 16.0f) / (float) spb * 0.5f);
    const float cy = 0.5f + 22.0f;
    return std::fabs(p.x - stepX(step, spb)) <= half && std::fabs(p.y - cy) <= 16.0f;
}

void BeatBar::mouseDown(const juce::MouseEvent& e)
{
    boxing = false;
    if (proc.isOneShot()) { downStep = -1; return; }
    if (e.mods.isPopupMenu())
    {
        downStep = -1;
        showStepMenu(stepAt(e.position.x));
        return;
    }
    if (!onDot(e.position, stepAt(e.position.x)))
    {
        // v10: an empty spot - start a selection box (a plain click here only clears the selection)
        downStep = -1;
        boxing = true;
        boxFrom = boxTo = e.position;
        proc.setSelectedDots(0);
        repaint();
        return;
    }
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        // v11: Ctrl + click on a dot adds it to the chosen dots (or takes it out)
        const int st = stepAt(e.position.x);
        if (st >= 0 && st < 32) proc.setSelectedDots(proc.getSelectedDots() ^ (1u << st));
        downStep = -1;
        repaint();
        return;
    }
    downStep = stepAt(e.position.x);
    dragged = false;
    startY = lastY = e.position.y;
    const float v = proc.getStepVel(downStep);
    dragDb = v > 0.001f ? bs::dotToDb(v) : -12.0f; // a muted dot comes back at -12 dB
    downDb = dragDb;
    // v12: a chosen dot drags every chosen dot by the same dB, each from its own level (muted ones stay muted)
    dragMask = 0;
    const uint32_t sel = proc.getSelectedDots();
    if (sel & (1u << downStep))
    {
        dragMask = sel;
        for (int i = 0; i < bs::kPatternLen; ++i)
        {
            const float vi = proc.getStepVel(i);
            dragFromDb[i] = vi > 0.001f ? bs::dotToDb(vi) : -100.0f;
        }
        dragFromDb[downStep] = dragDb;
    }
}
void BeatBar::mouseDrag(const juce::MouseEvent& e)
{
    if (boxing)
    {
        boxTo = e.position.withX(juce::jlimit(0.0f, (float) getWidth(), e.position.x)).withY(juce::jlimit(0.0f, (float) getHeight(), e.position.y));
        // every dot whose column the box covers is selected
        const int spb = bs::stepsPerBarFor((int) proc.apvts.getRawParameterValue("grid")->load());
        const float lo = juce::jmin(boxFrom.x, boxTo.x), hi = juce::jmax(boxFrom.x, boxTo.x);
        const float half = juce::jlimit(3.0f, 10.0f, ((float) getWidth() - 16.0f) / (float) spb * 0.3f);
        uint32_t m = 0;
        for (int i = 0; i < spb && i < 32; ++i)
        {
            const float x = stepX(i, spb);
            if (x + half >= lo && x - half <= hi) m |= (1u << i);
        }
        proc.setSelectedDots(hi - lo > 3.0f ? m : 0u);
        repaint();
        return;
    }
    if (downStep < 0) return;
    if (!dragged && std::fabs(e.position.y - startY) < 4.0f) return;
    if (!dragged) lastY = e.position.y; // no jump when the drag starts
    dragged = true;
    // v5: relative and fine - 0.15 dB per pixel, Shift = 0.03 dB per pixel. Up = louder, never above 0 dB.
    const float perPx = e.mods.isShiftDown() ? 0.03f : 0.15f;
    dragDb = juce::jlimit(bs::kDotMinDb, 0.0f, dragDb - (e.position.y - lastY) * perPx);
    lastY = e.position.y;
    if (dragMask != 0 && (dragMask & ~(1u << downStep)) != 0)
    {
        // v12: every chosen dot moves by the same amount
        const float delta = dragDb - downDb;
        const int clicked = downStep;
        const float clickedDb = dragDb;
        const float* from = dragFromDb;
        proc.editDots(dragMask, [clicked, clickedDb, delta, from](int i, float& vel, bs::StepExtras&)
        {
            if (i == clicked) { vel = bs::dbToDot(clickedDb); return; }
            if (from[i] <= -99.0f) return; // a muted dot stays muted
            vel = bs::dbToDot(juce::jlimit(bs::kDotMinDb, 0.0f, from[i] + delta));
        });
        return;
    }
    proc.setStepVel(downStep, bs::dbToDot(dragDb));
}
void BeatBar::mouseUp(const juce::MouseEvent&)
{
    if (boxing) { boxing = false; repaint(); return; }
    if (downStep >= 0 && !dragged)
    {
        const uint32_t selMask = proc.getSelectedDots();
        if (selMask & (1u << downStep))
        {
            // v10: a click on one selected dot turns every selected dot the way of the clicked one:
            // clicked dot on -> all off, clicked dot off -> all on
            proc.setStepsOn(selMask, proc.getStepVel(downStep) <= 0.001f);
        }
        else proc.setStepVel(downStep, proc.getStepVel(downStep) > 0.001f ? 0.0f : 0.8f);
    }
    downStep = -1;
}

// ============================================================================ v6: LOOP button
LoopButton::LoopButton(SlicerProcessor& p) : proc(p)
{
    setTooltip("LOOP on: press a key once and the loop keeps playing. Press a key again = stop, again = start from the top.\n"
               "In ONE SHOT too: one press and the one shot plays again and again, from the left to the right locator.\n"
               "LOOP off: the loop plays only while the key is held.");
    if (auto* prm = proc.apvts.getParameter("latch"))
    {
        attach = std::make_unique<juce::ParameterAttachment>(*prm, [this](float v) { on = v > 0.5f; repaint(); });
        attach->sendInitialUpdate();
    }
    startTimerHz(15);
}
void LoopButton::timerCallback()
{
    const bool l = proc.isLooping();
    if (l != looping) { looping = l; repaint(); }
}
void LoopButton::mouseUp(const juce::MouseEvent& e)
{
    if (attach != nullptr && getLocalBounds().contains(e.getPosition())) attach->setValueAsCompleteGesture(on ? 0.0f : 1.0f);
}
void LoopButton::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    const auto orange = LookAndFeel::orange();
    g.setColour(isMouseOver() ? juce::Colour(0xff252a33) : juce::Colour(0xff1b1e23));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(on ? orange : juce::Colour(0xff555c69));
    g.drawRoundedRectangle(b, 6.0f, on ? 1.4f : 1.0f);
    // the lamp
    const float r = 7.0f;
    const juce::Point<float> c(b.getCentreX(), b.getY() + 17.0f);
    if (on)
    {
        g.setColour(orange.withAlpha(looping ? 0.45f : 0.25f));
        g.fillEllipse(c.x - r - 4.0f, c.y - r - 4.0f, (r + 4.0f) * 2.0f, (r + 4.0f) * 2.0f);
    }
    g.setGradientFill(juce::ColourGradient(on ? orange.brighter(0.5f) : juce::Colour(0xff3a3f48), c.x - 2.0f, c.y - 3.0f,
                                           on ? orange.darker(0.3f) : juce::Colour(0xff1f2228), c.x + r, c.y + r, true));
    g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
    g.setColour(on ? orange : kDim);
    g.setFont(ui(10.5f, true).withExtraKerningFactor(0.1f));
    g.drawText("LOOP", b.withTop(b.getY() + 30.0f).withHeight(16.0f).toNearestInt(), juce::Justification::centred);
}

// ============================================================================ v10: SAMPLE / SLICE lamps (SLICE ENVELOPE)
EnvModeLamps::EnvModeLamps(SlicerProcessor& p) : proc(p)
{
    setTooltip("SAMPLE: the envelope works as before, for every slice together.\n"
               "SLICE: the envelope fits inside every slice. Click a slice on the waveform to give it its own envelope "
               "(the knobs then change only that slice); click it again to let it go.");
    if (auto* prm = proc.apvts.getParameter("envmode"))
    {
        attach = std::make_unique<juce::ParameterAttachment>(*prm, [this](float v) { mode = v > 0.5f ? 1 : 0; repaint(); });
        attach->sendInitialUpdate();
    }
}
juce::Rectangle<float> EnvModeLamps::lamp(int i) const
{
    const float w = (float) getWidth() * 0.5f;
    return { w * (float) i, 0.0f, w, (float) getHeight() };
}
void EnvModeLamps::mouseUp(const juce::MouseEvent& e)
{
    if (attach == nullptr || !getLocalBounds().contains(e.getPosition())) return;
    const int i = lamp(1).contains(e.position) ? 1 : 0;
    if (i != mode) attach->setValueAsCompleteGesture((float) i);
    if (i == 0) proc.setSelectedSeg(-1);
}
void EnvModeLamps::paint(juce::Graphics& g)
{
    const auto cyan = LookAndFeel::cyan();
    const char* names[] = { "SAMPLE", "SLICE" };
    const auto mouse = getMouseXYRelative().toFloat();
    for (int i = 0; i < 2; ++i)
    {
        const auto r = lamp(i).reduced(2.0f, 1.0f);
        const bool on = mode == i, hot = isMouseOver() && r.contains(mouse);
        g.setColour(hot ? juce::Colour(0xff252a33) : juce::Colour(0xff1b1e23));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(on ? cyan : juce::Colour(0xff555c69));
        g.drawRoundedRectangle(r, 5.0f, on ? 1.3f : 1.0f);
        const float lr = 4.0f;
        const juce::Point<float> c(r.getX() + 10.0f, r.getCentreY());
        if (on)
        {
            g.setColour(cyan.withAlpha(0.35f));
            g.fillEllipse(c.x - lr - 3.0f, c.y - lr - 3.0f, (lr + 3.0f) * 2.0f, (lr + 3.0f) * 2.0f);
        }
        g.setColour(on ? cyan.brighter(0.3f) : juce::Colour(0xff3a3f48));
        g.fillEllipse(c.x - lr, c.y - lr, lr * 2.0f, lr * 2.0f);
        g.setColour(on ? cyan : kDim);
        g.setFont(ui(9.5f, true));
        g.drawText(names[i], r.withTrimmedLeft(17.0f).toNearestInt(), juce::Justification::centredLeft);
    }
}

// ============================================================================ v8: lamp button (SUPER)
LampButton::LampButton(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& label, juce::Colour colour)
    : text(label), col(colour)
{
    if (auto* prm = apvts.getParameter(id))
    {
        attach = std::make_unique<juce::ParameterAttachment>(*prm, [this](float v) { on = v > 0.5f; repaint(); });
        attach->sendInitialUpdate();
    }
}
void LampButton::mouseUp(const juce::MouseEvent& e)
{
    if (attach != nullptr && getLocalBounds().contains(e.getPosition())) attach->setValueAsCompleteGesture(on ? 0.0f : 1.0f);
}
void LampButton::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(isMouseOver() ? juce::Colour(0xff252a33) : juce::Colour(0xff1b1e23));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(on ? col : juce::Colour(0xff555c69));
    g.drawRoundedRectangle(b, 6.0f, on ? 1.4f : 1.0f);
    g.setColour(on ? col : kDim);
    g.setFont(ui(11.5f, true).withExtraKerningFactor(0.12f));
    g.drawText(text, b.withHeight(22.0f).withY(b.getY() + 6.0f).toNearestInt(), juce::Justification::centred);
    const float r = 9.0f;
    const juce::Point<float> c(b.getCentreX(), b.getY() + 44.0f);
    if (on)
    {
        g.setColour(col.withAlpha(0.35f));
        g.fillEllipse(c.x - r - 5.0f, c.y - r - 5.0f, (r + 5.0f) * 2.0f, (r + 5.0f) * 2.0f);
    }
    g.setGradientFill(juce::ColourGradient(on ? col.brighter(0.5f) : juce::Colour(0xff3a3f48), c.x - 2.0f, c.y - 3.0f,
                                           on ? col.darker(0.3f) : juce::Colour(0xff1f2228), c.x + r, c.y + r, true));
    g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
    g.setColour(on ? col.brighter(0.3f) : kDim.withAlpha(0.7f));
    g.setFont(ui(9.0f, true));
    g.drawText(on ? "RANDOM START" : "FROM START", b.withTop(b.getBottom() - 18.0f).toNearestInt(), juce::Justification::centred);
}

// ============================================================================ v12: BALAGAN FREE / BPM lamps + the value box
BalModeLamps::BalModeLamps(juce::AudioProcessorValueTreeState& apvts)
{
    setTooltip("FREE (default): the repeats speed up smoothly from 1 to 100, not tied to the beat.\n"
               "BPM: the knob picks a note value (1/8, 1/8T, 1/16, 1/16T, 1/32 ... 1/256) and every repeat lands on the beat.");
    if (auto* prm = apvts.getParameter("balmode"))
    {
        attach = std::make_unique<juce::ParameterAttachment>(*prm, [this](float v) { mode = v > 0.5f ? 1 : 0; repaint(); });
        attach->sendInitialUpdate();
    }
}
juce::Rectangle<float> BalModeLamps::lamp(int i) const
{
    const float h = (float) getHeight() * 0.5f;
    return { 0.0f, h * (float) i, (float) getWidth(), h };
}
void BalModeLamps::mouseUp(const juce::MouseEvent& e)
{
    if (attach == nullptr || !getLocalBounds().contains(e.getPosition())) return;
    const int i = lamp(1).contains(e.position) ? 1 : 0;
    if (i != mode) attach->setValueAsCompleteGesture((float) i);
}
void BalModeLamps::paint(juce::Graphics& g)
{
    const auto green = LookAndFeel::lampGreen();
    const char* names[] = { "FREE", "BPM" };
    const auto mouse = getMouseXYRelative().toFloat();
    for (int i = 0; i < 2; ++i)
    {
        const auto r = lamp(i).reduced(1.0f, 1.5f);
        const bool on = mode == i, hot = isMouseOver() && r.contains(mouse);
        g.setColour(hot ? juce::Colour(0xff252a33) : juce::Colour(0xff1b1e23));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(on ? green : juce::Colour(0xff555c69));
        g.drawRoundedRectangle(r, 5.0f, on ? 1.3f : 1.0f);
        const float lr = 4.0f;
        const juce::Point<float> c(r.getX() + 10.0f, r.getCentreY());
        if (on)
        {
            g.setColour(green.withAlpha(0.35f));
            g.fillEllipse(c.x - lr - 3.0f, c.y - lr - 3.0f, (lr + 3.0f) * 2.0f, (lr + 3.0f) * 2.0f);
        }
        g.setColour(on ? green.brighter(0.3f) : juce::Colour(0xff3a3f48));
        g.fillEllipse(c.x - lr, c.y - lr, lr * 2.0f, lr * 2.0f);
        g.setColour(on ? green : kDim);
        g.setFont(ui(9.5f, true));
        g.drawText(names[i], r.withTrimmedLeft(18.0f).toNearestInt(), juce::Justification::centredLeft);
    }
}

BalaganValue::BalaganValue(juce::AudioProcessorValueTreeState& apvts) : state(apvts)
{
    setTooltip("BALAGAN value (1 - 100). Double-click to type an exact value.");
    editor.setJustificationType(juce::Justification::centred);
    editor.setFont(ui(17.0f, true));
    editor.setColour(juce::Label::textColourId, kText);
    editor.setColour(juce::Label::backgroundWhenEditingColourId, kField);
    editor.setColour(juce::Label::textWhenEditingColourId, kText);
    editor.setColour(juce::Label::outlineWhenEditingColourId, LookAndFeel::balagan());
    editor.setVisible(false);
    addChildComponent(editor);
    editor.onEditorShow = [this]
    {
        if (auto* te = editor.getCurrentTextEditor())
        {
            te->setText({}, false); // the number is cleared, ready to type
            te->setInputRestrictions(3, "0123456789");
            te->setJustification(juce::Justification::centred);
        }
    };
    editor.onTextChange = [this]
    {
        const auto t = editor.getText().trim();
        editor.setVisible(false);
        if (t.isEmpty()) { repaint(); return; }
        if (auto* p = state.getParameter("balagan"))
        {
            const float v = juce::jlimit(1.0f, 100.0f, (float) t.getIntValue());
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(v));
            p->endChangeGesture();
        }
        repaint();
    };
    editor.onEditorHide = [this] { editor.setVisible(false); repaint(); };
    startTimerHz(15);
}
void BalaganValue::resized() { editor.setBounds(getLocalBounds().withHeight(juce::jmin(getHeight(), 30))); }
void BalaganValue::mouseDoubleClick(const juce::MouseEvent&)
{
    editor.setText({}, juce::dontSendNotification);
    editor.setVisible(true);
    editor.showEditor();
}
void BalaganValue::paint(juce::Graphics& g)
{
    const float v = state.getRawParameterValue("balagan")->load();
    const bool bpm = state.getRawParameterValue("balmode")->load() > 0.5f;
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    const auto top = r.withHeight(juce::jmin(r.getHeight(), 30.0f));
    g.setColour(kField);
    g.fillRoundedRectangle(top, 5.0f);
    g.setColour(isMouseOver() ? LookAndFeel::balagan() : kFieldEdge);
    g.drawRoundedRectangle(top, 5.0f, 1.0f);
    if (!editor.isVisible())
    {
        g.setColour(kText);
        g.setFont(ui(17.0f, true));
        g.drawText(juce::String(juce::roundToInt(v)), top.toNearestInt(), juce::Justification::centred);
    }
    // under it: the note value in BPM mode, otherwise the range
    g.setFont(ui(9.5f, true));
    g.setColour(bpm ? LookAndFeel::lampGreen() : kDim.withAlpha(0.8f));
    g.drawText(bpm ? juce::String(bs::balaganDiv(v).name) : juce::String("1 - 100"), r.withTrimmedTop(top.getHeight() + 2.0f).toNearestInt(),
               juce::Justification::centredTop);
}

// ============================================================================ v7: choice button + ADD LAYER
ChoiceMenuButton::ChoiceMenuButton(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    getProperties().set("small", true);
    param = apvts.getParameter(id);
    if (param != nullptr)
    {
        attach = std::make_unique<juce::ParameterAttachment>(*param, [this](float v)
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(param))
                setButtonText(c->choices[juce::jlimit(0, c->choices.size() - 1, juce::roundToInt(v))] + "  " + juce::String::fromUTF8("\xe2\x96\xbe"));
        });
        attach->sendInitialUpdate();
    }
}
void ChoiceMenuButton::clicked()
{
    auto* c = dynamic_cast<juce::AudioParameterChoice*>(param);
    if (c == nullptr) return;
    juce::PopupMenu m;
    for (int i = 0; i < c->choices.size(); ++i) m.addItem(i + 1, c->choices[i], true, i == c->getIndex());
    juce::Component::SafePointer<ChoiceMenuButton> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()), [safe](int r)
    {
        if (safe == nullptr || r == 0 || safe->attach == nullptr) return;
        safe->attach->setValueAsCompleteGesture((float) (r - 1));
    });
}

LayerView::LayerView(SlicerProcessor& p) : proc(p)
{
    setTooltip("Drop a sound here (or click to choose one). It is blended into every chop.\n"
               "Drag left / right = where it starts (the left locator is where the chop starts), Shift = fine.\n"
               "The silence before the sound is skipped by itself. Right-click = align again / remove.");
    startTimerHz(15);
}
// v13: the whole layer sound fits between the two locators when it starts at its top
float LayerView::pxPerSec() const
{
    auto l = proc.getLayer();
    const double len = l && l->size() > 1 ? (double) l->size() / l->sr : 1.0;
    return (float) ((double) juce::jmax(10, getWidth() - 8) / juce::jmax(0.02, len));
}
void LayerView::timerCallback()
{
    auto l = proc.getLayer();
    if (l.get() != shownFor)
    {
        // peaks of the whole sound (fine bins, drawn at any start point)
        shownFor = l.get();
        peaks.clear();
        if (l && l->size() > 1)
        {
            const int bins = 2048;
            for (int c = 0; c < bins; ++c)
            {
                const int a = (int) ((int64_t) l->size() * c / bins), e = juce::jmax(a + 1, (int) ((int64_t) l->size() * (c + 1) / bins));
                float mn = 0.0f, mx = 0.0f;
                const int stride = juce::jmax(1, (e - a) / 64);
                for (int i = a; i < e && i < l->size(); i += stride)
                {
                    const float v = 0.5f * (l->L[(size_t) i] + l->R[(size_t) i]);
                    mn = juce::jmin(mn, v); mx = juce::jmax(mx, v);
                }
                peaks.push_back({ mn, mx });
            }
        }
        repaint();
    }
    // the start point and the layer's envelope: drawn again when they change
    juce::uint64 h = 1469598103934665603ull;
    for (const char* id : { "layatk", "laydec", "laysus", "layrel", "layunlink", "layermix" })
    {
        h ^= (juce::uint64) juce::roundToInt(proc.apvts.getRawParameterValue(id)->load() * 1000.0f); h *= 1099511628211ull;
    }
    h ^= (juce::uint64) juce::roundToInt(proc.getLayerStart() * 100000.0f); h *= 1099511628211ull;
    if (h != shownSig) { shownSig = h; repaint(); }
}
void LayerView::paint(juce::Graphics& g)
{
    if (proc.getLayer().get() != shownFor) timerCallback(); // a new sound: draw it right away
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff0f1115));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(hover ? LookAndFeel::orange() : juce::Colour(0xff555c69));
    g.drawRoundedRectangle(b, 6.0f, hover ? 1.5f : 1.0f);
    auto l = proc.getLayer();
    if (peaks.empty() || !l)
    {
        g.setColour(juce::Colour(0xff8a93a3));
        g.setFont(ui(13.0f, true));
        g.drawText("Drop sound", getLocalBounds(), juce::Justification::centred);
        return;
    }
    const float cy = b.getCentreY(), h = b.getHeight() * 0.42f;
    float pk = 0.05f;
    for (auto& p : peaks) pk = juce::jmax(pk, std::fabs(p.first), std::fabs(p.second));
    const float xL = 4.0f, xR = (float) getWidth() - 4.0f, pps = pxPerSec();
    const double lenSec = (double) l->size() / l->sr, start = (double) proc.getLayerStart();
    const double origin = juce::jmax(0.0, start);
    auto raw = [this](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
    const double atk = raw("layatk") * 0.001, dec = raw("laydec") * 0.001, rel = juce::jmax(0.001, raw("layrel") * 0.001);
    const float sus = raw("laysus");
    // the layer's own envelope: Attack from where it starts, Release ends where the sound ends
    auto envAt = [&](double t) -> float
    {
        const double since = t - origin, left = lenSec - t;
        if (since < 0.0) return 0.0f;
        float e = since < atk ? (float) (since / juce::jmax(1.0e-5, atk))
                              : (since < atk + dec ? 1.0f + (sus - 1.0f) * (float) ((since - atk) / juce::jmax(1.0e-5, dec)) : sus);
        if (left < rel) e *= (float) juce::jmax(0.0, left / rel);
        return e;
    };
    g.saveState();
    g.reduceClipRegion(b.reduced(1.0f).toNearestInt());
    juce::Path envLine;
    bool started = false;
    const int nb = (int) peaks.size();
    for (int x = (int) xL; x < (int) xR; ++x)
    {
        const double t = start + (double) ((float) x - xL) / pps; // the time in the sound that this column plays
        if (t < 0.0 || t >= lenSec) continue;
        const int bi = juce::jlimit(0, nb - 1, (int) (t / lenSec * nb)), bj = juce::jlimit(bi, nb - 1, (int) ((t + 1.0 / pps) / lenSec * nb));
        float mn = 0.0f, mx = 0.0f;
        for (int k = bi; k <= bj; ++k) { mn = juce::jmin(mn, peaks[(size_t) k].first); mx = juce::jmax(mx, peaks[(size_t) k].second); }
        const float e = envAt(t);
        g.setColour(LookAndFeel::orange().withAlpha(0.22f)); // the sound as it is (dim)
        g.drawVerticalLine(x, cy - mx / pk * h, cy - mn / pk * h + 1.0f);
        if (e > 0.001f)
        {
            g.setColour(LookAndFeel::orange()); // what is heard, shaped by the layer's envelope
            g.drawVerticalLine(x, cy - mx * e / pk * h, cy - mn * e / pk * h + 1.0f);
        }
        const float ly = cy - e * h;
        if (!started) { envLine.startNewSubPath((float) x, ly); started = true; } else envLine.lineTo((float) x, ly);
    }
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.strokePath(envLine, juce::PathStrokeType(1.1f));
    g.restoreState();
    // the two locators (they stay at the walls; the sound slides under them)
    g.setColour(LookAndFeel::orange());
    g.fillRect(xL - 2.0f, b.getY() + 1.0f, 3.0f, b.getHeight() - 2.0f);
    g.fillRect(xR - 1.0f, b.getY() + 1.0f, 3.0f, b.getHeight() - 2.0f);
    if (dragging && moved)
    {
        const juce::String t = start >= 0.0 ? "starts " + juce::String(start * 1000.0, 1) + " ms in" : "waits " + juce::String(-start * 1000.0, 1) + " ms";
        g.setColour(kText);
        g.setFont(ui(9.5f, true));
        g.drawText(t, getLocalBounds().reduced(8, 4), juce::Justification::bottomLeft);
    }
}
void LayerView::mouseDown(const juce::MouseEvent& e)
{
    downX = lastX = e.position.x;
    moved = false;
    dragging = !e.mods.isPopupMenu() && proc.getLayer() != nullptr;
    dragStart = proc.getLayerStart();
}
void LayerView::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging) return;
    if (std::fabs(e.position.x - downX) > 3.0f) moved = true;
    const float dx = e.position.x - lastX;
    lastX = e.position.x;
    if (!moved) return;
    // drag right = the sound comes later, drag left = earlier. Shift = 10x finer
    proc.setLayerStart(proc.getLayerStart() - dx / pxPerSec() * (e.mods.isShiftDown() ? 0.1f : 1.0f));
    repaint();
}
void LayerView::mouseUp(const juce::MouseEvent& e)
{
    const bool wasDrag = dragging && moved;
    dragging = false;
    repaint();
    if (wasDrag || !getLocalBounds().contains(e.getPosition())) return;
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem(2, "Align start (skip the silence)", proc.getLayer() != nullptr);
        m.addItem(1, "Remove sound", proc.getLayer() != nullptr);
        juce::Component::SafePointer<LayerView> safe(this);
        m.showMenuAsync(juce::PopupMenu::Options(), [safe](int r)
        {
            if (safe == nullptr) return;
            if (r == 1) safe->proc.clearLayer();
            if (r == 2) safe->proc.autoAlignLayer();
        });
        return;
    }
    chooser = std::make_unique<juce::FileChooser>("Choose a layer sound", juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                  "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    juce::Component::SafePointer<LayerView> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser& fc) { if (safe != nullptr && fc.getResult().existsAsFile()) safe->proc.loadLayer(fc.getResult()); });
}

// ============================================================================ v13: UNLINK + its list, the small boxes
UnlinkButton::UnlinkButton(SlicerProcessor& p) : proc(p)
{
    setTooltip("Unlink from: Balagan, Pitch, Filter, LFO, Chorus, Unison, Wah\n"
               "The dots, the timing and the envelope stay linked. Arrow = choose what to link back.");
    if (auto* prm = proc.apvts.getParameter("layunlink"))
    {
        attach = std::make_unique<juce::ParameterAttachment>(*prm, [this](float v) { on = v > 0.5f; repaint(); });
        attach->sendInitialUpdate();
    }
}
void UnlinkButton::mouseUp(const juce::MouseEvent& e)
{
    if (!getLocalBounds().contains(e.getPosition())) return;
    if (arrowArea().contains(e.position)) { if (onArrow) onArrow(); return; }
    if (attach != nullptr) attach->setValueAsCompleteGesture(on ? 0.0f : 1.0f);
}
void UnlinkButton::paint(juce::Graphics& g)
{
    const auto green = LookAndFeel::lampGreen();
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    const auto mouse = getMouseXYRelative().toFloat();
    const bool hotArrow = isMouseOver() && arrowArea().contains(mouse), hotMain = isMouseOver() && !hotArrow;
    g.setColour(juce::Colour(0xff1b1e23));
    g.fillRoundedRectangle(r, 5.0f);
    if (hotMain || hotArrow)
    {
        g.setColour(juce::Colour(0xff252a33));
        g.fillRoundedRectangle(hotArrow ? arrowArea().reduced(1.0f) : r.withTrimmedRight(20.0f).reduced(1.0f), 4.0f);
    }
    g.setColour(on ? green : juce::Colour(0xff555c69));
    g.drawRoundedRectangle(r, 5.0f, on ? 1.3f : 1.0f);
    g.drawVerticalLine((int) arrowArea().getX(), r.getY() + 3.0f, r.getBottom() - 3.0f);
    const float lr = 4.0f;
    const juce::Point<float> c(r.getX() + 10.0f, r.getCentreY());
    if (on)
    {
        g.setColour(green.withAlpha(0.35f));
        g.fillEllipse(c.x - lr - 3.0f, c.y - lr - 3.0f, (lr + 3.0f) * 2.0f, (lr + 3.0f) * 2.0f);
    }
    g.setColour(on ? green.brighter(0.3f) : juce::Colour(0xff3a3f48));
    g.fillEllipse(c.x - lr, c.y - lr, lr * 2.0f, lr * 2.0f);
    g.setColour(on ? green : kDim);
    g.setFont(ui(9.5f, true));
    g.drawText("UNLINK", r.withTrimmedLeft(18.0f).withTrimmedRight(20.0f).toNearestInt(), juce::Justification::centredLeft);
    juce::Path tri;
    const auto a = arrowArea().getCentre();
    tri.addTriangle(a.x - 4.0f, a.y - 2.0f, a.x + 4.0f, a.y - 2.0f, a.x, a.y + 3.0f);
    g.setColour(on ? green : kDim);
    g.fillPath(tri);
}
void FloatBox::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(r.translated(0.0f, 3.0f), 9.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a404a), 0.0f, 0.0f, juce::Colour(0xff23272e), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(accent);
    g.drawRoundedRectangle(r, 9.0f, 1.3f);
    g.setFont(ui(11.0f, true).withExtraKerningFactor(0.14f));
    g.drawText(title, 12, 6, getWidth() - 44, 16, juce::Justification::centredLeft);
}
void LinkList::paint(juce::Graphics& g)
{
    const bool un = proc.isLayerUnlinked();
    const int mask = proc.getLayerLinks();
    g.setFont(ui(9.5f, true));
    g.setColour(un ? kDim : LookAndFeel::lampGreen().withAlpha(0.85f));
    g.drawText(un ? "Filled = linked back" : "UNLINK is off: the layer follows all", 4, 0, getWidth() - 8, kTop - 4, juce::Justification::centredLeft);
    const auto mouse = getMouseXYRelative();
    for (int i = 0; i < bs::kLinkCount; ++i)
    {
        const auto row = juce::Rectangle<int>(0, kTop + i * kRowH, getWidth(), kRowH);
        const bool linked = un && (mask & bs::layerLinkBit(i)) != 0;
        if (un && isMouseOver() && row.contains(mouse)) { g.setColour(juce::Colour(0xff2c313a)); g.fillRoundedRectangle(row.toFloat().reduced(1.0f), 4.0f); }
        const float alpha = un ? 1.0f : 0.4f;
        const juce::Point<float> c(12.0f, (float) row.getCentreY());
        g.setColour(LookAndFeel::lampGreen().withAlpha(alpha));
        if (linked) g.fillEllipse(c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f);
        g.drawEllipse(c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f, 1.3f);
        g.setColour(kText.withAlpha(alpha));
        g.setFont(ui(11.0f, linked));
        g.drawText(juce::String("Link to ") + bs::layerLinkName(i), row.withTrimmedLeft(26), juce::Justification::centredLeft);
    }
}
void LinkList::mouseUp(const juce::MouseEvent& e)
{
    if (!proc.isLayerUnlinked()) return; // nothing to link back while everything is linked
    const int i = (e.getPosition().y - kTop) / kRowH;
    if (e.getPosition().y < kTop || i < 0 || i >= bs::kLinkCount) return;
    proc.setLayerLinks(proc.getLayerLinks() ^ bs::layerLinkBit(i));
    repaint();
}

bool LayerView::isInterestedInFileDrag(const juce::StringArray& f)
{
    for (auto& s : f)
    {
        const auto x = juce::File(s).getFileExtension().toLowerCase();
        if (x == ".wav" || x == ".aif" || x == ".aiff" || x == ".flac" || x == ".mp3" || x == ".ogg") return true;
    }
    return false;
}
void LayerView::filesDropped(const juce::StringArray& f, int, int)
{
    hover = false;
    for (auto& s : f) if (proc.loadLayer(juce::File(s))) break;
    repaint();
}

// ============================================================================ rhythm bar (header)
RhythmBar::RhythmBar(SlicerProcessor& p) : proc(p)
{
    for (auto* c : { &prev, &next, &save, &nameButton }) addAndMakeVisible(*c);
    prev.onClick = [this] { step(-1); };
    next.onClick = [this] { step(+1); };
    save.onClick = [this] { askSave(); };
    nameButton.onClick = [this] { showMenu(); };
    startTimerHz(10);
    timerCallback();
}
void RhythmBar::resized()
{
    prev.setBounds(0, 0, 34, 34);
    nameButton.setBounds(40, 0, 250, 34);
    next.setBounds(296, 0, 34, 34);
    save.setBounds(336, 0, 50, 34);
}
void RhythmBar::timerCallback()
{
    // v7: no preset chosen (or edited by hand) -> the button reads "Presets"; Custom lives inside the list
    const bool none = proc.getCurrentFactory() < 0 && proc.getCurrentRhythm() < 0;
    const auto n = none ? juce::String("Presets") : proc.getRhythmName();
    if (n != shown)
    {
        shown = n;
        nameButton.setButtonText(n + "   " + juce::String::fromUTF8("\xe2\x96\xbe"));
    }
}
void RhythmBar::step(int d)
{
    // the arrows walk through the factory presets first, then the rhythms
    const int nf = (int) bs::factoryPresets().size(), nr = (int) bs::rhythms().size(), n = nf + nr;
    int cur = proc.getCurrentFactory() >= 0 ? proc.getCurrentFactory() : (proc.getCurrentRhythm() >= 0 ? nf + proc.getCurrentRhythm() : -1);
    const int nx = cur < 0 ? 0 : (cur + d + n) % n;
    if (nx < nf) proc.applyFactoryPreset(nx);
    else proc.applyRhythm(nx - nf);
}
// v13: the list is built from libraries - every library is one line with an arrow that opens its presets
// (Factory, Rhythms, Monkey Library, User, and every folder in "Documents\Psycho Beat Lab Presets").
// The folders are read again every time the list opens, so a new folder or preset shows up by itself.
void RhythmBar::showMenu()
{
    juce::PopupMenu menu;
    const auto& fac = bs::factoryPresets();
    const int curF = proc.getCurrentFactory(), curR = proc.getCurrentRhythm();
    menu.addItem(400, kDefaultItem, true, false); // v9: resets everything. v10: no tick, "Default" drawn in orange, "Reset Plugin"
    menu.addSeparator();
    juce::PopupMenu facMenu, rhyMenu, monkeyMenu;
    for (int i = 0; i < (int) fac.size(); ++i)
        (fac[(size_t) i].library ? monkeyMenu : facMenu).addItem(500 + i, fac[(size_t) i].name, true, i == curF);
    const auto& all = bs::rhythms();
    for (int i = 0; i < (int) all.size(); ++i) rhyMenu.addItem(1000 + i, all[(size_t) i].name, true, curF < 0 && i == curR);
    const bool facOn = curF >= 0 && !fac[(size_t) curF].library, monkeyOn = curF >= 0 && fac[(size_t) curF].library;
    menu.addSubMenu("Factory presets", facMenu, true, nullptr, facOn);
    menu.addSubMenu("Rhythms", rhyMenu, true, nullptr, curF < 0 && curR >= 0);
    menu.addSubMenu("Monkey Library", monkeyMenu, true, nullptr, monkeyOn);
    menu.addSeparator();
    // the libraries on the computer
    auto files = std::make_shared<juce::Array<juce::File>>();
    const auto shownName = proc.getPresetName();
    const bool userPresetOn = curF < 0 && curR < 0;
    std::function<bool(juce::PopupMenu&, const juce::Array<juce::File>&, const juce::File&)> fill;
    fill = [&](juce::PopupMenu& m, const juce::Array<juce::File>& presets, const juce::File& dir) -> bool
    {
        bool ticked = false;
        if (dir != juce::File())
        {
            auto subs = dir.findChildFiles(juce::File::findDirectories, false);
            subs.sort();
            for (auto& d : subs)
            {
                if (d.isHidden()) continue;
                juce::PopupMenu sub;
                const bool t = fill(sub, SlicerProcessor::getPresetFilesIn(d), d);
                m.addSubMenu(d.getFileName(), sub, true, nullptr, t);
                ticked = ticked || t;
            }
        }
        auto sorted = presets;
        sorted.sort();
        for (auto& f : sorted)
        {
            const bool t = userPresetOn && f.getFileNameWithoutExtension() == shownName;
            files->add(f);
            m.addItem(5000 + files->size() - 1, f.getFileNameWithoutExtension(), true, t);
            ticked = ticked || t;
        }
        if (m.getNumItems() == 0) m.addItem(-1, "(empty)", false, false);
        return ticked;
    };
    {
        juce::PopupMenu userMenu;
        const bool t = fill(userMenu, proc.getUserPresetFiles(), SlicerProcessor::hasLibraryRoot() ? SlicerProcessor::getUserPresetDir() : juce::File());
        menu.addSubMenu("User", userMenu, true, nullptr, t);
    }
    for (auto& lib : proc.getLibraries())
    {
        juce::PopupMenu libMenu;
        const bool t = fill(libMenu, SlicerProcessor::getPresetFilesIn(lib), lib);
        menu.addSubMenu(lib.getFileName(), libMenu, true, nullptr, t);
    }
    menu.addSeparator();
    menu.addItem(9000, "Save current sound as...");
    menu.addItem(9003, "Create New Library...");  // v13
    menu.addItem(9002, "Import Preset...");  // v12: a preset file someone sent - added to My presets
    menu.addItem(9001, "Export Preset...");  // v12: the last line - a file with the sound inside, to send to someone
    juce::Component::SafePointer<RhythmBar> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&nameButton).withMinimumWidth(nameButton.getWidth()),
                       [safe, files](int r)
                       {
                           if (safe == nullptr || r == 0) return;
                           if (r == 400) safe->proc.applyCustom();
                           else if (r >= 500 && r < 1000) safe->proc.applyFactoryPreset(r - 500);
                           else if (r >= 1000 && r < 5000) safe->proc.applyRhythm(r - 1000);
                           else if (r >= 5000 && r < 9000 && r - 5000 < files->size()) safe->proc.loadUserPreset((*files)[r - 5000]);
                           else if (r == 9000) safe->askSave();
                           else if (r == 9001) safe->askExport();
                           else if (r == 9002) safe->askImport();
                           else if (r == 9003) safe->askNewLibrary();
                       });
}

// v13: Create New Library - a save window opens in the presets folder; type the name and press Save: a new folder
// (library) is made there. Copy presets into it and it shows in the list with its own arrow.
void RhythmBar::askNewLibrary()
{
    if (!SlicerProcessor::hasLibraryRoot())
    {
        juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::InfoIcon, "Create New Library",
            "The presets folder (Documents\\Psycho Beat Lab Presets) is made when the plugin is installed with Install_PsychoBeatLab.bat.\n"
            "After that, new libraries can be made here.", "OK", this), nullptr);
        return;
    }
    const auto root = SlicerProcessor::getLibraryRoot();
    chooser = std::make_unique<juce::FileChooser>("Create New Library - type a name and press Save", root.getChildFile("New Library"), "*");
    juce::Component::SafePointer<RhythmBar> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [safe](const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File()) return;
        const auto made = SlicerProcessor::createLibrary(fc.getResult().getFileNameWithoutExtension());
        if (made == juce::File())
            juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon, "Create New Library",
                                                                                 "The folder could not be made.", "OK", safe.getComponent()), nullptr);
    });
}

// v12: Export Preset - the current sound as one file (the sample and the layer sound travel inside it)
void RhythmBar::askExport()
{
    juce::String name = proc.getPresetName();
    if (name.isEmpty() || name == "Init" || name == "Default") name = "My Psycho Beat Lab preset";
    const auto start = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(juce::File::createLegalFileName(name) + ".bspreset");
    chooser = std::make_unique<juce::FileChooser>("Export Preset", start, "*.bspreset");
    juce::Component::SafePointer<RhythmBar> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
                         [safe](const juce::FileChooser& fc)
                         {
                             if (safe == nullptr || fc.getResult() == juce::File()) return;
                             const bool ok = safe->proc.exportPreset(fc.getResult());
                             juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                                 ok ? "Preset exported" : "Export failed",
                                 ok ? "Saved as " + fc.getResult().withFileExtension(".bspreset").getFileName() + "\nThe sample is inside the file - send it as it is."
                                    : juce::String("The file could not be written."), "OK", safe.getComponent()), nullptr);
                         });
}
// v12: Import Preset - opens a preset file, adds it to My presets and loads it
void RhythmBar::askImport()
{
    chooser = std::make_unique<juce::FileChooser>("Import Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.bspreset");
    juce::Component::SafePointer<RhythmBar> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe](const juce::FileChooser& fc)
    {
        if (safe == nullptr || !fc.getResult().existsAsFile()) return;
        const auto added = safe->proc.importPreset(fc.getResult());
        if (added.existsAsFile()) safe->proc.loadUserPreset(added);
        else juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon, "Import failed",
                                                                                 "This file is not a Psycho Beat Lab preset.", "OK", safe.getComponent()), nullptr);
    });
}
void RhythmBar::askSave()
{
    auto* w = new juce::AlertWindow("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("name", proc.getPresetName());
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<RhythmBar> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w](int r)
    {
        if (r != 1 || safe == nullptr) return;
        const juce::String name = w->getTextEditorContents("name");
        if (!SlicerProcessor::userPresetExists(name)) { safe->proc.saveUserPreset(name); return; }
        // v11: the name is already taken - ask before the old preset is replaced (OK = overwrite, Cancel = nothing changes)
        juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOkCancel(juce::MessageBoxIconType::WarningIcon, "Overwrite Preset?",
                                         "A preset named \"" + juce::File::createLegalFileName(name.trim()) + "\" already exists.\nReplace it with the current sound?",
                                         "OK", "Cancel", safe.getComponent()),
                                     [safe, name](int ok) { if (ok == 1 && safe != nullptr) safe->proc.saveUserPreset(name); });
    }), true);
}

// ============================================================================ v4: knob modulation (LFO routing)
void KnobCell::setModTarget(SlicerProcessor* p, int modIndex)
{
    proc = p;
    modIdx = modIndex;
    if (modIdx >= 0)
    {
        slider.addMouseListener(this, false);
        startTimerHz(15);
    }
}
void KnobCell::timerCallback()
{
    if (proc == nullptr) return;
    const bool on = proc->isModOn(modIdx), eOn = proc->isEnvModOn(modIdx);
    const float a = proc->getModAmount(modIdx), ea = proc->getEnvModAmount(modIdx);
    if (on != shownOn || std::fabs(a - shownAmt) > 1.0e-4f || eOn != shownEnvOn || std::fabs(ea - shownEnvAmt) > 1.0e-4f)
    {
        shownOn = on; shownAmt = a; shownEnvOn = eOn; shownEnvAmt = ea;
        repaint();
    }
}
void KnobCell::itemDropped(const SourceDetails& d)
{
    dragOver = false;
    if (proc != nullptr)
    {
        if (d.description.toString() == "env") proc->addEnvMod(modIdx); // v12
        else proc->addMod(modIdx);
    }
    repaint();
}
// v12: right-click on a knob with the LFO and / or the ENV on it - UNDRAG each one on its own (the knob then works as before)
void KnobCell::mouseDown(const juce::MouseEvent& e)
{
    if (proc == nullptr || !e.mods.isPopupMenu()) return;
    const bool lfoOn = proc->isModOn(modIdx), envOn = proc->isEnvModOn(modIdx);
    if (!lfoOn && !envOn) return;
    juce::PopupMenu m;
    const juce::String label(bs::modInfo(modIdx).label);
    if (lfoOn)
    {
        m.addSectionHeader("LFO -> " + label);
        m.addItem(1, "UNDRAG LFO");
        m.addItem(2, "Invert LFO amount");
    }
    if (envOn)
    {
        if (lfoOn) m.addSeparator();
        m.addSectionHeader("ENV -> " + label);
        m.addItem(3, "UNDRAG ENV");
        m.addItem(4, "Invert ENV amount");
    }
    juce::Component::SafePointer<KnobCell> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options(), [safe](int r)
    {
        if (safe == nullptr || safe->proc == nullptr) return;
        auto* p = safe->proc;
        const int d = safe->modIdx;
        if (r == 1) p->removeMod(d);
        else if (r == 2) p->setModAmount(d, -p->getModAmount(d));
        else if (r == 3) p->removeEnvMod(d);
        else if (r == 4) p->setEnvModAmount(d, -p->getEnvModAmount(d));
    });
}
void KnobCell::paintOverChildren(juce::Graphics& g)
{
    if (modIdx < 0 || proc == nullptr) return;
    // same geometry as LookAndFeel::drawRotarySlider
    const auto sb = slider.getBounds();
    const float textH = getProperties().contains("noTextBox") ? 0.0f : 14.0f; // v12: the monster has no number under it
    auto area = juce::Rectangle<float>((float) sb.getX(), (float) sb.getY(), (float) sb.getWidth(), (float) sb.getHeight() - textH).reduced(3.0f);
    const float d = juce::jmin(area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float trackR = d * 0.5f - 2.0f;
    const float start = juce::MathConstants<float>::pi * 1.25f, end = juce::MathConstants<float>::pi * 2.75f;
    if (dragOver)
    {
        g.setColour((dragIsEnv ? LookAndFeel::env() : LookAndFeel::lfo()).withAlpha(0.35f));
        g.fillEllipse(c.x - trackR - 3.0f, c.y - trackR - 3.0f, (trackR + 3.0f) * 2.0f, (trackR + 3.0f) * 2.0f);
    }
    // the range the LFO (purple) and the ENV (green, v12) move the knob over
    const float base = (float) slider.valueToProportionOfLength(slider.getValue());
    auto ring = [&](float amount, float radius, juce::Colour col)
    {
        const float tgt = juce::jlimit(0.0f, 1.0f, base + amount);
        float a0 = start + base * (end - start), a1 = start + tgt * (end - start);
        if (a1 < a0) std::swap(a0, a1);
        juce::Path arc;
        arc.addCentredArc(c.x, c.y, radius, radius, 0.0f, a0, juce::jmax(a1, a0 + 0.02f), true);
        g.setColour(col);
        g.strokePath(arc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse(c.x + std::sin(start + tgt * (end - start)) * radius - 3.0f, c.y - std::cos(start + tgt * (end - start)) * radius - 3.0f, 6.0f, 6.0f);
    };
    if (proc->isModOn(modIdx)) ring(proc->getModAmount(modIdx), trackR + 3.5f, LookAndFeel::lfo());
    if (proc->isEnvModOn(modIdx)) ring(proc->getEnvModAmount(modIdx), trackR + (proc->isModOn(modIdx) ? 7.0f : 3.5f), LookAndFeel::env());
}

// ============================================================================ v4: LFO shape editor
LfoEditor::LfoEditor(SlicerProcessor& p) : proc(p) { startTimerHz(30); }

juce::Point<float> LfoEditor::toScreen(float x, float y) const
{
    const auto a = area();
    return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
}
juce::Point<float> LfoEditor::toShape(juce::Point<float> s) const
{
    const auto a = area();
    return { juce::jlimit(0.0f, 1.0f, (s.x - a.getX()) / a.getWidth()), juce::jlimit(0.0f, 1.0f, (a.getBottom() - s.y) / a.getHeight()) };
}
juce::Point<float> LfoEditor::curveHandle(const std::vector<bs::LfoPoint>& pts, int seg) const
{
    const auto& a = pts[(size_t) seg];
    const auto& b = pts[(size_t) seg + 1];
    return toScreen(0.5f * (a.x + b.x), a.y + (b.y - a.y) * bs::lfoCurve(0.5f, a.curve));
}
int LfoEditor::pointAt(juce::Point<float> s) const
{
    const auto pts = proc.getLfoPoints();
    for (int i = (int) pts.size() - 1; i >= 0; --i)
        if (toScreen(pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom(s) < 7.0f) return i;
    return -1;
}
int LfoEditor::curveAt(juce::Point<float> s) const
{
    const auto pts = proc.getLfoPoints();
    for (int i = 0; i + 1 < (int) pts.size(); ++i)
        if (pts[(size_t) i + 1].x - pts[(size_t) i].x > 0.01f && curveHandle(pts, i).getDistanceFrom(s) < 7.0f) return i;
    return -1;
}

void LfoEditor::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff0f1115));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(juce::Colour(0xff555c69));
    g.drawRoundedRectangle(b, 6.0f, 1.0f);
    const auto a = area();
    g.setColour(juce::Colour(0xff262b33));
    for (int k = 1; k < 8; ++k) g.drawVerticalLine((int) (a.getX() + a.getWidth() * (float) k / 8.0f), a.getY(), a.getBottom());
    for (int k = 1; k < 4; ++k) g.drawHorizontalLine((int) (a.getY() + a.getHeight() * (float) k / 4.0f), a.getX(), a.getRight());

    const auto pts = proc.getLfoPoints();
    const auto table = bs::buildLfoTable(pts);
    juce::Path curve, fill;
    for (int i = 0; i <= bs::kLfoTable; i += 4)
    {
        const auto p = toScreen((float) i / (float) bs::kLfoTable, table->v[i]);
        if (i == 0) { curve.startNewSubPath(p); fill.startNewSubPath(a.getX(), a.getBottom()); }
        else curve.lineTo(p);
        fill.lineTo(p);
    }
    fill.lineTo(a.getRight(), a.getBottom());
    fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(LookAndFeel::lfo().withAlpha(0.30f), 0.0f, a.getY(), LookAndFeel::lfo().withAlpha(0.03f), 0.0f, a.getBottom(), false));
    g.fillPath(fill);
    g.setColour(LookAndFeel::lfo());
    g.strokePath(curve, juce::PathStrokeType(2.0f));

    // curve handles + points
    for (int i = 0; i + 1 < (int) pts.size(); ++i)
    {
        if (pts[(size_t) i + 1].x - pts[(size_t) i].x <= 0.01f) continue;
        const auto h = curveHandle(pts, i);
        g.setColour(LookAndFeel::lfo().withAlpha(i == dragCurve ? 1.0f : 0.7f));
        g.drawEllipse(h.x - 3.5f, h.y - 3.5f, 7.0f, 7.0f, 1.2f);
    }
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const auto p = toScreen(pts[(size_t) i].x, pts[(size_t) i].y);
        g.setColour(i == dragPoint ? juce::Colours::white : LookAndFeel::lfo().brighter(0.4f));
        g.fillEllipse(p.x - 4.5f, p.y - 4.5f, 9.0f, 9.0f);
        g.setColour(juce::Colour(0xff0f1115));
        g.drawEllipse(p.x - 4.5f, p.y - 4.5f, 9.0f, 9.0f, 1.0f);
    }
    // where the LFO is right now
    const float ph = proc.getLfoPhase();
    const auto pp = toScreen(ph, table->v[juce::jlimit(0, bs::kLfoTable, (int) (ph * bs::kLfoTable))]);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawVerticalLine((int) pp.x, a.getY(), a.getBottom());
    g.setColour(juce::Colours::white);
    g.fillEllipse(pp.x - 3.0f, pp.y - 3.0f, 6.0f, 6.0f);
}

void LfoEditor::showMenu(int point)
{
    juce::PopupMenu m;
    m.addSectionHeader("LFO shape");
    for (int i = 0; i < bs::kLfoPresets; ++i) m.addItem(100 + i, bs::lfoPresetName(i));
    const auto pts = proc.getLfoPoints();
    if (point > 0 && point < (int) pts.size() - 1)
    {
        m.addSeparator();
        m.addItem(1, "Delete point");
    }
    m.addSeparator();
    m.addItem(2, "Flip up / down");
    juce::Component::SafePointer<LfoEditor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options(), [safe, point](int r)
    {
        if (safe == nullptr || r == 0) return;
        auto pts2 = safe->proc.getLfoPoints();
        if (r >= 100) pts2 = bs::lfoPreset(r - 100);
        else if (r == 1 && point > 0 && point < (int) pts2.size() - 1) pts2.erase(pts2.begin() + point);
        else if (r == 2) for (auto& q : pts2) { q.y = 1.0f - q.y; q.curve = -q.curve; }
        safe->proc.setLfoPoints(pts2);
    });
}

void LfoEditor::mouseDown(const juce::MouseEvent& e)
{
    dragPoint = pointAt(e.position);
    dragCurve = dragPoint < 0 ? curveAt(e.position) : -1;
    if (e.mods.isPopupMenu()) { const int p = dragPoint; dragPoint = dragCurve = -1; showMenu(p); return; }
    if (dragCurve >= 0) { curveFrom = proc.getLfoPoints()[(size_t) dragCurve].curve; dragStartY = e.position.y; }
}
void LfoEditor::mouseDrag(const juce::MouseEvent& e)
{
    auto pts = proc.getLfoPoints();
    if (dragPoint >= 0 && dragPoint < (int) pts.size())
    {
        auto s = toShape(e.position);
        if (e.mods.isShiftDown()) { s.x = std::round(s.x * 16.0f) / 16.0f; s.y = std::round(s.y * 8.0f) / 8.0f; } // snap to the grid
        const int last = (int) pts.size() - 1;
        float x = s.x;
        if (dragPoint == 0) x = 0.0f;
        else if (dragPoint == last) x = 1.0f;
        else x = juce::jlimit(pts[(size_t) dragPoint - 1].x, pts[(size_t) dragPoint + 1].x, x);
        pts[(size_t) dragPoint].x = x;
        pts[(size_t) dragPoint].y = s.y;
        // the first and last point stay at the same height, so the LFO loops without a jump
        if (dragPoint == 0 && last > 0 && std::fabs(pts[(size_t) last].y - pts[0].y) < 0.02f) pts[(size_t) last].y = s.y;
        proc.setLfoPoints(pts);
    }
    else if (dragCurve >= 0 && dragCurve + 1 < (int) pts.size())
    {
        // drag up = the middle of the segment goes up, whatever its direction
        const float dir = pts[(size_t) dragCurve + 1].y >= pts[(size_t) dragCurve].y ? 1.0f : -1.0f;
        pts[(size_t) dragCurve].curve = juce::jlimit(-1.0f, 1.0f, curveFrom + dir * (dragStartY - e.position.y) / 60.0f);
        proc.setLfoPoints(pts);
    }
}
void LfoEditor::mouseUp(const juce::MouseEvent&) { dragPoint = dragCurve = -1; }
void LfoEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    auto pts = proc.getLfoPoints();
    const int p = pointAt(e.position);
    const int c = p < 0 ? curveAt(e.position) : -1;
    if (p > 0 && p < (int) pts.size() - 1) pts.erase(pts.begin() + p);          // double-click a point = delete
    else if (c >= 0) pts[(size_t) c].curve = 0.0f;                               // double-click a curve handle = straight
    else if (p < 0)
    {
        const auto s = toShape(e.position);                                      // double-click empty space = new point
        pts.push_back({ s.x, s.y, 0.0f });
    }
    proc.setLfoPoints(pts);
}

void LfoHandle::paint(juce::Graphics& g)
{
    const auto col = env ? LookAndFeel::env() : LookAndFeel::lfo();
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(isMouseOver() ? col.withAlpha(0.35f) : col.withAlpha(0.18f));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(col);
    g.drawRoundedRectangle(r, 5.0f, 1.0f);
    g.setFont(ui(env ? 10.0f : 10.5f, true));
    g.drawText(juce::String::fromUTF8(env ? "\xe2\x9c\xa5  DRAG ENV ONTO A KNOB" : "\xe2\x9c\xa5  DRAG LFO ONTO A KNOB"), getLocalBounds(), juce::Justification::centred);
}
void LfoHandle::mouseDrag(const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return;
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor(this))
        if (!c->isDragAndDropActive())
        {
            juce::Image img(juce::Image::ARGB, 70, 22, true);
            juce::Graphics g(img);
            g.setColour(env ? LookAndFeel::env() : LookAndFeel::lfo());
            g.fillRoundedRectangle(0.0f, 0.0f, 70.0f, 22.0f, 5.0f);
            g.setColour(juce::Colour(0xff15121c));
            g.setFont(ui(11.0f, true));
            g.drawText(env ? "ENV" : "LFO", 0, 0, 70, 22, juce::Justification::centred);
            c->startDragging(env ? "env" : "lfo", this, juce::ScaledImage(img));
        }
}

void ModList::paint(juce::Graphics& g)
{
    if (rows.empty())
    {
        g.setColour(kDim.withAlpha(0.7f));
        g.setFont(ui(10.0f));
        g.drawFittedText("No targets yet.\nDrag the LFO or the ENV onto a knob.", getLocalBounds().reduced(4), juce::Justification::centred, 2);
    }
}
void ModList::refresh()
{
    shownVersion = proc.getModVersion();
    // v12: the LFO rows (purple) first, then the ENV rows (green, "E" in front)
    std::vector<std::pair<int, bool>> want;
    for (int d = 0; d < bs::kModCount; ++d) if (proc.isModOn(d)) want.push_back({ d, false });
    for (int d = 0; d < bs::kModCount; ++d) if (proc.isEnvModOn(d)) want.push_back({ d, true });
    bool same = want.size() == rows.size();
    for (size_t i = 0; same && i < want.size(); ++i) same = rows[i]->dest == want[i].first && rows[i]->env == want[i].second;
    if (!same)
    {
        rows.clear();
        for (auto [d, isEnv] : want)
        {
            auto r = std::make_unique<Row>();
            r->dest = d;
            r->env = isEnv;
            const auto col = isEnv ? LookAndFeel::env() : LookAndFeel::lfo();
            r->name = std::make_unique<juce::Label>();
            r->name->setFont(ui(10.0f, true));
            r->name->setColour(juce::Label::textColourId, col.brighter(0.3f));
            r->amt = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            r->amt->setRange(-100.0, 100.0, 1.0);
            r->amt->getProperties().set("accent", (int) col.getARGB());
            r->amt->setDoubleClickReturnValue(true, 50.0);
            r->del = std::make_unique<juce::TextButton>("x");
            r->del->getProperties().set("small", true);
            r->del->setTooltip(isEnv ? "UNDRAG ENV" : "UNDRAG LFO");
            auto* rp = r.get();
            r->amt->onValueChange = [this, rp]
            {
                if (rp->env) proc.setEnvModAmount(rp->dest, (float) rp->amt->getValue() / 100.0f);
                else proc.setModAmount(rp->dest, (float) rp->amt->getValue() / 100.0f);
            };
            r->del->onClick = [this, d = d, e = isEnv] { if (e) proc.removeEnvMod(d); else proc.removeMod(d); };
            for (juce::Component* c : { (juce::Component*) r->name.get(), (juce::Component*) r->amt.get(), (juce::Component*) r->del.get() }) addAndMakeVisible(c);
            rows.push_back(std::move(r));
        }
    }
    const int rowH = 20;
    setSize(getWidth(), juce::jmax(rowH * (int) rows.size(), getParentHeight()));
    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto& r = *rows[i];
        const int y = (int) i * rowH;
        const float a = r.env ? proc.getEnvModAmount(r.dest) : proc.getModAmount(r.dest);
        r.name->setText((r.env ? "E: " : "") + juce::String(bs::modInfo(r.dest).label) + "  " + juce::String::formatted("%+d%%", juce::roundToInt(a * 100.0f)), juce::dontSendNotification);
        if (!r.amt->isMouseButtonDown()) r.amt->setValue(a * 100.0, juce::dontSendNotification);
        r.name->setBounds(0, y, 86, rowH);
        r.amt->setBounds(86, y + 2, getWidth() - 86 - 22, rowH - 4);
        r.del->setBounds(getWidth() - 19, y + 2, 17, rowH - 4);
    }
    repaint();
}

// ============================================================================ v4: drag out
void ExportDrag::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(isMouseOver() ? juce::Colour(0xff303540) : juce::Colour(0xff252932));
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(LookAndFeel::orange());
    g.drawRoundedRectangle(r, 6.0f, 1.0f);
    g.setFont(ui(12.0f, true));
    g.drawText(status.isNotEmpty() ? status : juce::String::fromUTF8("\xe2\x87\xa9  DRAG TO DAW"), getLocalBounds(), juce::Justification::centred);
}
void ExportDrag::mouseDrag(const juce::MouseEvent& e)
{
    if (started || e.getDistanceFromDragStart() < 6) return;
    started = true;
    status = "rendering...";
    repaint();
    const auto f = proc.renderToFile(proc.getExportMono());
    status = {};
    repaint();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this);
}
void ExportDrag::mouseUp(const juce::MouseEvent& e)
{
    if (started || e.getDistanceFromDragStart() >= 6) return;
    const auto f = proc.renderToFile(proc.getExportMono());
    if (f.existsAsFile()) f.revealToUser();
}

// ============================================================================ content
KnobCell* Content::knob(const char* id, const char* label)
{
    auto* k = knobs.add(new KnobCell(proc.apvts, id, label));
    k->setModTarget(&proc, bs::modIndexOf(id)); // every knob in the LFO table accepts the LFO
    return k;
}
Panel* Content::panel(const juce::String& title, juce::Rectangle<int> r)
{
    auto* p = panels.add(new Panel(title));
    addAndMakeVisible(p);
    p->setBounds(r);
    return p;
}

Content::Content(SlicerProcessor& p) : proc(p), rhythmBar(p), keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&laf);
    setSize(kOldW, kBaseH); // v10: built at the v9 width, spread out to the wider window at the end
    const auto orange = LookAndFeel::orange(), cyan = LookAndFeel::cyan();

    // ---- sample panel
    auto* sp = panel("SAMPLE", { 10, 66, 1010, 312 });
    samplePanel = sp;
    wave = std::make_unique<WaveView>(proc);
    sp->addAndMakeVisible(*wave);
    wave->setBounds(12, 34, 986, 134); // v7: 20 px lower, the dots got taller
    // v6: zoom slider under the waveform - mark the area with the orange markers, then slide to close in on it
    zoomLabel.setText("ZOOM", juce::dontSendNotification);
    zoomLabel.setFont(ui(10.0f, true));
    zoomLabel.setColour(juce::Label::textColourId, cyan);
    sp->addAndMakeVisible(zoomLabel);
    zoomLabel.setBounds(12, 170, 44, 18);
    zoomSlider.setRange(0.0, 1.0, 0.0);
    zoomSlider.setDoubleClickReturnValue(true, 0.0);
    zoomSlider.getProperties().set("accent", (int) cyan.getARGB());
    zoomSlider.setTooltip("Zoom in on the part between the orange markers. Double-click = whole sample.");
    zoomSlider.onValueChange = [this] { wave->setZoom((float) zoomSlider.getValue()); };
    sp->addAndMakeVisible(zoomSlider);
    zoomSlider.setBounds(56, 170, 744, 18); // v9: shorter, MAIN MIX sits at its right end
    // v9: MAIN MIX - level of the main sample only (the ADD LAYER sound keeps its own Mix)
    mainMixLabel.setText("MAIN MIX", juce::dontSendNotification);
    mainMixLabel.setFont(ui(10.0f, true));
    mainMixLabel.setColour(juce::Label::textColourId, orange);
    mainMixLabel.setJustificationType(juce::Justification::centredRight);
    sp->addAndMakeVisible(mainMixLabel);
    mainMixLabel.setBounds(806, 170, 62, 18);
    mainMix.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    mainMix.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd5dbe5));
    mainMix.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    mainMix.getProperties().set("accent", (int) orange.getARGB());
    mainMix.setTooltip("MAIN MIX: level of the main sample only. The layer sound keeps its own Mix. Double-click = 100 %.");
    sp->addAndMakeVisible(mainMix);
    mainMix.setBounds(870, 170, 128, 18);
    mainMixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "mainmix", mainMix);
    mainMix.setDoubleClickReturnValue(true, 1.0);
    beat = std::make_unique<BeatBar>(proc);
    sp->addAndMakeVisible(*beat);
    beat->setBounds(70, 192, 928, 74); // v7: room for three letters under every dot
    beat->setTooltip("Dot: click = mute, drag = level in dB (Shift = fine), right-click = pitch / reverse / stutter / long / wah / balagan / panning / copy.\n"
                     "Choose dots: drag on an empty spot, or Ctrl+click a dot. Chosen dots: Attack / Decay / Sustain / Release / Length work on them only.\n"
                     "Letters under a dot: R = reverse, W = wah, C = copied chop, E = own envelope. Red dot = balagan. Black line on a dot = its panning.\n"
                     "Ctrl+drag on the waveform = slip.");
    // v6: LOOP button with a lamp, at the left edge of the dots
    loopBtn = std::make_unique<LoopButton>(proc);
    sp->addAndMakeVisible(*loopBtn);
    loopBtn->setBounds(12, 192, 52, 74);

    sampleInfo.setJustificationType(juce::Justification::centredLeft);
    sampleInfo.setColour(juce::Label::textColourId, kDim);
    sampleInfo.setMinimumHorizontalScale(1.0f); // v10: a long name is shortened with "...", never squeezed
    sp->addAndMakeVisible(sampleInfo);
    sampleInfo.setBounds(80, 4, proc.isStandalone() ? 370 : 500, 18);
    sampleInfo.setText("No sample loaded", juce::dontSendNotification);
    if (proc.isStandalone())
    {
        // v5: Standalone only - the BPM of the internal clock (drag up / down, click to type, or the + / - buttons).
        // In Cubase this box does not exist and the plugin follows the project tempo.
        bpmLabel.setText("BPM", juce::dontSendNotification);
        bpmLabel.setFont(ui(10.0f, true));
        bpmLabel.setColour(juce::Label::textColourId, orange);
        bpmLabel.setJustificationType(juce::Justification::centredRight);
        sp->addAndMakeVisible(bpmLabel);
        bpmLabel.setBounds(454, 3, 36, 22);
        bpmBox.setIncDecButtonsMode(juce::Slider::incDecButtonsDraggable_Vertical);
        bpmBox.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 64, 22);
        bpmBox.setColour(juce::Slider::textBoxBackgroundColourId, kField);
        bpmBox.setColour(juce::Slider::textBoxOutlineColourId, kFieldEdge);
        bpmBox.setColour(juce::Slider::textBoxTextColourId, kText);
        sp->addAndMakeVisible(bpmBox);
        bpmBox.setBounds(494, 3, 118, 22);
        bpmAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "tempo", bpmBox);
        bpmBox.setTextBoxIsEditable(true);
    }

    auto addRow = [&](Panel* pn, const char* id, juce::StringArray labels, int x, int y, bool isBool = false, std::vector<int> values = {})
    {
        auto* r = rows.add(new ChoiceRow(proc.apvts, id, labels, isBool, std::move(values)));
        pn->addAndMakeVisible(*r);
        r->setBounds(x, y, r->preferredWidth(), 22);
        return r;
    };
    auto label = [&](Panel* pn, const juce::String& t, int x, int y)
    {
        auto* l = new juce::Label();
        l->setText(t, juce::dontSendNotification);
        l->setFont(ui(10.0f, true));
        l->setBounds(x, y, 44, 22);
        pn->addAndMakeVisible(l);
        labelsOwned.add(l);
        return l;
    };
    {
        // header of the sample panel, from the right: LOAD, CLEAR (v6), bars, ONE SHOT
        auto* load = buttons.add(new juce::TextButton("LOAD"));
        load->getProperties().set("small", true);
        sp->addAndMakeVisible(*load);
        load->setBounds(938, 3, 60, 22);
        load->onClick = [this] { chooseFile(); };
        auto* clear = buttons.add(new juce::TextButton("CLEAR"));
        clear->getProperties().set("small", true);
        makeSolid(*clear); // v13: orange, like a lit PLAY
        clear->setTooltip("Empty the waveform");
        sp->addAndMakeVisible(*clear);
        clear->setBounds(872, 3, 60, 22);
        clear->onClick = [this] { zoomSlider.setValue(0.0); proc.clearSample(); };
        // v10: RESET SAMPLE - the sample stays, everything done to it goes (knobs, LFO and effects stay)
        auto* reset = buttons.add(new juce::TextButton("RESET SAMPLE"));
        reset->getProperties().set("small", true);
        reset->setTooltip("Back to the sample as it was loaded: markers, dots, slip, cuts and slice envelopes are reset. "
                          "Knobs, LFO and effects stay as they are.");
        sp->addAndMakeVisible(*reset);
        reset->onClick = [this] { zoomSlider.setValue(0.0); proc.resetSample(); };
        auto* bars = addRow(sp, "bars", { "1 bar", "2", "4", "8" }, 0, 3);
        bars->setTopLeftPosition(clear->getX() - 10 - bars->getWidth(), 3);
        auto* os = addRow(sp, "oneshot", { "ONE SHOT" }, 0, 3, true);
        os->setTopLeftPosition(bars->getX() - 10 - os->getWidth(), 3);
        // v10: LOCK ONE SHOT - holds the markers of the one shot, so the dots mode chops only that part
        auto* lock = addRow(sp, "oslock", { "LOCK ONE SHOT" }, 0, 3, true);
        lock->setTooltip("Takes a picture of the One Shot as it is now. Back in the dots, every dot plays this one shot (on any grid),\n"
                         "and each dot can still be changed. Off = back to the normal dots and preset; on again = the locked one shot\n"
                         "comes back with all its work.");
        loadBtn = load; clearBtn = clear; resetBtn = reset; barsRow = bars; oneShotRow = os; lockRow = lock;
    }
    auto* gridLabel = label(sp, "GRID", 12, 276);
    // v10: 1/2 first on screen (saved last, so old projects keep their grid)
    auto* gridRow = addRow(sp, "grid", { "1/2", "1/4", "1/8", "1/16", "1/32", "Trip" }, 56, 276, false, { 5, 0, 1, 2, 3, 4 });
    // v13: the PICK and NOTES titles are gone (their buttons speak for themselves) - more room in the row
    auto* pickRow = addRow(sp, "pick", { "AUTO", "MANUAL", "RANDOM" }, 372, 276);
    auto* rr = buttons.add(new juce::TextButton("Reroll"));
    {
        rr->getProperties().set("small", true);
        sp->addAndMakeVisible(*rr);
        rr->setBounds(590, 276, 66, 22);
        rr->onClick = [this] { proc.reroll(); };
    }
    auto* modeRow = addRow(sp, "mode", { "PLAY", "AUTO" }, 740, 276);
    auto* keyRow = addRow(sp, "keytrack", { "KEY PITCH" }, 872, 276, true);
    // v12: PICK ALL - chooses every dot of the bar in one click (handy with many dots)
    pickAllBtn = buttons.add(new juce::TextButton("PICK ALL"));
    pickAllBtn->getProperties().set("small", true);
    pickAllBtn->getProperties().set("fontSize", 10.0f); // v13: sits under LOOP, in the place of the word GRID
    makeSolid(*pickAllBtn);
    pickAllBtn->setTooltip("Choose every dot of the bar. Then every change goes to all of them. Click an empty spot on the dots to let go.");
    sp->addAndMakeVisible(*pickAllBtn);
    pickAllBtn->setBounds(0, 276, ui(11.0f, true).getStringWidth("PICK ALL") + 22, 22);
    pickAllBtn->onClick = [this]
    {
        const int spb = proc.stepsPerBarNow();
        proc.setSelectedDots(spb >= 32 ? 0xffffffffu : ((1u << spb) - 1u));
    };
    sp->setAccent(orange);

    // ---- knob rows
    auto* shape = panel("SLICE SHAPE", { 10, 388, 166, 150 });
    lengthKnob = knob("length", "Length"); // v11: also follows the chosen dots
    auto* noiseKnob = knob("noise", "Noise");
    for (auto* c : { lengthKnob, noiseKnob }) { shape->addAndMakeVisible(c); shape->add(c, 74, 90); }
    // v7: noise type, right under the Noise knob
    noiseTypeBtn = std::make_unique<ChoiceMenuButton>(proc.apvts, "noisetype");
    noiseTypeBtn->setTooltip("Noise type");
    shape->addAndMakeVisible(*noiseTypeBtn);
    noiseTypeBtn->setBounds(84, 124, 72, 20);
    shape->setAccent(cyan);

    auto* flt = panel("FILTER", { 186, 388, 214, 150 });
    auto* cutoffKnob = knob("cutoff", "Cutoff");
    auto* resKnob = knob("resonance", "Resonance");
    for (auto* c : { cutoffKnob, resKnob }) { flt->addAndMakeVisible(c); flt->add(c, 80, 100); }
    {
        auto* ft = addRow(flt, "ftype", { "LP", "HP", "BP" }, 0, 3);
        ft->setTopLeftPosition(214 - 9 - ft->getWidth(), 3);
        // switching to HP / BP with the cutoff fully open would be silent, so start from a useful spot
        ft->onPick = [this](int i)
        {
            auto* c = proc.apvts.getParameter("cutoff");
            if (c == nullptr || i == 0 || proc.apvts.getRawParameterValue("cutoff")->load() < 19000.0f) return;
            c->beginChangeGesture();
            c->setValueNotifyingHost(c->convertTo0to1(i == 1 ? 200.0f : 1200.0f));
            c->endChangeGesture();
        };
    }
    flt->setAccent(cyan);

    auto* env = panel("SLICE ENVELOPE", { 410, 388, 300, 150 });
    envKnobs[0] = knob("attack", "Attack"); envKnobs[1] = knob("decay", "Decay"); envKnobs[2] = knob("sustain", "Sustain"); envKnobs[3] = knob("release", "Release");
    for (auto* c : envKnobs) { env->addAndMakeVisible(c); env->add(c, 70, 92); } // v12: a little shorter - DRAG ENV sits under them
    env->setAccent(cyan);
    envLamps = std::make_unique<EnvModeLamps>(proc); // v10
    env->addAndMakeVisible(*envLamps);
    envLamps->setBounds(300 - 9 - 128, 4, 128, 19);
    // v12: drag the envelope onto Cutoff, Pitch, Noise, Balagan or Bal. Filter, like the LFO
    env->addAndMakeVisible(envHandle);
    envHandle.setTooltip("Drag onto Cutoff, Pitch, Noise, Balagan or the Balagan Filter. The envelope of every chop moves that knob "
                         "(it starts again with every chop). Right-click the knob = UNDRAG ENV.");
    envHandle.setBounds(50, 127, 200, 18);

    auto* mix = panel("PER-SLICE MIX", { 720, 388, 300, 150 });
    auto* pitchKnob = knob("pitch", "Pitch");
    auto* volumeKnob = knob("volume", "Volume");
    for (auto* c : { pitchKnob, volumeKnob, knob("velocity", "Vel Depth"), knob("random", "Random") }) { mix->addAndMakeVisible(c); mix->add(c, 70, 100); }
    mix->setAccent(orange);
    // v12: with dots chosen, these knobs work on the chosen dots only (same order as bs::kDot*)
    {
        KnobCell* dk[bs::kDotKnobCount] = { envKnobs[0], envKnobs[1], envKnobs[2], envKnobs[3], lengthKnob, cutoffKnob, resKnob, noiseKnob, pitchKnob, volumeKnob };
        for (int k = 0; k < bs::kDotKnobCount; ++k) dotKnobs[k] = dk[k];
        Panel* dp[4] = { shape, flt, env, mix };
        for (int k = 0; k < 4; ++k) { dotPanels[k] = dp[k]; dotPanelTitles[k] = dp[k]->title; }
    }

    // ---- v4: LFO / unison / chorus row
    auto* lp = panel("LFO", { 10, 548, 518, 160 }); // v6: a bit narrower, to make room for WAH
    ChoiceRow* lfoModeRow = nullptr;
    ChoiceRow* lfoBpmRow = nullptr;
    {
        auto* mode = addRow(lp, "lfomode", { "TRIG", "ENV", "FREE" }, 0, 3);
        auto* bpmRow = addRow(lp, "lfosync", { "BPM" }, 0, 3, true);
        lfoModeRow = mode; lfoBpmRow = bpmRow;
        bpmRow->setTopLeftPosition(518 - 9 - bpmRow->getWidth(), 3);
        mode->setTopLeftPosition(bpmRow->getX() - 8 - mode->getWidth(), 3);
        lfoEditor = std::make_unique<LfoEditor>(proc);
        lp->addAndMakeVisible(*lfoEditor);
        lfoEditor->setBounds(9, 34, 240, 118);
        lfoRateKnob = knob("lforate", "Rate");
        lfoHzKnob = knob("lfohz", "Rate Hz");
        for (auto* k : { lfoRateKnob, lfoHzKnob }) { lp->addAndMakeVisible(k); k->setBounds(254, 40, 74, 100); }
        lp->addAndMakeVisible(lfoHandle);
        lfoHandle.setBounds(334, 34, 175, 24);
        modList = std::make_unique<ModList>(proc);
        modView.setViewedComponent(modList.get(), false);
        modView.setScrollBarsShown(true, false);
        modView.setScrollBarThickness(6);
        lp->addAndMakeVisible(modView);
        modView.setBounds(334, 62, 175, 90);
        modList->setSize(168, 90);
        modList->refresh();
    }
    lp->setAccent(LookAndFeel::lfo());

    auto* uni = panel("UNISON", { 538, 548, 168, 160 });
    for (auto* c : { knob("univoices", "Voices"), knob("unidetune", "Detune"), knob("unispread", "Spread") }) { uni->addAndMakeVisible(c); uni->add(c, 50, 100); }
    uni->setAccent(cyan);

    auto* cho = panel("CHORUS", { 716, 548, 168, 160 });
    for (auto* c : { knob("chorate", "Rate"), knob("chodepth", "Depth"), knob("chomix", "Mix") }) { cho->addAndMakeVisible(c); cho->add(c, 50, 100); }
    cho->setAccent(cyan);

    // v6: WAH - amount (0 = off) and speed. Only the dots marked W (right-click a dot) get it.
    auto* wah = panel("WAH", { 894, 548, 126, 160 });
    for (auto* c : { knob("wah", "Amount"), knob("wahspeed", "Speed") }) { wah->addAndMakeVisible(c); wah->add(c, 54, 100); }
    wah->setAccent(cyan);

    auto* timing = panel("TIMING", { 10, 718, 400, 128 });
    auto* tempoKnob = knob("tempo", proc.isStandalone() ? "Tempo" : "Tempo*");
    for (auto* c : { knob("swing", "Swing"), knob("reverse", "Reverse"), tempoKnob, knob("master", "Master") }) { timing->addAndMakeVisible(c); timing->add(c, 90, 90); }
    timing->setAccent(orange);
    // v5: in a host (Cubase) the project tempo rules, so the Tempo knob is shown dimmed there
    if (!proc.isStandalone()) tempoKnob->setAlpha(0.45f);

    // v7: the preset buttons moved into the PRESETS list at the top. Their place is now ADD LAYER (left half);
    // the right half is kept free for the next idea.
    auto* lay = panel("ADD LAYER", { 420, 718, 300, 128 });
    layerPanel = lay;
    layerView = std::make_unique<LayerView>(proc);
    lay->addAndMakeVisible(*layerView);
    layerView->setBounds(9, 34, 200, 86);
    KnobCell* layerMixKnob = nullptr;
    {
        auto* mixKnob = knob("layermix", "Mix");
        lay->addAndMakeVisible(mixKnob);
        mixKnob->setBounds(216, 30, 76, 94);
        layerMixKnob = mixKnob;
    }
    // v13: ENVELOPE opens the layer's own envelope; UNLINK (green) takes the layer out of the effects, its arrow links parts back
    layerEnvBtn.getProperties().set("small", true);
    layerEnvBtn.setTooltip("Opens a simple envelope for the layer only (Attack, Decay, Sustain, Release). The sound keeps playing while you set it.");
    lay->addAndMakeVisible(layerEnvBtn);
    unlinkBtn = std::make_unique<UnlinkButton>(proc);
    lay->addAndMakeVisible(*unlinkBtn);
    lay->setAccent(orange);
    {
        layerEnvBox = std::make_unique<FloatBox>("LAYER ENVELOPE", orange);
        static const char* const ids[] = { "layatk", "laydec", "laysus", "layrel" };
        static const char* const names[] = { "Attack", "Decay", "Sustain", "Release" };
        for (int i = 0; i < 4; ++i)
        {
            auto* k = knobs.add(new KnobCell(proc.apvts, ids[i], names[i]));
            k->slider.getProperties().set("accent", (int) orange.getARGB());
            layerEnvBox->addAndMakeVisible(k);
            k->setBounds(10 + 70 * i, 30, 70, 96);
        }
        layerEnvBox->setSize(10 + 4 * 70 + 10, 134);
        addChildComponent(*layerEnvBox);
        linkList = std::make_unique<LinkList>(proc);
        linkBox = std::make_unique<FloatBox>("UNLINK  -  LINK BACK", LookAndFeel::lampGreen());
        linkBox->addAndMakeVisible(*linkList);
        linkList->setBounds(8, 28, 176, LinkList::kTop + bs::kLinkCount * LinkList::kRowH + 2);
        linkBox->setSize(192, linkList->getBottom() + 8);
        addChildComponent(*linkBox);
        layerEnvBtn.onClick = [this] { toggleBox(layerEnvBox.get()); };
        unlinkBtn->onArrow = [this] { toggleBox(linkBox.get()); };
    }

    // v8: BALAGAN - in the free right half of the bottom row. The knob sets how fast the red dots repeat,
    // SUPER = every repeat starts from a random place in the chop, Filter = the filter moves along the roll.
    auto* bal = panel("BALAGAN", { 730, 718, 290, 128 });
    KnobCell* balKnob = nullptr;
    KnobCell* balFilterKnob = nullptr;
    {
        auto* bk = knob("balagan", "Balagan");
        bk->slider.getProperties().set("monster", true);
        bk->getProperties().set("noTextBox", true); // v12: the value has its own box at the side
        bal->addAndMakeVisible(bk);
        bk->setBounds(8, 28, 96, 84);
        auto* fk = knob("balfilter", "Filter");
        bal->addAndMakeVisible(fk);
        fk->setBounds(108, 34, 76, 90);
        superBtn = std::make_unique<LampButton>(proc.apvts, "balsuper", "SUPER", LookAndFeel::balagan());
        superBtn->setTooltip("SUPER off: every repeat starts from the start of the chop (like a trigger).\n"
                             "SUPER on: every repeat starts from a random place inside the chop.");
        bal->addAndMakeVisible(*superBtn);
        superBtn->setBounds(192, 36, 88, 82);
        // v12: the value next to the monster (double-click = type it) and the FREE / BPM lamps under it
        balValue = std::make_unique<BalaganValue>(proc.apvts);
        bal->addAndMakeVisible(*balValue);
        balLamps = std::make_unique<BalModeLamps>(proc.apvts);
        bal->addAndMakeVisible(*balLamps);
        balKnob = bk; balFilterKnob = fk;
    }
    bal->setAccent(LookAndFeel::balagan());

    addAndMakeVisible(rhythmBar);
    rhythmBar.setBounds(350, 11, 386, 34);

    keyboard.setKeyWidth(20.6f);
    keyboard.setAvailableRange(12, 95);
    keyboard.setOctaveForMiddleC(3);
    keyboard.setScrollButtonsVisible(false);
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, orange.withAlpha(0.8f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, orange.withAlpha(0.3f));
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffe9edf3));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff15171b));
    addAndMakeVisible(keyboard);
    keyboard.setBounds(10, 858, 1010, 56);

    // ---- v4: drag the result out (header)
    exportDrag = std::make_unique<ExportDrag>(proc);
    addAndMakeVisible(*exportDrag);
    exportDrag->setBounds(746, 11, 128, 34);
    for (auto* b : { &stereoBtn, &monoBtn })
    {
        b->getProperties().set("small", true);
        addAndMakeVisible(*b);
    }
    stereoBtn.setBounds(882, 17, 66, 22);
    monoBtn.setBounds(951, 17, 58, 22);
    stereoBtn.onClick = [this] { proc.setExportMono(false); };
    monoBtn.onClick = [this] { proc.setExportMono(true); };

    // ---- v10: the wider window. Everything keeps its size and order, the space between grows.
    spreadLayout();
    {
        const int W = sp->getWidth();
        wave->setBounds(12, 34, W - 24, 134);
        beat->setBounds(70, 192, W - 12 - 70, 74);
        mainMix.setBounds(W - 12 - 128, 170, 128, 18);
        mainMixLabel.setBounds(mainMix.getX() - 64, 170, 62, 18);
        zoomSlider.setBounds(56, 170, mainMixLabel.getX() - 6 - 56, 18);
        // bottom row of the sample panel.
        // v13: PICK ALL sits under LOOP (where the word GRID was), orange. The grid starts exactly at the left edge of the
        // dots window and the word GRID moves after Trip. Then the PICK buttons + Reroll and PLAY / AUTO + KEY PITCH,
        // with equal space between the groups.
        pickAllBtn->setBounds(loopBtn->getX(), 276, loopBtn->getWidth(), 22);
        gridLabel->setSize(40, 22);
        struct Item { juce::Component* c; int gapBefore; };
        const std::vector<std::vector<Item>> groups {
            { { gridRow, 0 }, { gridLabel, 8 } },
            { { pickRow, 0 }, { rr, 10 } },
            { { modeRow, 0 }, { keyRow, 14 } } };
        int total = 0;
        std::vector<int> gw;
        for (auto& gr : groups) { int w = 0; for (auto& it : gr) w += it.gapBefore + it.c->getWidth(); gw.push_back(w); total += w; }
        const int x0 = beat->getX();
        const float gap = (float) ((W - 12 - x0) - total) / (float) (groups.size() - 1);
        float x = (float) x0;
        for (size_t gi = 0; gi < groups.size(); ++gi)
        {
            int cx = juce::roundToInt(x);
            for (auto& it : groups[gi]) { cx += it.gapBefore; it.c->setTopLeftPosition(cx, 276); cx += it.c->getWidth(); }
            x += (float) gw[gi] + gap;
        }
        layoutSampleHeader();
    }
    {
        // v13: TIMING a little narrower (its knobs come closer, same spacing between them), ADD LAYER wider by the same:
        // a bigger window to see the layer, with ENVELOPE and UNLINK in its title row
        const int take = 44;
        timing->setBounds(timing->getX(), timing->getY(), timing->getWidth() - take, timing->getHeight());
        lay->setBounds(lay->getX() - take, lay->getY(), lay->getWidth() + take, lay->getHeight());
        const int W = lay->getWidth();
        layerMixKnob->setBounds(W - 9 - 76, 30, 76, 94);
        layerView->setBounds(9, 34, layerMixKnob->getX() - 8 - 9, 86);
        unlinkBtn->setBounds(W - 9 - 96, 4, 96, 19);
        const int ew = ui(11.0f, true).getStringWidth("ENVELOPE") + 22;
        layerEnvBtn.setBounds(unlinkBtn->getX() - 8 - ew, 3, ew, 21);
    }
    if (lfoModeRow != nullptr && lfoBpmRow != nullptr) lfoModeRow->setTopLeftPosition(lfoBpmRow->getX() - 8 - lfoModeRow->getWidth(), 3);
    envLamps->setBounds(env->getWidth() - 9 - 128, 4, 128, 19);
    envHandle.setBounds((env->getWidth() - 210) / 2, 127, 210, 18); // v12
    {
        // v12: BALAGAN - monster, its value + FREE / BPM lamps, Filter, SUPER
        const int W = bal->getWidth();
        balKnob->setBounds(6, 30, 96, 84);
        balValue->setBounds(106, 36, 58, 46);
        balLamps->setBounds(106, 86, 58, 36);
        superBtn->setBounds(W - 9 - 88, 36, 88, 82);
        balFilterKnob->setBounds(164 + (superBtn->getX() - 164 - 76) / 2, 34, 76, 90);
    }
    startTimerHz(10);
    timerCallback();
}

// v10: moves the v9 layout (built 1030 wide) out to the wider window. Panels grow in width; inside them
// the knobs keep their size and spread over equal slots, wide parts (waveform, dots, LFO shape) grow,
// parts at the edges stay at the edges, everything else keeps its place relative to the panel.
void Content::spreadLayout()
{
    const float f = (float) (kBaseW - 20) / (float) (kOldW - 20);
    for (auto* p : panels)
    {
        const auto ob = p->getBounds();
        const int oldW = ob.getWidth();
        std::vector<std::pair<juce::Component*, juce::Rectangle<int>>> kids;
        for (auto* ch : p->getChildren())
            if (!p->manages(ch)) kids.push_back({ ch, ch->getBounds() });
        const int nx = 10 + juce::roundToInt((float) (ob.getX() - 10) * f), nr = 10 + juce::roundToInt((float) (ob.getRight() - 10) * f);
        p->setBounds(nx, ob.getY(), nr - nx, ob.getHeight());
        const int newW = nr - nx;
        const float pf = (float) newW / (float) oldW;
        for (auto& [ch, r] : kids)
        {
            auto n = r;
            const int rightM = oldW - r.getRight();
            if ((float) r.getWidth() >= (float) oldW * 0.4f) n.setWidth(juce::roundToInt((float) r.getWidth() * pf));
            else if (rightM <= 14) n.setX(newW - rightM - r.getWidth());
            else if (r.getX() > 14) n.setX(juce::roundToInt((float) r.getCentreX() * pf - (float) r.getWidth() * 0.5f));
            ch->setBounds(n);
        }
    }
    setSize(kBaseW, kBaseH);
    // header: the right-hand group stays at the right edge, the preset bar sits in the middle of the free space
    const int dx = kBaseW - kOldW;
    for (juce::Component* c : { (juce::Component*) exportDrag.get(), (juce::Component*) &stereoBtn, (juce::Component*) &monoBtn })
        c->setTopLeftPosition(c->getX() + dx, c->getY());
    rhythmBar.setTopLeftPosition(350 + (exportDrag->getX() - 350 - rhythmBar.getWidth()) / 2, rhythmBar.getY());
    keyboard.setBounds(10, keyboard.getY(), kBaseW - 20, keyboard.getHeight());
    keyboard.setKeyWidth(20.6f * f);
}

// v10: top row of the SAMPLE panel, from the right: LOAD, CLEAR, RESET SAMPLE, bars, ONE SHOT, LOCK ONE SHOT, (BPM), name
void Content::layoutSampleHeader()
{
    if (samplePanel == nullptr || loadBtn == nullptr) return;
    const int W = samplePanel->getWidth();
    auto textW = [](juce::TextButton* b) { return ui(11.0f, true).getStringWidth(b->getButtonText()) + 22; };
    loadBtn->setBounds(W - 12 - 60, 3, 60, 22);
    clearBtn->setBounds(loadBtn->getX() - 6 - 60, 3, 60, 22);
    resetBtn->setBounds(clearBtn->getX() - 6 - textW(resetBtn), 3, textW(resetBtn), 22);
    barsRow->setTopLeftPosition(resetBtn->getX() - 10 - barsRow->getWidth(), 3);
    oneShotRow->setTopLeftPosition(barsRow->getX() - 10 - oneShotRow->getWidth(), 3);
    lockRow->setTopLeftPosition(oneShotRow->getX() - 6 - lockRow->getWidth(), 3);
    int left = lockRow->getX();
    if (proc.isStandalone())
    {
        bpmBox.setBounds(left - 10 - 118, 3, 118, 22);
        bpmLabel.setBounds(bpmBox.getX() - 40, 3, 36, 22);
        left = bpmLabel.getX();
    }
    sampleInfo.setBounds(80, 4, juce::jmax(60, left - 10 - 80), 18);
}

// v10: SLICE lamp + a chosen slice -> the four envelope knobs show and change only that slice.
// v11: chosen dots come first -> Attack, Decay, Sustain, Release and Length show the highest value of the chosen dots,
// and turning a knob moves every chosen dot by the same amount, each from its own value (the differences stay).
// v12: the panels whose knobs follow the chosen dots say so in their title (suffix empty = back to the plain title)
void Content::setDotTitles(const juce::String& suffix)
{
    for (int k = 0; k < 4; ++k)
        if (dotPanels[k] != nullptr)
        {
            // SLICE SHAPE is narrow: the short form. FILTER has its LP / HP / BP buttons in the title row: no room, it keeps its title.
            const bool add = suffix.isNotEmpty() && k != 1;
            const juce::String t = dotPanelTitles[k] + (add ? "  -  " + (k == 0 ? suffix.upToFirstOccurrenceOf(" (", false, false) : suffix) : juce::String());
            if (dotPanels[k]->title != t) { dotPanels[k]->title = t; dotPanels[k]->repaint(); }
        }
}

// v13: the small boxes over the ADD LAYER panel - one open at a time, the same button closes it again
void Content::toggleBox(FloatBox* box)
{
    if (box == nullptr || layerPanel == nullptr) return;
    const bool show = !box->isVisible();
    for (auto* b : { layerEnvBox.get(), linkBox.get() }) if (b != nullptr) b->setVisible(false);
    if (!show) return;
    const auto lp = layerPanel->getBounds();
    const int x = box == linkBox.get() ? lp.getRight() - 8 - box->getWidth() : lp.getX() + 8;
    box->setTopLeftPosition(juce::jlimit(4, getWidth() - box->getWidth() - 4, x), lp.getY() - 4 - box->getHeight());
    box->setVisible(true);
    box->toFront(false);
}

void Content::updateEnvKnobs()
{
    auto* envPanel = dynamic_cast<Panel*>(envKnobs[0]->getParentComponent());
    const int spb = bs::stepsPerBarFor((int) proc.apvts.getRawParameterValue("grid")->load());
    uint32_t dots = proc.isOneShot() ? 0u : proc.getSelectedDots();
    if (spb < 32) dots &= (1u << spb) - 1u;
    if (dots != 0)
    {
        if (proc.getSelectedSeg() >= 0) proc.setSelectedSeg(-1);
        // refresh when the choice changes, or when the values changed from somewhere else (not by these knobs)
        juce::String key = "d" + juce::String((juce::int64) dots) + "/" + juce::String(spb) + (proc.isOneShotLocked() ? "L" : "");
        bool moved = false;
        for (int k = 0; k < bs::kDotKnobCount; ++k)
            if (std::fabs(proc.getDotsShownValue(dots, k) - dotShown[k]) > 1.0e-4f * juce::jmax(1.0f, std::fabs(dotShown[k]))) moved = true;
        if (key == envShownKey && !moved) return;
        envShownKey = key;
        envShownSeg = -2;
        // v11: Attack, Decay, Sustain, Release, Length. v12: + Cutoff, Resonance, Noise, Pitch, Volume - every change goes
        // to every chosen dot (each moves by the same turn from its own value), the dots that are not chosen stay as they are
        for (int k = 0; k < bs::kDotKnobCount; ++k)
        {
            dotShown[k] = proc.getDotsShownValue(dots, k);
            dotKnobs[k]->setOverride(dotShown[k], [this, k, dots](float v)
            {
                proc.moveDotsEnv(dots, k, dotShown[k], v);
                dotShown[k] = proc.getDotsShownValue(dots, k);
            });
        }
        int count = 0;
        juce::String list;
        for (int i = 0; i < spb && i < 32; ++i)
            if (dots & (1u << i)) { ++count; if (count <= 4) list << (count > 1 ? "," : "") << (i + 1); }
        setDotTitles(count == 1 ? "DOT " + list : juce::String(count) + " DOTS" + (count <= 4 ? " (" + list + ")" : juce::String()));
        juce::ignoreUnused(envPanel);
        return;
    }
    if (envShownKey.isNotEmpty())
    {
        // the dots were let go: the knobs belong to their parameters again
        envShownKey = {};
        envShownSeg = -2;
        for (auto* k : dotKnobs) k->clearOverride();
        setDotTitles({});
    }
    int seg = -1;
    if (proc.isEnvSlice() && !proc.isOneShot() && !proc.isOneShotLocked())
    {
        seg = proc.getSelectedSeg();
        auto plan = proc.getPlan();
        if (seg >= 0 && (!plan || seg >= (int) plan->segments.size())) { proc.setSelectedSeg(-1); seg = -1; }
    }
    else if (proc.getSelectedSeg() >= 0) proc.setSelectedSeg(-1);
    if (seg == envShownSeg) return;
    envShownSeg = seg;
    if (seg < 0)
    {
        for (auto* k : envKnobs) k->clearOverride();
        if (envPanel != nullptr) { envPanel->title = "SLICE ENVELOPE"; envPanel->repaint(); }
        return;
    }
    const auto e = proc.getSegEnv(seg);
    const float vals[4] = { e.attackMs, e.decayMs, e.sustain, e.releaseMs };
    for (int k = 0; k < 4; ++k)
        envKnobs[k]->setOverride(vals[k], [this, seg, k](float v) { proc.setSegEnvValue(seg, k, v); });
    if (envPanel != nullptr) { envPanel->title = "SLICE ENVELOPE  -  SLICE " + juce::String(seg + 1); envPanel->repaint(); }
}

Content::~Content() { stopTimer(); setLookAndFeel(nullptr); }

void Content::timerCallback()
{
    juce::String t = "No sample loaded";
    const juce::String name = proc.getSampleName();
    if (name.isNotEmpty())
    {
        const juce::String rest = "  -  " + juce::String(proc.getSampleSeconds(), 1) + " s  -  "
                                  + (proc.isOneShot() ? juce::String("one shot") : "suggested: " + juce::String(proc.getSuggestedBars()) + " bars");
        // v10: a long name is shortened ("Vocal Chop Fe...") so it never pushes into the buttons; the full name is in the tooltip
        const auto font = laf.getLabelFont(sampleInfo);
        const int avail = sampleInfo.getWidth() - 4;
        juce::String shownName = name;
        if (font.getStringWidth(name + rest) > avail)
        {
            int n = name.length();
            while (n > 4 && font.getStringWidth(name.substring(0, n).trimEnd() + "..." + rest) > avail) --n;
            shownName = name.substring(0, n).trimEnd() + "...";
        }
        t = shownName + rest;
    }
    if (t != sampleInfo.getText()) sampleInfo.setText(t, juce::dontSendNotification);
    if (name != shownInfo) { shownInfo = name; sampleInfo.setTooltip(name); }
    // v10: LOCK ONE SHOT shows in One Shot, and stays visible while it is locked (so it can be switched off)
    // v11: ... and in the dots while a locked one shot is parked, so it can be switched back on
    const bool showLock = proc.isOneShot() || proc.isOneShotLocked() || proc.hasLockSnapshot();
    if (lockRow != nullptr && showLock != lockShown) { lockShown = showLock; lockRow->setVisible(showLock); }
    updateEnvKnobs();
    // v4
    if (modList != nullptr && modList->shownVersion != proc.getModVersion()) modList->refresh();
    const bool sync = proc.apvts.getRawParameterValue("lfosync")->load() > 0.5f;
    if (lfoRateKnob != nullptr) lfoRateKnob->setVisible(sync);
    if (lfoHzKnob != nullptr) lfoHzKnob->setVisible(!sync);
    stereoBtn.setToggleState(!proc.getExportMono(), juce::dontSendNotification);
    monoBtn.setToggleState(proc.getExportMono(), juce::dontSendNotification);
}

void Content::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(kBgTop, 0.0f, 0.0f, kBgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();
    g.setColour(kPanelEdge);
    g.drawHorizontalLine(56, 0.0f, (float) getWidth());
    // v9: PSYCHO BEAT LAB - same font as before: PSYCHO orange, BEAT white, LAB white a little smaller and a little further away
    juce::AttributedString as;
    as.append("PSYCHO ", ui(26.0f, true).withExtraKerningFactor(0.12f), LookAndFeel::orange());
    as.append("BEAT", ui(26.0f).withExtraKerningFactor(0.12f), juce::Colour(0xffe8ecf2));
    as.append("   LAB", ui(21.0f).withExtraKerningFactor(0.12f), juce::Colour(0xffe8ecf2));
    as.setJustification(juce::Justification::centredLeft);
    as.draw(g, juce::Rectangle<float>(18.0f, 8.0f, 330.0f, 40.0f));
}

bool Content::isInterestedInFileDrag(const juce::StringArray& f)
{
    for (auto& s : f)
    {
        const auto e = juce::File(s).getFileExtension().toLowerCase();
        if (e == ".wav" || e == ".aif" || e == ".aiff" || e == ".flac" || e == ".mp3" || e == ".ogg") return true;
    }
    return false;
}
void Content::filesDropped(const juce::StringArray& f, int, int)
{
    for (auto& s : f) if (proc.loadSample(juce::File(s))) break;
}
void Content::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser>("Choose a sample", juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                  "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadSample(fc.getResult()); });
}

// ============================================================================ editor
SlicerEditor::SlicerEditor(SlicerProcessor& p) : AudioProcessorEditor(&p), content(p) // v10: kBaseW is now 1290 (wider window)
{
    addAndMakeVisible(content);
    content.setBounds(0, 0, kBaseW, kBaseH);
    constrainer.setFixedAspectRatio((double) kBaseW / (double) kBaseH);
    constrainer.setSizeLimits((int) (kBaseW * 0.6), (int) (kBaseH * 0.6), (int) (kBaseW * 1.8), (int) (kBaseH * 1.8));
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize((int) (kBaseW * 0.85), (int) (kBaseH * 0.85));
}
SlicerEditor::~SlicerEditor() = default;
void SlicerEditor::resized()
{
    content.setBounds(0, 0, kBaseW, kBaseH);
    content.setTransform(juce::AffineTransform::scale((float) getWidth() / (float) kBaseW));
}

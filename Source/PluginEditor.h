#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace bsui
{
// v10: the window is about 25 % wider (1030 -> 1290). The layout is written for the v9 width (kOldW)
// and then spread out to the new width, so every part keeps its size and order - there is just more air.
inline constexpr int kOldW = 1030;
inline constexpr int kBaseW = 1290;
inline constexpr int kBaseH = 926;

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    // v10: the first line of the preset list - "Default" in orange, "Reset Plugin" in white
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>&, bool, bool, bool, bool, bool, const juce::String&,
                           const juce::String&, const juce::Drawable*, const juce::Colour*) override;

    static juce::Colour orange() { return juce::Colour(0xffff7a1a); }
    static juce::Colour cyan() { return juce::Colour(0xff22d3c5); }
    static juce::Colour lfo() { return juce::Colour(0xffb18cff); } // LFO / modulation colour
    static juce::Colour balagan() { return juce::Colour(0xffff2d4a); } // v8: BALAGAN red
    static juce::Colour env() { return juce::Colour(0xff5fe39a); }     // v12: ENV routing (green)
    static juce::Colour lampGreen() { return juce::Colour(0xff3ee07a); } // v12: FREE / BPM lamps
    static juce::Colour accentOf(const juce::Component& c)
    {
        if (c.getProperties().contains("accent")) return juce::Colour((juce::uint32) (int) c.getProperties()["accent"]);
        return orange();
    }
};

class Panel : public juce::Component
{
public:
    explicit Panel(juce::String t) : title(std::move(t)) {}
    void add(juce::Component* c, int w, int h) { items.push_back({ c, w, h }); resized(); }
    bool manages(const juce::Component* c) const { for (auto& it : items) if (it.comp == c) return true; return false; }
    void setAccent(juce::Colour c);
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::String title;

private:
    struct Item { juce::Component* comp; int w, h; };
    std::vector<Item> items;
    juce::Colour accent = juce::Colour(0xffff7a1a);
};

// v6: knob that turns a little slower than before (85 % of the v5 speed) and much finer while Shift is held.
// The drag is relative, so pressing or releasing Shift in the middle of a drag never makes the knob jump.
class KnobSlider : public juce::Slider
{
public:
    KnobSlider() : juce::Slider(juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow) {}
    static constexpr float kPixelsFullRange = 235.0f; // v5 was 200 px for the whole range -> 85 % speed
    static constexpr float kShiftFine = 0.15f;        // Shift: about 7x finer
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    float lastY = 0.0f, prop = 0.0f;
    bool ours = false;
};

class KnobCell : public juce::Component, public juce::DragAndDropTarget, private juce::Timer
{
public:
    KnobCell(juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label);
    ~KnobCell() override { stopTimer(); }
    void resized() override;
    void paintOverChildren(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    // v10: the knob edits something else than its parameter (the own envelope of the chosen slice). clearOverride = back to the parameter.
    void setOverride(float value, std::function<void(float)> onChange);
    void clearOverride();
    bool hasOverride() const { return overridden; }
    // LFO routing: drop the LFO handle on the knob. v12: the ENV handle too (only on the knobs where it makes sense)
    void setModTarget(SlicerProcessor* p, int modIndex);
    bool isInterestedInDragSource(const SourceDetails& d) override
    {
        const auto s = d.description.toString();
        return modIdx >= 0 && (s == "lfo" || (s == "env" && bs::envDestAllowed(modIdx)));
    }
    void itemDragEnter(const SourceDetails& d) override { dragOver = true; dragIsEnv = d.description.toString() == "env"; repaint(); }
    void itemDragExit(const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped(const SourceDetails&) override;
    SlicerProcessor* proc = nullptr;
    int modIdx = -1;
    bool dragOver = false, dragIsEnv = false;
    float shownAmt = 0.0f; bool shownOn = false;
    float shownEnvAmt = 0.0f; bool shownEnvOn = false; // v12
    void timerCallback() override;
    KnobSlider slider;
    juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    juce::AudioProcessorValueTreeState* state = nullptr;
    juce::String paramId;
    bool overridden = false;
};

// a row of small buttons bound to a choice parameter (or to a bool when labels = {"ON"} style via bool flag)
class ChoiceRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    // v10: `values` = the parameter index of every button (empty = 0, 1, 2 ...), so the screen order can differ from the saved order
    ChoiceRow(juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::StringArray& labels, bool isBool = false,
              std::vector<int> values = {});
    void resized() override;
    int preferredWidth() const { return totalW; }
    std::function<void(int)> onPick; // called after the user clicks a button (index)

private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::OwnedArray<juce::Label> labelsOwned;
    std::unique_ptr<juce::ParameterAttachment> attach;
    juce::RangedAudioParameter* param = nullptr;
    bool boolMode = false;
    int totalW = 0;
    std::vector<int> valueOf;
};

class WaveView : public juce::Component, private juce::Timer
{
public:
    explicit WaveView(SlicerProcessor&);
    ~WaveView() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    // v6: zoom. 0 = the whole sample, 1 = only the part between the orange markers (the anchor)
    void setZoom(float z);
    float getZoom() const { return zoom; }
    void anchorZoomToRegion();   // the zoom closes in on the current marker region

private:
    void timerCallback() override;
    void rebuildPeaks(const bs::SampleData*);
    int segmentAt(float x) const;
    // v12: the dots' own locators. In LOCK ONE SHOT every copy has them; in the dots they show on the chosen dots' boxes.
    struct TrimHandle { int dot; int which; float x; };
    std::vector<TrimHandle> trimHandles() const;
    bool trimRange(int dot, int& boxA, int& boxB) const; // the audio the locators of this dot are measured in
    int trimDot = -1, trimWhich = -1;
    float trimStartX = 0.0f;
    std::array<float, bs::kPatternLen> trimFrom {};
    uint32_t trimMask = 0;
    static bool slipKey(const juce::ModifierKeys& m) { return m.isCtrlDown() || m.isCommandDown(); }
    // v6: the visible part of the sample (normalised 0..1); x <-> sample position goes through it
    float viewA() const { return viewLo; }
    float viewSpan() const { return juce::jmax(1.0e-4f, viewHi - viewLo); }
    void updateView();
    float xToNorm(float x) const { return juce::jlimit(0.0f, 1.0f, viewA() + (x - 4.0f) / ((float) getWidth() - 8.0f) * viewSpan()); }
    float normToX(float n) const { return 4.0f + (n - viewA()) / viewSpan() * ((float) getWidth() - 8.0f); }
    SlicerProcessor& proc;
    float zoom = 0.0f, anchorLo = 0.0f, anchorHi = 1.0f, viewLo = 0.0f, viewHi = 1.0f, peaksLo = -1.0f, peaksHi = -1.0f;
    const bs::SampleData* peaksFor = nullptr;
    std::shared_ptr<const bs::SampleData> keep;
    std::vector<std::pair<float, float>> peaks;
    int dragging = 0; // 1 start, 2 end
    bool pickedSeg = false; // v10: this click chose a slice (SLICE envelope), nothing else happens
    int slipSeg = -1;  // slice being slipped (Ctrl + drag)
    float slipFrom = 0.0f, slipStartX = 0.0f;
    juce::uint64 shownSig = 0; // v13: the waveform is drawn again only when something it shows has changed
    juce::uint64 signature() const;
};

// v6: LOOP - a clear on/off button with a lamp, at the left edge of the beat bar
class LoopButton : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit LoopButton(SlicerProcessor&);
    ~LoopButton() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    void timerCallback() override;
    SlicerProcessor& proc;
    std::unique_ptr<juce::ParameterAttachment> attach;
    bool on = false, looping = false;
};

class BeatBar : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit BeatBar(SlicerProcessor&);
    ~BeatBar() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override { if (!proc.isPlayingNow() && !wasPlaying) repaint(); }
    // v13: while the loop plays, the dots are drawn on every frame of the screen, so the playhead moves smoothly with the sound
    juce::VBlankAttachment vblank { this, [this] { const bool p = proc.isPlayingNow(); if (p || wasPlaying) repaint(); wasPlaying = p; } };
    bool wasPlaying = false;
    void showStepMenu(int step);
    int stepAt(float x) const;
    float stepX(int i, int spb) const;
    bool onDot(juce::Point<float> p, int step) const;
    SlicerProcessor& proc;
    int downStep = -1;
    // v12: dragging a chosen dot moves every chosen dot by the same dB (each from its own level)
    uint32_t dragMask = 0;
    float dragFromDb[bs::kPatternLen] {};
    float downDb = 0.0f;
    // v10: selection box - drag on an empty spot to select dots; a click on any selected dot turns them all on / off
    int selGrid = -1;   // v11: the chosen dots themselves live in the processor
    bool boxing = false;
    juce::Point<float> boxFrom, boxTo;
    bool dragged = false;
    float startY = 0.0f;
    // v5: the dot is dragged in dB (fine steps, Shift = extra fine) and shows its level while dragging
    float lastY = 0.0f, dragDb = 0.0f;
};

// v10: SAMPLE / SLICE lamps next to the SLICE ENVELOPE title (only one is on)
class EnvModeLamps : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EnvModeLamps(SlicerProcessor&);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    juce::Rectangle<float> lamp(int i) const;
    SlicerProcessor& proc;
    std::unique_ptr<juce::ParameterAttachment> attach;
    int mode = 0;
};

// v8: an on/off button with a lamp, bound to a bool parameter (SUPER in the BALAGAN panel)
class LampButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    LampButton(juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label, juce::Colour colour);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    std::unique_ptr<juce::ParameterAttachment> attach;
    juce::String text;
    juce::Colour col;
    bool on = false;
};

// v12: FREE / BPM lamps next to the BALAGAN knob (only one is on; FREE = the default, green)
class BalModeLamps : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit BalModeLamps(juce::AudioProcessorValueTreeState&);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    juce::Rectangle<float> lamp(int i) const;
    std::unique_ptr<juce::ParameterAttachment> attach;
    int mode = 0;
};

// v12: the BALAGAN value, big and clear at the side of the monster (never over its mouth).
// Double-click = the number is cleared and an exact value (1..100) can be typed. In BPM mode the note value shows under it.
class BalaganValue : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit BalaganValue(juce::AudioProcessorValueTreeState&);
    ~BalaganValue() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::AudioProcessorValueTreeState& state;
    juce::Label editor;
};

// v7: a small button that shows the current choice of a parameter and opens a list (Noise type)
class ChoiceMenuButton : public juce::TextButton
{
public:
    ChoiceMenuButton(juce::AudioProcessorValueTreeState&, const juce::String& id);
    void clicked() override;

private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attach;
};

// v7: ADD LAYER - drop a sound here, it shows as a small wave
class LayerView : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit LayerView(SlicerProcessor&);
    ~LayerView() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;   // v13: drag the layer left / right (Shift = fine)
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    void fileDragEnter(const juce::StringArray&, int, int) override { hover = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { hover = false; repaint(); }

private:
    void timerCallback() override;
    SlicerProcessor& proc;
    const bs::SampleData* shownFor = nullptr;
    std::vector<std::pair<float, float>> peaks;
    std::unique_ptr<juce::FileChooser> chooser;
    bool hover = false;
    // v13: the two locators sit at the walls. The left one is where the chop starts; the layer slides under it.
    float pxPerSec() const;
    float lastX = 0.0f, downX = 0.0f, dragStart = 0.0f;
    bool moved = false, dragging = false;
    juce::uint64 shownSig = 0;
};

// v13: ADD LAYER - UNLINK (green lamp) + its arrow (the list of what the layer is linked back to)
class UnlinkButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit UnlinkButton(SlicerProcessor&);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    std::function<void()> onArrow;
    juce::Rectangle<float> arrowArea() const { return getLocalBounds().toFloat().removeFromRight(20.0f); }

private:
    SlicerProcessor& proc;
    std::unique_ptr<juce::ParameterAttachment> attach;
    bool on = false;
};

// v13: a small box that opens above the ADD LAYER panel (the layer's envelope, or the UNLINK list). It is not modal:
// the keys and every knob keep working while it is open. The button that opened it, or the x, closes it.
class FloatBox : public juce::Component
{
public:
    FloatBox(juce::String title, juce::Colour accent) : title(std::move(title)), accent(accent) { addAndMakeVisible(close); close.onClick = [this] { setVisible(false); }; }
    void paint(juce::Graphics&) override;
    void resized() override { close.setBounds(getWidth() - 26, 5, 20, 18); }
    juce::String title;
    juce::Colour accent;
    juce::TextButton close { "x" };
};

class LinkList : public juce::Component, private juce::Timer
{
public:
    explicit LinkList(SlicerProcessor& p) : proc(p) { startTimerHz(10); }
    ~LinkList() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    static constexpr int kRowH = 21, kTop = 22;

private:
    void timerCallback() override { const int k = (proc.isLayerUnlinked() ? 1 : 0) + proc.getLayerLinks() * 2; if (k != shown) { shown = k; repaint(); } }
    SlicerProcessor& proc;
    int shown = -1;
};

class RhythmBar : public juce::Component, private juce::Timer
{
public:
    explicit RhythmBar(SlicerProcessor&);
    ~RhythmBar() override { stopTimer(); }
    void resized() override;

private:
    void timerCallback() override;
    void showMenu();
    void step(int d);
    void askSave();
    void askExport(); // v12
    void askImport(); // v12
    void askNewLibrary(); // v13
    std::unique_ptr<juce::FileChooser> chooser;
    SlicerProcessor& proc;
    juce::TextButton prev { juce::CharPointer_UTF8("\xe2\x97\x80") }, next { juce::CharPointer_UTF8("\xe2\x96\xb6") }, save { "Save" }, nameButton;
    juce::String shown;
};

// ---- v4: LFO
class LfoEditor : public juce::Component, private juce::Timer
{
public:
    explicit LfoEditor(SlicerProcessor&);
    ~LfoEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced(6.0f, 6.0f); }
    juce::Point<float> toScreen(float x, float y) const;
    juce::Point<float> toShape(juce::Point<float>) const;
    int pointAt(juce::Point<float>) const;
    int curveAt(juce::Point<float>) const;
    juce::Point<float> curveHandle(const std::vector<bs::LfoPoint>&, int seg) const;
    void showMenu(int point);
    SlicerProcessor& proc;
    int dragPoint = -1, dragCurve = -1;
    float curveFrom = 0.0f, dragStartY = 0.0f;
};

class LfoHandle : public juce::Component, public juce::SettableTooltipClient
{
public:
    // v12: the same handle is used for the ENV ("env"): drag it onto a knob
    explicit LfoHandle(bool forEnv = false) : env(forEnv) {}
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    bool env = false;
};

class ModList : public juce::Component
{
public:
    explicit ModList(SlicerProcessor& p) : proc(p) {}
    void refresh();
    void paint(juce::Graphics&) override;
    int shownVersion = -1;

private:
    // v12: env = an ENV row (green), otherwise an LFO row
    struct Row { int dest; bool env = false; std::unique_ptr<juce::Label> name; std::unique_ptr<juce::Slider> amt; std::unique_ptr<juce::TextButton> del; };
    SlicerProcessor& proc;
    std::vector<std::unique_ptr<Row>> rows;
};

// ---- v4: drag the rendered loop out into the DAW
class ExportDrag : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ExportDrag(SlicerProcessor& p) : proc(p) { setTooltip("Drag into an audio track. Click = save the file and show it."); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override { started = false; }
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    SlicerProcessor& proc;
    bool started = false;
    juce::String status;
};

class Content : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit Content(SlicerProcessor&);
    ~Content() override;
    void paint(juce::Graphics&) override;
    bool isInterestedInFileDrag(const juce::StringArray& f) override;
    void filesDropped(const juce::StringArray& f, int, int) override;

private:
    KnobCell* knob(const char* id, const char* label);
    Panel* panel(const juce::String& title, juce::Rectangle<int> r);
    void chooseFile();
    void timerCallback() override;
    void spreadLayout();      // v10: v9 layout -> the wider window
    void layoutSampleHeader(); // v10: LOAD, CLEAR, RESET SAMPLE, bars, ONE SHOT, LOCK ONE SHOT (+ BPM), then the name
    void updateEnvKnobs();    // v10: the envelope knobs follow the chosen slice (SLICE lamp)

    SlicerProcessor& proc;
    LookAndFeel laf;
    juce::OwnedArray<KnobCell> knobs;
    juce::OwnedArray<Panel> panels;
    juce::OwnedArray<ChoiceRow> rows;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::OwnedArray<juce::Label> labelsOwned;
    std::unique_ptr<WaveView> wave;
    std::unique_ptr<BeatBar> beat;
    std::unique_ptr<LoopButton> loopBtn;            // v6
    std::unique_ptr<LayerView> layerView;          // v7
    std::unique_ptr<ChoiceMenuButton> noiseTypeBtn; // v7
    std::unique_ptr<LampButton> superBtn;          // v8: SUPER (BALAGAN)
    juce::Slider zoomSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }; // v6
    juce::Label zoomLabel;
    juce::Slider mainMix { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight }; // v9: MAIN MIX
    juce::Label mainMixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mainMixAttach;
    RhythmBar rhythmBar;
    juce::Label sampleInfo;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::MidiKeyboardComponent keyboard;
    // v4
    std::unique_ptr<LfoEditor> lfoEditor;
    LfoHandle lfoHandle;
    std::unique_ptr<ModList> modList;
    juce::Viewport modView;
    KnobCell* lfoRateKnob = nullptr;
    KnobCell* lfoHzKnob = nullptr;
    std::unique_ptr<ExportDrag> exportDrag;
    juce::TextButton stereoBtn { "STEREO" }, monoBtn { "MONO" };
    // v5: Standalone only - BPM of the internal clock
    juce::Label bpmLabel;
    juce::Slider bpmBox { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttach;
    juce::TooltipWindow tooltips { this, 500 };
    // v10
    Panel* samplePanel = nullptr;
    ChoiceRow* lockRow = nullptr;
    ChoiceRow* oneShotRow = nullptr;
    ChoiceRow* barsRow = nullptr;
    juce::TextButton* loadBtn = nullptr;
    juce::TextButton* clearBtn = nullptr;
    juce::TextButton* resetBtn = nullptr;
    std::unique_ptr<EnvModeLamps> envLamps;
    KnobCell* envKnobs[4] {};
    int envShownSeg = -2;
    // v11: chosen dots -> Attack, Decay, Sustain, Release and Length show and move those dots
    // v12: ... and Cutoff, Resonance, Noise, Pitch and Volume (every change goes to every chosen dot)
    KnobCell* lengthKnob = nullptr;
    KnobCell* dotKnobs[bs::kDotKnobCount] {};
    Panel* dotPanels[4] {};          // SLICE SHAPE, FILTER, SLICE ENVELOPE, PER-SLICE MIX (their titles show the chosen dots)
    juce::String dotPanelTitles[4];
    juce::String envShownKey;
    float dotShown[bs::kDotKnobCount] {};
    juce::String shownInfo;
    bool lockShown = true; // the row starts visible; the first timer tick hides it when not needed
    // v12
    LfoHandle envHandle { true };    // DRAG ENV ONTO A KNOB
    std::unique_ptr<BalModeLamps> balLamps;
    std::unique_ptr<BalaganValue> balValue;
    juce::TextButton* pickAllBtn = nullptr;
    void setDotTitles(const juce::String& suffix);
    // v13: ADD LAYER - ENVELOPE (opens the layer's own envelope) and UNLINK (+ the list of what stays linked)
    juce::TextButton layerEnvBtn { "ENVELOPE" };
    std::unique_ptr<UnlinkButton> unlinkBtn;
    std::unique_ptr<FloatBox> layerEnvBox, linkBox;
    std::unique_ptr<LinkList> linkList;
    Panel* layerPanel = nullptr;
    void toggleBox(FloatBox* box);
};
} // namespace bsui

class SlicerEditor : public juce::AudioProcessorEditor
{
public:
    explicit SlicerEditor(SlicerProcessor&);
    ~SlicerEditor() override;
    void paint(juce::Graphics&) override {}
    void resized() override;

private:
    bsui::Content content;
    juce::ComponentBoundsConstrainer constrainer;
};

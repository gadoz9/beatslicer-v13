#pragma once
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Rhythms.h"
#include "dsp/SliceEngine.h"

class SlicerProcessor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::AudioProcessorValueTreeState::Listener
{
public:
    SlicerProcessor();
    ~SlicerProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.allNotesOff(); }
    bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Psycho Beat Lab"; } // v9: new name
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- sample / slicing (message thread)
    bool loadSample(const juce::File& file);
    juce::String getSampleName() const { return sampleName; }
    double getSampleSeconds() const { return sampleSeconds; }
    int getSuggestedBars() const { return suggestedBars; }
    std::shared_ptr<const bs::Plan> getPlan() const { const juce::SpinLock::ScopedLockType l(planLock); return plan; }
    void rebuildPlan();
    void reroll();

    // rhythm / pattern
    void applyRhythm(int index);
    int getCurrentRhythm() const { return currentRhythm; }
    juce::String getRhythmName() const;
    float getStepVel(int i) const { return pattern[(size_t) juce::jlimit(0, bs::kPatternLen - 1, i)]; }
    void setStepVel(int i, float v);
    // per-step extras (pitch, reverse, stutter, long)
    const bs::StepExtras& getStepExtras() const { return extras; }
    void setStepSemi(int i, float semi);
    void setStepRev(int i, bool on);
    void setStepRep(int i, bool on);
    void setStepLen(int i, int steps);
    void setStepWah(int i, bool on);   // v6
    // v7
    void copyStepToNext(int i);
    void copyStepToAll(int i);
    void setStepSrc(int i, int src);   // 0 = the dot plays its own chop again
    // v8
    void setStepBal(int i, bool on);   // BALAGAN (red dot)
    void setStepPan(int i, float pan); // -1 left .. 1 right
    void applyCustom();                // v9: "Default - start from scratch" (resets the whole plugin)
    bool loadLayer(const juce::File&); // ADD LAYER
    void clearLayer();
    // v13: ADD LAYER - where the layer starts (seconds into its sound; below 0 = it waits), found by itself on load
    float getLayerStart() const { return layerStart.load(); }
    void setLayerStart(float sec);
    void autoAlignLayer();             // back to the start the plugin found (the silence before the sound skipped)
    // v13: UNLINK list - what the layer is linked back to (bits bs::kLink*)
    int getLayerLinks() const { return layerLinks.load(); }
    void setLayerLinks(int mask) { layerLinks.store(mask & ((1 << bs::kLinkCount) - 1)); }
    bool isLayerUnlinked() const { return raw("layunlink") > 0.5f; }
    std::shared_ptr<const bs::SampleData> getLayer() const { const juce::SpinLock::ScopedLockType l(planLock); return layer; }

    // factory presets (the whole sound, not only the rhythm)
    void applyFactoryPreset(int index);
    int getCurrentFactory() const { return currentFactory; }

    // slip: move the audio inside one slice without moving the slice in the rhythm
    float getSlip(int seg) const { return seg >= 0 && seg < (int) slips.size() ? slips[(size_t) seg] : 0.0f; }
    void setSlip(int seg, float normOffset);
    void resetSlip(int seg) { setSlip(seg, 0.0f); }
    bool isOneShot() const { return raw("oneshot") > 0.5f; }

    // v4: LFO shape (Serum-style points) and routing (drag the LFO onto a knob)
    std::vector<bs::LfoPoint> getLfoPoints() const { return lfoPoints; }
    void setLfoPoints(const std::vector<bs::LfoPoint>& pts);
    float getLfoPhase() const { return engine.lfoPhase(); }
    bool isModOn(int d) const { return d >= 0 && d < bs::kModCount && modOn[d].load(); }
    float getModAmount(int d) const { return d >= 0 && d < bs::kModCount ? modAmt[d].load() : 0.0f; }
    void addMod(int d);
    void setModAmount(int d, float amount);
    void removeMod(int d);
    void clearMods();                  // v12: removes the LFO and the ENV routing
    int getModVersion() const { return modVersion.load(); }
    // v12: ENV - the envelope of every chop, dragged onto a knob like the LFO (Cutoff, Pitch, Noise, Balagan, Bal. Filter)
    bool isEnvModOn(int d) const { return d >= 0 && d < bs::kModCount && envOn[d].load(); }
    float getEnvModAmount(int d) const { return d >= 0 && d < bs::kModCount ? envAmt[d].load() : 0.0f; }
    void addEnvMod(int d);
    void setEnvModAmount(int d, float amount);
    void removeEnvMod(int d);

    // v4: render the loop (or the one shot) to a wav file, ready to drag into the DAW
    juce::File renderToFile(bool mono);
    bool getExportMono() const { return exportMono; }
    void setExportMono(bool m) { exportMono = m; }
    static juce::File getExportDir();
    std::vector<float> getManualCuts() const { return manualCuts; }
    void addManualCut(float norm);
    void removeManualCut(float norm, float tol);
    int getLiveStep() const { return engine.liveStep(); }
    // v13: the step that is HEARD right now (the screen goes on smoothly from the last audio block and waits for the
    // sound to come out of the speakers). -1 = nothing plays. total = steps in the loop.
    double getHeardStep(int& total) const;
    // v5
    bool isStandalone() const { return wrapperType == juce::AudioProcessor::wrapperType_Standalone; }
    double getCurrentBpm() const { const double b = lastBpm.load(); return b > 1.0 ? b : (double) raw("tempo"); }
    // v6
    bool isPlayingNow() const { return engine.isActive(); }  // the playheads run only while this is true
    bool isLooping() const { return engine.isLatched(); }    // LOOP is holding the loop
    void clearSample();                                       // CLEAR button
    // v10
    void resetSample();                                       // RESET SAMPLE: back to the sample as it was loaded
    void setStepsOn(uint32_t mask, bool on);                  // the dots in the selection box, all on / off at once
    bool isOneShotLocked() const { return raw("oslock") > 0.5f; }
    bool isEnvSlice() const { return raw("envmode") > 0.5f; }
    int getSelectedSeg() const { return selectedSeg; }
    void setSelectedSeg(int seg) { selectedSeg = seg; if (seg >= 0) selDots = 0; } // v11: a box and dots are never chosen together
    bs::SegEnv getSegEnv(int seg) const;                      // the envelope a slice plays (own, or the knobs)
    bool segHasOwnEnv(int seg) const { return seg >= 0 && seg < (int) segEnvs.size() && segEnvs[(size_t) seg].on; }
    void setSegEnvValue(int seg, int which, float value);     // which: 0 attack, 1 decay, 2 sustain, 3 release
    // v11: chosen dots + their own envelope (Attack, Decay, Sustain, Release, Length - see bs::kDotEnv*)
    uint32_t getSelectedDots() const { return selDots; }
    void setSelectedDots(uint32_t mask) { selDots = mask; if (mask != 0) selectedSeg = -1; }
    float getDotEnvValue(int dot, int which) const;           // what the dot plays now (own value, else box / knob)
    float getDotsShownValue(uint32_t mask, int which) const;  // the highest of the chosen dots (what the knob shows)
    void moveDotsEnv(uint32_t mask, int which, float fromValue, float toValue); // every chosen dot moves from its own value
    void clearDotEnv(int dot);                                // "Clear step extras" also clears the dot's envelope
    bool dotHasOwnEnv(int dot) const { return dot >= 0 && dot < bs::kPatternLen && extras.env[dot].any(); }
    bool hasLockSnapshot() const { return lockSlot.valid; }   // LOCK ONE SHOT can be switched back on from the dots
    // v12: the dot's own locators (where its chop starts / ends, 0..1 of the chop). which = bs::kDotStart / bs::kDotEnd.
    float getDotTrim(int dot, int which) const;
    void setDotTrims(uint32_t mask, int which, const float* values); // values[i] for every dot in the mask (32 entries)
    // v12: chosen dots - one edit for several dots, and the plan is built once
    void editDots(uint32_t mask, const std::function<void(int, float&, bs::StepExtras&)>& fn);
    int stepsPerBarNow() const { return bs::stepsPerBarFor((int) raw("grid")); }
    // v12: Export / Import - a preset file that carries its sample (and layer) inside, so it works on any computer
    bool exportPreset(const juce::File& dest);
    juce::File importPreset(const juce::File& src);           // copies it into the presets folder; returns that file

    // user presets (whole state, incl. sample path)
    // v13: preset libraries. The folder "Documents\Psycho Beat Lab Presets" is made by the installer. Every folder in it
    // is a library in the preset list (with an arrow); "User" holds the presets saved with Save. Without that folder
    // (Standalone, not installed) nothing is created and the presets stay in Documents\BeatSlicer\Presets as before.
    static juce::File getLibraryRoot();
    static bool hasLibraryRoot() { return getLibraryRoot().isDirectory(); }
    static juce::File getOldPresetDir();
    juce::Array<juce::File> getLibraries() const;                   // the library folders (User first), only with the root
    static juce::Array<juce::File> getPresetFilesIn(const juce::File& dir) { return dir.findChildFiles(juce::File::findFiles, false, "*.bspreset"); }
    static juce::File createLibrary(const juce::String& name);      // a new folder in the root (empty file = could not)
    static juce::File getUserPresetDir();
    juce::Array<juce::File> getUserPresetFiles() const;
    bool saveUserPreset(const juce::String& name);
    static bool userPresetExists(const juce::String& name);   // v11: same name already saved -> ask before overwriting
    bool loadUserPreset(const juce::File& f);
    juce::String getPresetName() const { return presetName; }
    static juce::File getSampleStoreDir();                     // v12: where imported presets put their sounds

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    std::atomic<float> outputPeak { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    void parameterChanged(const juce::String& id, float) override;
    bs::Params readParams() const;
    float raw(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    void restoreFromState();
    void storeStateExtras();
    void setParam(const char* id, float plainValue);

    bs::Engine engine;
    mutable juce::SpinLock planLock;
    std::shared_ptr<const bs::Plan> plan, enginePlan;
    std::shared_ptr<const bs::SampleData> sample;
    juce::String samplePath, sampleName, presetName { "Init" };
    double sampleSeconds = 0.0;
    int suggestedBars = 1;
    float pattern[bs::kPatternLen] {};
    bs::StepExtras extras;
    std::vector<float> slips;
    int currentRhythm = 1;
    int currentFactory = -1;
    std::vector<bs::LfoPoint> lfoPoints;
    std::shared_ptr<const bs::LfoTable> lfoTable, engineLfo;
    std::atomic<float> modAmt[bs::kModCount] {};
    std::atomic<bool> modOn[bs::kModCount] {};
    std::atomic<float> envAmt[bs::kModCount] {}; // v12: ENV routing
    std::atomic<bool> envOn[bs::kModCount] {};
    std::atomic<int> modVersion { 0 };
    bool exportMono = false;
    std::vector<float> manualCuts;
    double sampleRateHz = 44100.0;
    std::atomic<double> lastBpm { 120.0 };
    std::atomic<bool> flushVoices { false };
    // v13
    std::atomic<float> layerStart { 0.0f }, layerAutoStart { 0.0f };
    std::atomic<int> layerLinks { 0 };
    struct PlayAnchor { double start = -1.0, sps = 0.0, ms = 0.0, latency = 0.0, sr = 44100.0; int total = 1; float swing = 0.0f; bool running = false; };
    mutable juce::SpinLock anchorLock;
    PlayAnchor playAnchor;
    // v7
    void copyStep(int from, int to);
    void applyPresetExtras(const bs::FactoryPreset&, int spb); // v12: balagan / wah / pan / copied chops of a preset
    std::shared_ptr<const bs::SampleData> layer, engineLayer;
    juce::String layerPath;
    juce::AudioFormatManager formats;
    std::vector<float> scratchR;
    // v10
    std::vector<bs::SegEnv> segEnvs;     // own envelope per slice box (SLICE lamp)
    int selectedSeg = -1;                // the box chosen on the waveform (SLICE lamp)
    std::atomic<bool> clearEnvPending { false };
    void clearSegEnvs() { segEnvs.clear(); selectedSeg = -1; }

    // v11: three separate states. The dots (normal), One Shot, and LOCK ONE SHOT each keep their own work;
    // switching ONE SHOT / LOCK ONE SHOT parks the state that is left and brings back the one that is entered.
    struct ModeSlot
    {
        bool valid = false;
        std::vector<std::pair<juce::String, float>> knobs;
        float pattern[bs::kPatternLen] {};
        bs::StepExtras extras;
        int rhythm = -1, factory = -1;
        bool hasDots = false; // the One Shot slot keeps only knobs
    };
    enum { modeDots = 0, modeOneShot = 1, modeLock = 2 };
    ModeSlot dotsSlot, oneShotSlot, lockSlot;
    int lastMode = modeDots;
    bool applyingSlot = false;
    int lastSpb = 16;
    uint32_t selDots = 0;
    int currentMode() const { return isOneShotLocked() ? modeLock : (isOneShot() ? modeOneShot : modeDots); }
    void syncModes();                       // message thread: does the switch between the states
    void saveSlot(ModeSlot&, bool withDots);
    void loadSlot(const ModeSlot&);
    void startLockFromLive();               // a new picture: the one shot as it is now, every dot on, nothing extra
    void resetSlots() { dotsSlot = ModeSlot(); oneShotSlot = ModeSlot(); lockSlot = ModeSlot(); lastMode = currentMode(); }
    void remapForGrid(int oldSpb, int newSpb, bool wholeDots);
    static void writeSlot(juce::ValueTree&, const ModeSlot&);
    static void readSlot(const juce::ValueTree&, ModeSlot&);
    static void writeDots(juce::ValueTree&, const float* pattern, const bs::StepExtras&);
    static void readDots(const juce::ValueTree&, float* pattern, bs::StepExtras&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlicerProcessor)
};

Psycho Beat Lab - Windows installation
======================================
1. Right-click Install_PsychoBeatLab.bat -> "Run as administrator".
   (Or copy the folder PsychoBeatLab.vst3 manually to  C:\Program Files\Common Files\VST3\
    and delete the old BeatSlicer.vst3 from that folder.)
2. Open Cubase -> Studio -> Plug-in Manager -> Update (rescan).
3. Add an Instrument Track and choose "Psycho Beat Lab".
   Projects saved with Beat Slicer open with Psycho Beat Lab (same plugin, new name).

PsychoBeatLab_Standalone.exe runs the synth without a DAW (useful for a quick sound check with the on-screen keyboard).

Presets (v13): the installer makes the folder  Documents\Psycho Beat Lab Presets
  - every folder inside it is a library in the preset list (a line with an arrow)
  - "User" holds the presets you save; your older presets from Documents\BeatSlicer\Presets are copied there
  - "Create New Library..." in the preset list makes a new folder there
Without that folder (for example the Standalone app without the installer) nothing is created
and your presets stay in  Documents\BeatSlicer\Presets  as before.
Sounds that come inside imported presets go to  Documents\BeatSlicer\Samples

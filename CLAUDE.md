# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

RipplerX is a JUCE-based modal/physically-modelled synth plugin (VST3, LV2, AU on macOS, plus Standalone by default). It is a C++17 port of the Rippler2 JSFX. JUCE and MTS-ESP (microtuning client) are git submodules under `libs/`. Clone with `--recurse-submodules`, or run `git submodule update --init --recursive`.

## Build

There are no tests and no lint setup. CI (`.github/workflows/release.yml`) only builds on all three OSes, and on `v*` tags it publishes a draft release.

```powershell
# Windows (configure + build)
cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -S . -B ./build
cmake --build ./build --config Release

# Faster iteration: build just one format target, e.g. the standalone app or VST3
cmake --build ./build --config Debug --target RipplerX_Standalone
cmake --build ./build --config Release --target RipplerX_VST3
```

- `-DBUILD_STANDALONE=OFF` skips the Standalone target (CI does this).
- Output goes to `build/RipplerX_artefacts/<Config>/<Format>/`. `COPY_PLUGIN_AFTER_BUILD` is on, so JUCE also copies plugins into the system plugin folders. On Windows that can fail without write access to `C:\Program Files\Common Files\VST3`.
- Sources and `resources/` are collected with `file(GLOB ...)`, so **re-run the CMake configure step after adding or removing files**. Anything in `resources/` is embedded through `juce_add_binary_data` and shows up as `BinaryData::<name>_<ext>`.
- The version is set in `project(RipplerX VERSION x.y.z)` in `CMakeLists.txt` and exposed as `PROJECT_VERSION`.

## Architecture

**Signal flow (per sample, in `RipplerXAudioProcessor::processBlockByType`):** MIDI is queued into `midi` with sample offsets and handled sample by sample (note on/off, sustain pedal, pitch-wheel glide). For each voice, the outputs of the mallet, the noise generator and the audio input are split into a *direct* path (`*_mix` params) and a *resonator* path (`*_res` params). The resonator path feeds resonator A, and then B either in parallel (`couple` = A+B, blended by `ab_mix`) or serially (A>B, where A's output feeds B). Everything is then summed, multiplied by `gain`, run through `Comb` (the stereoizer) and finally `Limiter`.

**Parameter handling:** All parameters are defined in one `AudioProcessorValueTreeState` in the `PluginProcessor.cpp` constructor. Every parameter change only sets `paramChanged`. At the start of the next block, `onSlider()` reads *all* params and pushes them into every voice (`Resonator::setParams`, `Voice::setPitch/setRatio/setCoupling`, `updateResonators`). The audio code does not react to individual parameter changes. When adding a param: declare it in the constructor, read it in `onSlider()` or `processBlockByType`, and wire it to the editor.

**DSP hierarchy (`src/dsp/`):**
- `Voice` (up to `globals::MAX_POLYPHONY` = 16 allocated; `polyphony` voices are active) holds a `Mallet`, a `Noise`, and two `Resonator`s. It computes the per-note model frequencies, and in serial coupling it also computes the A/B frequency split (`calcFrequencyShifts`).
- `Resonator` holds 64 `Partial`s (a bank of modal bandpass resonators) and a `Waveguide`. The Open Tube and Closed Tube models use the waveguide; every other model uses the partials. A resonator deactivates itself after one second of silence.
- `Models` stores the frequency-ratio tables (`aModels`/`bModels`, which are separate so that each resonator can have its own `ratio`) and the gain tables. Beam, Membrane and Plate are recalculated when their ratio changes. Djembe is computed per note.
- `Sampler` loads the embedded mallet samples (FLAC) or a user file. The user sample is stored base64-encoded in the plugin state (`userSample`).

**Enum ↔ choice-list coupling:** the `ModalModels` enum (`Models.h`) and the `MalletType` enum (`Mallet.h`) must stay index-aligned with the `StringArray`s of the `a_model`/`b_model` and `mallet_type` choice params. The `Reserved` mallet slots exist to keep older presets stable. The `*_partials` choice indices are not monotonic (`4,8,16,32,64,1,2`) and are mapped to counts in `onSlider()`.

**Model changes have side effects:** in `onSlider()`, a change of `a_model`/`b_model` resets that resonator's `ratio` to a per-model default (through `MessageManager::callAsync`) and clears the voices. `last_*_model`/`last_*_partials` track these changes, and `resetLastModels()` must be called after any state load so that loading a preset does not count as a model change and overwrite the preset's ratios.

**State/presets:** factory presets are XML files in `resources/presets/`. To add one, edit three places in `PluginProcessor.cpp`: the `setCurrentProgram` index→`BinaryData` chain, `getProgramName`, and `getNumPrograms`. `setStateInformation`/`setCurrentProgram` contain migrations (old Hz `a_cut`/`b_cut` values converted to the normalized range). Keep them, and add new ones for any parameter whose range or meaning changes. Per-user settings (UI scale, polyphony, dark theme) are not in the plugin state. They live in a JUCE `PropertiesFile` (`loadSettings`/`saveSettings`).

**UI (`PluginEditor`, `src/ui/`):** most knobs are custom `Rotary` components that bind directly to a param ID. A second param ID can be passed to make a knob velocity-modulated (`vel_*` params, edited in the UI's velocity-map mode). Combo boxes use standard APVTS attachments. Colors are swapped between light and dark themes through the mutable globals in `Globals.h`.

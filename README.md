# MPC3000 SIMULATOR (WIP)

VST3 sampler instrument (JUCE 8.0.6, C++17, CMake, Windows x64). No internal sequencer: MIDI comes from FL Studio's Piano Roll.

## Pads and banks
4 pad banks (A-D) x 16 pads = 64 pads, matching the 64-instrument MPC XPM programs. Default MIDI notes 36-99 (editable per pad in state; XPM `PadNoteMap` is applied on import). Mouse: click to play, right-click for menu (load / locate missing / clear / reverse / one-shot / mute / solo). FULL LEVEL = fixed velocity; otherwise velocity follows the click height.

## Sound library
Not embedded in the binary (175 MB). Install once:
1. Extract your `MPC_3000_Kit` ZIP (keep the original ZIP).
2. `Library\install_library.ps1 -Source "<...>\MPC 3000 Kit\MPC3000"` -> `%ProgramData%\MPC3000 SIMULATOR\Library`, or press DISK > FOLDER in the plugin and pick any folder.
Projects store library-relative paths (plus the absolute path as fallback). Missing files are never replaced: the pad shows a red dot, the LCD says FILE MISSING, and right-click > "Locate missing file..." restores it. See `Library/LIBRARY_REPORT.md`.

## Browser (DISK button)
Replaces the LCD: search, SAMPLES/PROGRAMS toggle, single-click audition, double-click loads to the selected pad or imports an XPM program, drag a sample onto any pad. Dropping an audio file or an .xpm from Explorer onto the pads also works.

## Build
    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release --target MPC3000_SIMULATOR_VST3
Headless test: `-DMPC_BUILD_TESTS=ON`, target `EngineTest <file.xpm>`.

## Control map (no sequencer)
Soft keys switch pages (MAIN/SAMPLE/PROGRAM/LOAD/SAVE/MIDI/HELP). TEMPO,/SYNC,COUNT,WAIT SEQ pick the field the DATA wheel edits (START/END/TUNE/LEVEL); cursor up/down = field, left/right = pad. Keypad: type pad 1-64 + ENTER. REC = MIDI learn. STOP = all sound off. PLAY / PLAY START = play selected pad; bottom PLAY START = raw sample audition. EDIT LOOP = reverse, EDIT PUNCH = one-shot/gate, EDIT STOP = mute, TAP TEMPO = solo, TIMING CORRECT = reset pad. Fader = level (NOTE VARIATION = tune, AFTER = pan). 16 LEVELS: pad number = velocity level.

## Status (backup 03)
UI rebuilt from the reference image (896x1200 design space, resizable). Linux VST3 build OK; headless render via Tests/Snapshot.cpp (run under xvfb). Not verified on Windows / FL Studio.

## Status (backup 02, earlier)
Verified: compiles as VST3 with JUCE 8.0.6 on Linux; EngineTest imports 154/155 real XPM programs (the 155th is empty), loads every referenced sample and every triggered pad produces audio.
NOT verified: Windows build, FL Studio behaviour, UI visuals vs. reference, MP3 decoding.
Known limitations: XPM import uses the first layer only (velocity layers, filters, envelopes, mute groups ignored); audio thread may free a sample if it held the last reference after a pad was replaced.
Pending: exact UI recreation (reference image copy missing from Resources/Graphics), waveform editing by mouse, MIDI Learn / external controller mapping, program save/load files, remaining hardware buttons, DATA wheel.

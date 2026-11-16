# Sound library analysis (MPC_3000_Kit ZIP)

- Root inside the ZIP: `MPC 3000 Kit/MPC3000/` (the plugin's library root is this folder).
- 1756 files, ~175 MB: 1597 WAV, 155 XPM programs, 4 MIDI demo files (`Standard/*.MID`, not used by the plugin).
- Categories (actual folders, none invented): Natural (472 WAV / 72 XPM), Synth (776 / 66), Percussion (249 / 13), Standard (100 / 4 + 4 MID).
- WAV format: all 44.1 kHz / 16-bit; 1488 mono, 109 stereo. No zero-byte or unreadable headers found.
- Duplicates: 1 byte-identical pair (`Natural/BR_J_CON_MU.WAV` = `Percussion/CNG_MID_M.WAV`). Both kept (XPM programs reference them by their own folder).
- Same file name in different folders occurs 16 times, so XPM sample references are resolved only inside the XPM's own folder.
- XPM: 155/155 parse; all 2909 sample references resolve in the XPM's folder (0 missing). One program (`Synth/6031_RADICAL__1_.xpm`) has no samples assigned. Programs have up to 64 instruments (4 banks of 16), so pad banks A-D are used.
- Redistribution rights are NOT established. Keep the library for your own projects; do not ship it commercially without clearing the rights.

Full per-file listing: `library_index.csv`.

# Writing WAV files moves to the engine
Type: fix
Status: open
Cycle: 1

## Goal
Alignment task, from the alignment review of `main..engine-game-layering` (CLAUDE.md). `writeAudioWav` in game/src/main.cpp both renders the game's weather sound and encodes it as a 16-bit stereo WAV file (float to int16 conversion, RIFF header, file write). The encoding is a file format, needed unchanged by any game that writes audio, the same as PNG writing (`pxe::writePng`, engine, per docs/architecture.md). Move the encoding to the engine; the game keeps the `--audio-wav` mode and what it renders.

## Decisions
- New engine function `pxe::writeWav`, beside `pxe::writePng`: takes a path, a sample rate, and interleaved stereo float samples in -1..1 (the format `pxe::AudioDevice` renders), clamps and converts them to 16-bit PCM exactly as the game does today (`static_cast<std::int16_t>(std::clamp(v, -1.f, 1.f) * 32767.f)`), writes the same 44-byte header, returns false on failure. Header and placement (image.hpp/.cpp, or a new audio_file header) are the developer's choice; prefer matching writePng's style and error handling.
- The game's `writeAudioWav` keeps rendering, the per-second level log and the thunder schedule, collects float samples, and calls `pxe::writeWav`. Its log lines and exit codes stay the same.
- The `--audio-wav` output must be byte-identical.

## Layering
- Engine: `pxe::writeWav` (WAV encoding).
- Game: `writeAudioWav` in main.cpp uses it.

## Acceptance criteria
- [ ] `pxe::writeWav` exists in the engine with a header comment stating format, sample layout, range and return value.
- [ ] game/src/main.cpp writes no RIFF/WAV bytes itself and does no float-to-int16 conversion.
- [ ] `--audio-wav out.wav` output is byte-identical to the cycle base, and its log (apart from the "wrote" line) is identical, for the default settings and for `--rain 1 --wind-strength 4` (or the flags the game actually offers for rain and wind).
- [ ] docs/engine-guide.md lists `writeWav` (Capture section and header reference); docs/architecture.md gets the row update from the orchestrator.
- [ ] The game's four standard captures are byte-identical to the cycle base, and `--self-test` passes.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None

## Out of scope
- Any change to the sound itself or to the `--audio-wav` mode's behaviour.
- Reading WAV files, other formats or bit depths.

## Cycle log
- Cycle 1: 2026-10-27 started, base 9ac02fc

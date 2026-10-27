## Cycle 1
Build: pass

### Changes
- engine/include/pxe/audio_file.hpp (new): declares `pxe::writeWav(path, sampleRate, interleavedStereo)`, with a header comment giving the format (16-bit PCM stereo WAV, 44-byte RIFF header), sample layout (interleaved L/R as `AudioDevice` renders), range (-1..1, clamped, scaled by 32767, truncated toward zero) and return value (false on non-positive rate, odd sample count, data too large for a 32-bit RIFF size, or open/write/close failure).
- engine/src/audio_file.cpp (new): the encoder. Builds the header and samples as explicit little-endian bytes in one buffer (the old game code wrote host-order ints, the same bytes on little-endian hosts), then one `fwrite`. Same conversion as before: `static_cast<std::int16_t>(std::clamp(v, -1.f, 1.f) * 32767.f)`.
- engine/CMakeLists.txt: adds `src/audio_file.cpp` after `src/image.cpp`.
- game/src/main.cpp: `writeAudioWav` keeps the rendering, thunder schedule and per-second level log, collects the float samples and calls `pxe::writeWav`. It returns 1 if that fails, as it did when `fopen` failed. Its RIFF writing and int16 conversion are gone.
- docs/engine-guide.md: the Capture section, now "Capture, writePng and writeWav", gets a `writeWav` bullet. The header reference gets an `audio_file.hpp` entry.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: new header `pxe/audio_file.hpp` with `pxe::writeWav`. I used a new header rather than image.hpp because WAV isn't an image. Its style follows writePng: free function, `const std::string&` path, bool result.
- Game: `writeAudioWav` in game/src/main.cpp still owns the `--audio-wav` mode, what it renders and its log.
- Misplaced code touched: none.
- docs/architecture.md not edited; it needs the orchestrator's row update (WAV writing: engine, `pxe::writeWav` in audio_file.hpp).

### Verification
Fresh Debug build (`build-check --fresh`): no warnings apart from stb's sprintf. The cycle base 51bebc4 was built from git archive in the scratchpad.
- `--audio-wav`, compared with cmp: default, `--rain 1 --wind-strength 4`, and `--rain 0.2 --wind 90 --wind-strength 0.5` all give byte-identical output, and their logs are identical apart from the "wrote" line. As a sanity check, default and rain/wind outputs differ from each other.
- `--audio-wav` into a directory that doesn't exist: exit 1 for both base and new.
- Captures: default, `--scene hills`, `--hour 21 --foxes 3`, `--hour 21 --candles --shelter-test --extra-fires 6` are all byte-identical, with identical logs.
- `--self-test`: PASS.

### How to playtest
- Nothing interactive changed. Run `./build/game/pxe --audio-wav out.wav --rain 1 --wind-strength 4` and listen: 20 s of wind and rain, thunder on the left at 2 s and on the right at 10 s, as before.

### Known limitations
- `writeWav` doesn't create parent directories, unlike `writePng`. I left it that way so the `--audio-wav` behaviour stays the same (it still fails if the directory is missing). Making it match writePng is a one-line change if wanted.
- The clamp path (|v| > 1) isn't exercised by these runs (peak level stays below 1), but it's the same expression as before.
- A failed or short `fwrite`/`fclose` now returns false (exit 1). Before, write errors were ignored and the run reported success. This only affects failing writes.

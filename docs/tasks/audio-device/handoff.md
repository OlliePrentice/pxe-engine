## Cycle 1
Build: pass

### Changes
- engine/include/pxe/audio_device.hpp (new): `pxe::AudioDevice`. `bool open(int sampleRate, Render render)` with `using Render = std::function<void(float* out, int frames)>`; opens the default output as interleaved stereo f32 at the given rate, starts it, and calls `render` on the audio thread. Returns false (nothing plays) if there is no output or it cannot start. Closes on destruction; copy deleted (and so no implicit move). Pimpl, so no miniaudio in the public header.
- engine/src/audio_device.cpp (new): the miniaudio device code moved from `game::Audio::start` / `~Audio`, unchanged in substance (`ma_device_config_init`, f32, 2 channels, init then start, uninit if start fails). `pUserData` points at the heap-allocated Impl holding the callback. Calling `open` again closes any device already open first.
- engine/src/miniaudio.cpp: moved from game/src/miniaudio.cpp with `git mv`. Same three defines (`MA_NO_ENCODING`, `MA_NO_DECODING`, `MA_NO_GENERATION`); only the comment changed ("for the engine").
- engine/CMakeLists.txt: adds `src/audio_device.cpp` and `src/miniaudio.cpp`; `src/miniaudio.cpp` joins the existing `-w` `set_source_files_properties` list; `${miniaudio_SOURCE_DIR}` added as a PRIVATE include directory.
- game/CMakeLists.txt: drops `src/miniaudio.cpp`, its `-w` property and `${miniaudio_SOURCE_DIR}` from the include path.
- game/src/audio.cpp: includes `<pxe/audio_device.hpp>` instead of `<miniaudio.h>`. `Audio::Impl` loses `ma_device device` and `bool open`, and gains `pxe::AudioDevice device` as its **last** member, so it is destroyed first and the audio thread stops before any state `render` reads is torn down (the old `~Audio` uninitialised the device before `impl_` was freed; this keeps that order). `~Audio` is now `= default`. `start()` is `impl->device.open(kRate, [impl](float* out, int frames) { impl->render(out, frames); })`. The synth code and `audio.hpp` are untouched.
- CMakeLists.txt (root): miniaudio comment now says the implementation is compiled in one source file of the engine.

### Issues addressed
- None (cycle 1).

### Acceptance checks
- `grep -rn "miniaudio\|ma_" game/src`: no matches (exit 1).
- miniaudio compiled in exactly one source, engine/src/miniaudio.cpp, with `-w` (checked in compile_commands.json); game/src/audio.cpp no longer has the miniaudio include path. No engine public header includes miniaudio.
- `--audio-wav --rain 0.6`: base build of 6b447a3 (git archive into /private/tmp/claude-501/audio-base, own build dir) and this cycle's build both written and compared with `cmp`: identical. SHA-1 of both: d038ea9a5f850dade6cfa8f81afb0308851352d7 (3.8 MB).
- Fresh build directory (Debug) configures and builds. The only warning is the pre-existing `sprintf` deprecation from third_party/stb/stb_image_write.h in game/src/main.cpp, which the base build also emits.

### Layering
- Engine: new `pxe::AudioDevice` (engine/include/pxe/audio_device.hpp, engine/src/audio_device.cpp); miniaudio implementation unit engine/src/miniaudio.cpp.
- Game: `game::Audio::Impl` owns the `pxe::AudioDevice` and passes its `render` as the callback; the synth stays in game/src/audio.cpp.
- Misplaced code touched: game/src/audio.cpp / game/src/miniaudio.cpp (the "Audio playback (miniaudio)" row of docs/architecture.md). The device is now in the engine, so that row can be marked in place; I did not edit docs/architecture.md.

### How to playtest
- Run the game normally: wind, rain and thunder should sound exactly as before, and the settings menu's volume sliders and sound toggle still apply.
- With no audio output device (e.g. all outputs disabled), the game should print "pxe: no audio output, running silent" and run as before.
- `pxe --audio-wav out.wav --rain 0.6` produces the same file as before.

### Known limitations
- The no-output path was not exercised on this machine (it has an output device). The logic is the same miniaudio init/start failure handling moved verbatim, and `Audio::start` returns `AudioDevice::open`'s result directly.

# Audio playback device in the engine
Type: feature
Status: open
Cycle: 1

## Goal
The miniaudio playback device lives in the game (`Audio::Impl::device` and `Audio::start` in game/src/audio.cpp, the implementation unit game/src/miniaudio.cpp). docs/architecture.md puts audio playback in the engine and the sounds themselves, including the synthesised wind, rain and thunder, in the game. Move the device into the engine as `pxe::AudioDevice`, which plays whatever the game renders. The synth stays in the game unchanged.

## Decisions
- `pxe::AudioDevice` opens the default output as interleaved stereo 32-bit float at a sample rate the caller gives, and calls a render callback the caller gives, `void(float* out, int frames)`, on the audio thread. Opening returns false if there is no output (today's "running silent" path). RAII close, non-copyable.
- miniaudio's implementation unit, its defines (`MA_NO_ENCODING`, `MA_NO_DECODING`, `MA_NO_GENERATION`) and its `-w` compile option move to the engine unchanged. The miniaudio include path becomes PRIVATE to `pxe_engine`; no engine public header includes miniaudio.
- `game::Audio`'s public interface is unchanged, including `render()` for `--audio-wav`, and the synth code is untouched apart from how the device is held.

## Layering
- Engine: `pxe::AudioDevice` (engine/include/pxe/audio_device.hpp, engine/src/audio_device.cpp), engine/src/miniaudio.cpp; engine/CMakeLists.txt.
- Game: `game::Audio` holds a `pxe::AudioDevice` and passes `Impl::render` as its callback; game/src/miniaudio.cpp is deleted; game/CMakeLists.txt drops miniaudio's include path and source. The root CMakeLists.txt comment about miniaudio is updated.

## Acceptance criteria
- [ ] `grep -rn "miniaudio\|ma_" game/src` finds nothing.
- [ ] `pxe::AudioDevice` is as described under Decisions, with no game content in names or comments.
- [ ] `game::Audio::start()` returns false and the game runs silent when there is no output, as today.
- [ ] `--audio-wav` output is byte-identical before and after (`--rain 0.6`), checked with `cmp` against the cycle base build. Record in the handoff.
- [ ] miniaudio compiles in exactly one engine source with the same defines and `-w`.
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
None

## Out of scope
- Moving the synth or its building blocks (they are game content per docs/architecture.md).
- Sample playback or decoding.

## Cycle log

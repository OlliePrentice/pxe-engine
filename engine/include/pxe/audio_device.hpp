#pragma once

#include <functional>
#include <memory>

namespace pxe {

// The default audio output, playing whatever the caller renders. Interleaved
// stereo, 32-bit float, at the sample rate given to open. The render
// callback runs on the audio thread, not the caller's: anything it shares
// with other threads needs atomics or a lock, and it should not block. The
// device closes when destroyed, and the callback is never called after that,
// so destroy the device before anything its callback reads.
class AudioDevice {
 public:
  // Fill `out` with `frames` frames of interleaved stereo (2 * frames floats).
  using Render = std::function<void(float* out, int frames)>;

  AudioDevice();
  ~AudioDevice();
  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;

  // Open the default output and start playing. False, and nothing plays, if
  // there is no output or it cannot start. Closes any output already open.
  bool open(int sampleRate, Render render);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace pxe

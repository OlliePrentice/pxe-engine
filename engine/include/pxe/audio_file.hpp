#pragma once

#include <string>
#include <vector>

namespace pxe {

// Writes `interleavedStereo` (left, right, left, right... as AudioDevice
// renders it, nominally -1..1) to a 16-bit PCM stereo WAV at `path`, with a
// 44-byte RIFF header. Each sample is clamped to -1..1 and scaled by 32767,
// truncating toward zero. Does not create directories. False if the sample
// rate is not positive, the sample count is odd or too large for a WAV, or
// the file can't be written.
bool writeWav(const std::string& path, int sampleRate, const std::vector<float>& interleavedStereo);

}  // namespace pxe

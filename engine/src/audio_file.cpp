#include "pxe/audio_file.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace pxe {

namespace {

void putU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xff));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xff));
}

void putTag(std::vector<std::uint8_t>& out, const char* tag) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(tag[i]));
}

}  // namespace

bool writeWav(const std::string& path, int sampleRate, const std::vector<float>& interleavedStereo) {
  constexpr std::uint16_t kChannels = 2;
  constexpr std::uint16_t kBits = 16;
  constexpr std::uint16_t kBlockAlign = kChannels * kBits / 8;
  constexpr std::size_t kHeaderBytes = 44;
  if (sampleRate <= 0 || interleavedStereo.size() % kChannels != 0) return false;
  const std::size_t dataBytes = interleavedStereo.size() * 2;
  if (dataBytes > 0xffffffffu - (kHeaderBytes - 8)) return false;
  const std::uint32_t bytes = static_cast<std::uint32_t>(dataBytes);

  std::vector<std::uint8_t> out;
  out.reserve(kHeaderBytes + dataBytes);
  putTag(out, "RIFF");
  putU32(out, static_cast<std::uint32_t>(kHeaderBytes - 8) + bytes);
  putTag(out, "WAVE");
  putTag(out, "fmt ");
  putU32(out, 16);  // fmt chunk size
  putU16(out, 1);   // PCM
  putU16(out, kChannels);
  putU32(out, static_cast<std::uint32_t>(sampleRate));
  putU32(out, static_cast<std::uint32_t>(sampleRate) * kBlockAlign);
  putU16(out, kBlockAlign);
  putU16(out, kBits);
  putTag(out, "data");
  putU32(out, bytes);
  for (float v : interleavedStereo) {
    const auto s = static_cast<std::int16_t>(std::clamp(v, -1.f, 1.f) * 32767.f);
    putU16(out, static_cast<std::uint16_t>(s));
  }

  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const bool written = std::fwrite(out.data(), 1, out.size(), f) == out.size();
  const bool closed = std::fclose(f) == 0;
  return written && closed;
}

}  // namespace pxe

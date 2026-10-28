#include "pxe/audio_device.hpp"

#include <miniaudio.h>

#include <utility>

namespace pxe {

struct AudioDevice::Impl {
  ma_device device{};
  bool open = false;
  Render render;

  void close() {
    if (!open) return;
    ma_device_uninit(&device);
    open = false;
  }
};

AudioDevice::AudioDevice() : impl_(std::make_unique<Impl>()) {}

AudioDevice::~AudioDevice() { impl_->close(); }

bool AudioDevice::open(int sampleRate, Render render) {
  impl_->close();
  impl_->render = std::move(render);
  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.sampleRate = static_cast<ma_uint32>(sampleRate);
  config.pUserData = impl_.get();
  config.dataCallback = [](ma_device* device, void* out, const void*, ma_uint32 frames) {
    static_cast<Impl*>(device->pUserData)->render(static_cast<float*>(out), static_cast<int>(frames));
  };
  if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) return false;
  if (ma_device_start(&impl_->device) != MA_SUCCESS) {
    ma_device_uninit(&impl_->device);
    return false;
  }
  impl_->open = true;
  return true;
}

}  // namespace pxe

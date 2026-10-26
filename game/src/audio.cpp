#include "audio.hpp"

#include <pxe/audio_device.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace game {
namespace {

constexpr float kPi = 3.14159265f;
constexpr float kDt = 1.f / static_cast<float>(Audio::kRate);

struct Random {
  std::uint32_t s;
  explicit Random(std::uint32_t seed) : s(seed) {}
  float uniform() {  // 0..1
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return static_cast<float>(s & 0xffffff) / 16777216.f;
  }
  float white() { return uniform() * 2.f - 1.f; }
};

// Per-sample coefficient of a one-pole smoother with time constant `tc`
// seconds, or of a one-pole low-pass at `hz`.
float smoothing(float tc) { return 1.f - std::exp(-kDt / tc); }
float lowPassCoeff(float hz) { return 1.f - std::exp(-2.f * kPi * hz * kDt); }

struct OnePole {
  float y = 0.f;
  float run(float x, float a) { return y += a * (x - y); }
};

// Chamberlin state-variable filter: low, band and high outputs at once.
struct Svf {
  float low = 0.f, band = 0.f, high = 0.f;
  void run(float x, float hz, float q) {
    const float f = 2.f * std::sin(kPi * std::min(hz, 7000.f) * kDt);
    high = x - low - q * band;
    band += f * high;
    low += f * band;
  }
};

// Random short bursts that decay away: drops striking. Each sample starts
// a burst with probability rate * dt; its level is the sum of the bursts
// still ringing.
struct Grains {
  float env = 0.f;
  float run(Random& r, float rate, float decay) {
    if (r.uniform() < rate * kDt) env += 0.3f + 0.7f * r.uniform();
    return env *= decay;
  }
};

// One roll of thunder: a crack if it is close, then a low rumble that
// swells and falls a few times as the sound arrives from different parts of
// the bolt, all lower and later the farther off it is.
struct Clap {
  float delay = 0.f;  // seconds until it is heard
  float t = 0.f;
  float distance = 0.5f;
  float loud = 1.f;
  float pan = 0.f;
  float swellAt[4] = {}, swellWidth[4] = {}, swellAmp[4] = {};
  float brown = 0.f;
  float crack = 1.f;
  Svf low, mid;
  OnePole crackLp;
};

}  // namespace

struct Audio::Impl {
  std::atomic<float> windTarget{1.f};
  std::atomic<float> rainTarget{0.f};
  std::atomic<float> master{0.8f}, windVol{0.7f}, rainVol{0.8f}, thunderVol{1.f};

  std::mutex pendingLock;
  std::vector<Clap> pending;  // queued by the game thread
  Random mainRandom{0x51ed27u};

  // Everything below belongs to the audio thread.
  std::vector<Clap> claps;
  Random rnd{0x2545f491u};
  float wind = 0.f, rain = 0.f;  // smoothed levels
  float gust = 0.5f, gustTarget = 0.5f, gustTimer = 0.f;
  // Leaves: a soft band of noise whose loudness drifts with a gentle,
  // smoothed flutter on top of the gusts. Nothing in it starts suddenly,
  // so it rustles without clicking.
  OnePole leafLow[2], leafHigh[2], windLow[2];
  float flutter[2] = {0.5f, 0.5f}, flutterTarget[2] = {0.5f, 0.5f}, flutterTimer[2] = {0.f, 0.f};
  OnePole hissLow[2], hissHigh[2], roar[2];
  Svf patterBand[2];
  Grains patter[2];

  // Last, so it closes first: the audio thread stops calling render before
  // anything render reads is destroyed.
  pxe::AudioDevice device;

  void render(float* out, int frames);
  float thunderSample(Clap& c);
};

float Audio::Impl::thunderSample(Clap& c) {
  if (c.delay > 0.f) {
    c.delay -= kDt;
    return 0.f;
  }
  c.t += kDt;
  const float d = c.distance;
  const float attack = std::min(1.f, c.t / (0.02f + 0.3f * d));
  const float decay = std::exp(-c.t / (1.6f + 1.6f * d));
  float swell = 0.f;
  for (int i = 0; i < 4; ++i) {
    const float x = (c.t - c.swellAt[i]) / c.swellWidth[i];
    swell += c.swellAmp[i] * std::exp(-x * x);
  }
  const float env = attack * decay * (0.45f + swell);

  // Brown noise for the body: the rumble, then a little of the mid band
  // for the rattle of a near strike.
  c.brown = 0.997f * c.brown + 0.03f * rnd.white();
  c.low.run(c.brown, 170.f - 100.f * d, 0.8f);
  c.mid.run(c.brown, 420.f, 1.2f);
  float s = env * (3.2f * c.low.low + 1.6f * (1.f - d) * c.mid.band);

  // A near strike cracks first: a burst of bright noise that dies in a
  // fraction of a second.
  if (d < 0.35f) {
    const float n = rnd.white();
    const float bright = n - c.crackLp.run(n, lowPassCoeff(900.f));
    s += c.crack * bright * 0.9f * (1.f - d / 0.35f);
    c.crack *= 1.f - smoothing(0.07f);
  }
  return s * c.loud;
}

void Audio::Impl::render(float* out, int frames) {
  if (pendingLock.try_lock()) {
    for (Clap& c : pending) claps.push_back(c);
    pending.clear();
    pendingLock.unlock();
  }
  const float windSet = 1.f - std::exp(-0.6f * std::max(windTarget.load(), 0.f));
  const float rainSet = std::clamp(rainTarget.load(), 0.f, 1.f);
  const float gm = master.load(), gw = windVol.load(), gr = rainVol.load(), gt = thunderVol.load();
  const float aLevel = smoothing(0.6f), aGust = smoothing(0.9f);
  const float aFlutter = smoothing(0.12f), patterDecay = 1.f - smoothing(0.004f);
  const float aWindLow = lowPassCoeff(220.f), aLeafLow = lowPassCoeff(4200.f), aLeafHigh = lowPassCoeff(900.f);
  const float aHissLow = lowPassCoeff(7500.f);
  const float aHissHigh = lowPassCoeff(600.f), aRoar = lowPassCoeff(300.f);

  for (int i = 0; i < frames; ++i) {
    wind += aLevel * (windSet - wind);
    rain += aLevel * (rainSet - rain);
    // Gusts: a new target every second or two, eased toward.
    if ((gustTimer -= kDt) <= 0.f) {
      gustTarget = 0.45f + 0.55f * rnd.uniform();
      gustTimer = 1.f + 2.5f * rnd.uniform();
    }
    gust += aGust * (gustTarget - gust);
    const float blow = gust * wind;

    // Thunder, each roll panned toward the side its flash lit from, never
    // all the way: it echoes round.
    float thunder[2] = {0.f, 0.f};
    for (Clap& c : claps) {
      const float t = thunderSample(c);
      const float p = 0.6f * c.pan;
      thunder[0] += t * std::sqrt(0.5f * (1.f - p));
      thunder[1] += t * std::sqrt(0.5f * (1.f + p));
    }

    for (int ch = 0; ch < 2; ++ch) {
      // Wind: mostly leaves rustling, a soft band of high noise that
      // swells with each gust and flutters gently within it, over a quiet
      // low rush.
      if ((flutterTimer[ch] -= kDt) <= 0.f) {
        flutterTarget[ch] = 0.35f + 0.65f * rnd.uniform();
        flutterTimer[ch] = 0.08f + 0.2f * rnd.uniform();
      }
      flutter[ch] += aFlutter * (flutterTarget[ch] - flutter[ch]);
      const float n1 = rnd.white();
      const float leafBand = leafLow[ch].run(n1 - leafHigh[ch].run(n1, aLeafHigh), aLeafLow);
      const float rustling = leafBand * blow * flutter[ch];
      const float rush = windLow[ch].run(rnd.white(), aWindLow) * blow;
      const float windSample = 0.6f * rustling + 1.f * rush;

      // Rain: a steady hiss, a patter of drops whose rate climbs with the
      // severity, and a low roar that only a downpour brings.
      const float n2 = rnd.white();
      const float bright = n2 - hissHigh[ch].run(n2, aHissHigh);
      const float hiss = hissLow[ch].run(bright, aHissLow) * std::pow(rain, 0.7f);
      patterBand[ch].run(rnd.white(), 2600.f, 0.9f);
      const float drops = patterBand[ch].band * patter[ch].run(rnd, 450.f * rain, patterDecay) * std::sqrt(rain);
      const float low = roar[ch].run(rnd.white(), aRoar) * rain * rain;
      const float rainSample = 0.2f * hiss + 0.3f * drops + 0.6f * low;

      out[i * 2 + ch] = gw * windSample + gr * rainSample + gt * thunder[ch];
    }
  }
  // A soft clip, so a close strike over a downpour saturates instead of
  // wrapping.
  for (int i = 0; i < frames * 2; ++i) out[i] = std::tanh(out[i] * gm);
  claps.erase(std::remove_if(claps.begin(), claps.end(), [](const Clap& c) { return c.t > 9.f; }), claps.end());
}

Audio::Audio() : impl_(std::make_unique<Impl>()) {}

Audio::~Audio() = default;

bool Audio::start() {
  Impl* impl = impl_.get();
  return impl->device.open(kRate, [impl](float* out, int frames) { impl->render(out, frames); });
}

void Audio::setWeather(float windStrength, float rain) {
  impl_->windTarget.store(windStrength);
  impl_->rainTarget.store(rain);
}

void Audio::setVolumes(const SoundSettings& sound) {
  impl_->master.store(sound.on ? sound.master : 0.f);
  impl_->windVol.store(sound.wind);
  impl_->rainVol.store(sound.rain);
  impl_->thunderVol.store(sound.thunder);
}

void Audio::thunder(float pan) {
  const float d = impl_->mainRandom.uniform();
  thunderAt(pan, d * d);  // more near strikes than far ones feel right in a storm
}

void Audio::thunderAt(float pan, float distance) {
  Random& r = impl_->mainRandom;
  Clap c;
  c.distance = std::clamp(distance, 0.f, 1.f);
  c.delay = 0.15f + 4.f * c.distance + 0.3f * r.uniform();
  c.loud = 1.f - 0.6f * c.distance;
  c.pan = std::clamp(pan, -1.f, 1.f);
  for (int i = 0; i < 4; ++i) {
    c.swellAt[i] = 0.2f + 3.2f * r.uniform() * (0.5f + c.distance);
    c.swellWidth[i] = 0.15f + 0.5f * r.uniform();
    c.swellAmp[i] = 0.3f + 0.7f * r.uniform();
  }
  std::lock_guard<std::mutex> lock(impl_->pendingLock);
  impl_->pending.push_back(c);
}

void Audio::render(float* out, int frames) { impl_->render(out, frames); }

}  // namespace game

#include "pxe/scene.hpp"

#include <cstdint>

namespace pxe {
namespace {

glm::vec3 hex(std::uint32_t rgb) {
  return glm::vec3{static_cast<float>((rgb >> 16) & 0xff), static_cast<float>((rgb >> 8) & 0xff),
                   static_cast<float>(rgb & 0xff)} /
         255.f;
}

// shadow, mid, lit
void setRamp(glm::vec3 (&out)[3], std::uint32_t shadow, std::uint32_t mid, std::uint32_t lit) {
  out[0] = hex(shadow);
  out[1] = hex(mid);
  out[2] = hex(lit);
}

}  // namespace

// The reference palettes. Day is sampled from the finished frame of
// t3ssel8r's scene video; golden hour, twilight and night follow his
// day/night video: warm lit faces against violet shadow, then a low-contrast
// dusk, then everything in moonlit navy. A building's slate roof (blue-grey)
// is the stone's ramps carried over, each phase's colour scaled from the
// slate's day colour as the stone's is from its own. Its wood (warm brown)
// and plaster (cream) follow the stone and bark through the day in their mid
// and lit tones, but indoors their shadow tone is the one most seen, so that
// stays warm in every phase: a dark warm brown for the wood and a muted warm
// beige for the plaster, each dimmed with the phase so it stays in key with
// the rest of it. A room's oak furniture is a honey oak, warm at every hour:
// amber-gold in golden hour, darker warm browns at twilight and night, never
// blue or grey (each tone's red above its green, its green above its blue),
// and lighter than the floor in every phase.
Scene::Scene() {
  struct Ramps {
    std::uint32_t band[3], detail[3], tip[3];
  };
  auto set = [&](Phase ph, Surface s, Ramps r) {
    setRamp(palette[ph][s].band, r.band[0], r.band[1], r.band[2]);
    setRamp(palette[ph][s].detail, r.detail[0], r.detail[1], r.detail[2]);
    setRamp(palette[ph][s].tip, r.tip[0], r.tip[1], r.tip[2]);
  };
  auto plain = [](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    return Ramps{{a, b, c}, {a, b, c}, {a, b, c}};
  };

  set(kPhaseDay, kSurfGrass, {{0x648474, 0x94b064, 0xacbc6c}, {0x74947c, 0x7c9c54, 0x8ca454}, {0x648474, 0xacbc6c, 0xc8d690}});
  set(kPhaseDay, kSurfDirt, plain(0x5c6468, 0x7c7874, 0x948c80));
  set(kPhaseDay, kSurfStone, plain(0x747480, 0xa49c8c, 0xbcac94));
  set(kPhaseDay, kSurfBark, plain(0x6a6a7a, 0x8a8684, 0xbcac90));
  set(kPhaseDay, kSurfLeaf, {{0x4c6e5c, 0x68905a, 0x88a850}, {0x58806a, 0x7ca25a, 0xaac65c}, {0x4c6e5c, 0x68905a, 0x88a850}});
  set(kPhaseDay, kSurfWater, {{0x74949c, 0x8cb4cc, 0x8cb4cc}, {0x84a4ac, 0xa4c8d8, 0xa4c8d8}, {0x74949c, 0x8cb4cc, 0x8cb4cc}});
  set(kPhaseDay, kSurfFoam, plain(0xc4d4d8, 0xdce8ec, 0xe8f2f2));
  set(kPhaseDay, kSurfWood, plain(0x4c3626, 0x86664e, 0xa88058));
  set(kPhaseDay, kSurfPlaster, plain(0x9c8a70, 0xc8bca2, 0xe2d6b6));
  set(kPhaseDay, kSurfRoof, plain(0x3c4454, 0x5a6676, 0x7e8a98));
  set(kPhaseDay, kSurfOak, plain(0x8c5a30, 0xb47a42, 0xd49c5c));
  rim[kPhaseDay] = hex(0xf5f0dc);
  ink[kPhaseDay] = hex(0x4c4c66);

  set(kPhaseGolden, kSurfGrass, {{0x44665c, 0x84a444, 0xb4c050}, {0x547464, 0x6c943c, 0x94ac3c}, {0x44665c, 0xa8b850, 0xe0d070}});
  set(kPhaseGolden, kSurfDirt, plain(0x4c4c5c, 0x6c6058, 0x9c8460));
  set(kPhaseGolden, kSurfStone, plain(0x5c5874, 0x9c8474, 0xdcb070));
  set(kPhaseGolden, kSurfBark, plain(0x545468, 0x84746c, 0xc09c70));
  set(kPhaseGolden, kSurfLeaf, {{0x3c5854, 0x5c8448, 0x94ac44}, {0x48685c, 0x749c44, 0xc0bc50}, {0x3c5854, 0x5c8448, 0x94ac44}});
  set(kPhaseGolden, kSurfWater, {{0x5c7488, 0x8ca0b4, 0x8ca0b4}, {0x6c8498, 0xc4bca8, 0xc4bca8}, {0x5c7488, 0x8ca0b4, 0x8ca0b4}});
  set(kPhaseGolden, kSurfFoam, plain(0xa8b4c4, 0xd8d4cc, 0xf0e0c0));
  set(kPhaseGolden, kSurfWood, plain(0x463022, 0x7a5a4a, 0xb27a48));
  set(kPhaseGolden, kSurfPlaster, plain(0x947c66, 0xb49c88, 0xf2c88a));
  set(kPhaseGolden, kSurfRoof, plain(0x30344c, 0x565662, 0x938d73));
  set(kPhaseGolden, kSurfOak, plain(0x8a5028, 0xb4703a, 0xe49c52));
  rim[kPhaseGolden] = hex(0xffe0a0);
  ink[kPhaseGolden] = hex(0x403860);

  set(kPhaseTwilight, kSurfGrass, {{0x2c4c50, 0x34565a, 0x3c5e5c}, {0x345a58, 0x3c625c, 0x446a60}, {0x2c4c50, 0x446a64, 0x507468}});
  set(kPhaseTwilight, kSurfDirt, plain(0x34364c, 0x3c3e54, 0x46465c));
  set(kPhaseTwilight, kSurfStone, plain(0x4c4868, 0x585474, 0x6c6484));
  set(kPhaseTwilight, kSurfBark, plain(0x40405c, 0x4c4c66, 0x585874));
  set(kPhaseTwilight, kSurfLeaf, {{0x203c44, 0x284648, 0x30504c}, {0x2c4a4c, 0x345450, 0x3c5e54}, {0x203c44, 0x284648, 0x30504c}});
  set(kPhaseTwilight, kSurfWater, {{0x34486c, 0x445a80, 0x445a80}, {0x40567c, 0x5c6c94, 0x5c6c94}, {0x34486c, 0x445a80, 0x445a80}});
  set(kPhaseTwilight, kSurfFoam, plain(0x6c7898, 0x848cac, 0x9ca4bc));
  set(kPhaseTwilight, kSurfWood, plain(0x34261f, 0x443e58, 0x504862));
  set(kPhaseTwilight, kSurfPlaster, plain(0x5c4e46, 0x645e7a, 0x787090));
  set(kPhaseTwilight, kSurfRoof, plain(0x272a44, 0x303762, 0x485088));
  set(kPhaseTwilight, kSurfOak, plain(0x5c3c24, 0x70482c, 0x825838));
  rim[kPhaseTwilight] = hex(0x9c94bc);
  ink[kPhaseTwilight] = hex(0x24203c);

  set(kPhaseNight, kSurfGrass, {{0x1c2448, 0x222c54, 0x28345e}, {0x222c54, 0x28345e, 0x2e3c6a}, {0x1c2448, 0x2e3c6a, 0x384a7c}});
  set(kPhaseNight, kSurfDirt, plain(0x1a1e3e, 0x20264a, 0x262e54));
  set(kPhaseNight, kSurfStone, plain(0x2a3260, 0x34406e, 0x445484));
  set(kPhaseNight, kSurfBark, plain(0x222a52, 0x2a345e, 0x343e6c));
  set(kPhaseNight, kSurfLeaf, {{0x141c3c, 0x1a2446, 0x202c52}, {0x1a2446, 0x202c52, 0x283662}, {0x141c3c, 0x1a2446, 0x202c52}});
  set(kPhaseNight, kSurfWater, {{0x1c2a58, 0x243668, 0x243668}, {0x2c4078, 0x3c5492, 0x3c5492}, {0x1c2a58, 0x243668, 0x243668}});
  set(kPhaseNight, kSurfFoam, plain(0x44548a, 0x54649a, 0x6474aa));
  set(kPhaseNight, kSurfWood, plain(0x221a1a, 0x262c50, 0x30385e));
  set(kPhaseNight, kSurfPlaster, plain(0x3e3432, 0x3e4876, 0x4e5a8a));
  set(kPhaseNight, kSurfRoof, plain(0x161d3f, 0x1d2a5d, 0x2e4388));
  set(kPhaseNight, kSurfOak, plain(0x3c2818, 0x4a3220, 0x583c28));
  rim[kPhaseNight] = hex(0x7084c0);
  ink[kPhaseNight] = hex(0x12163a);
}

}  // namespace pxe

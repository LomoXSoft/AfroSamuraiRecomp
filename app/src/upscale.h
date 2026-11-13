// Picture settings: upscaling (FSR 1 / CAS) with their sharpness, and FXAA. Used by
// the launcher (Settings -> Graphics), the in-game options (F1) and RecompLauncher
// (tools/recomplauncher.json): the afro_language.cfg keys and the runtime cvars they
// set. patches/rexglue/zzzzzzzzzzz-afro-fsr.patch adds FSR 1 and CAS to the build and
// zzzzzzzzzzzz-afro-fxaa-live.patch lets FXAA change while playing; all of these
// apply at once.
#pragma once

#include <cstdio>
#include <string>

namespace afro::upscale {

// upscale=: 0 bilinear (the original stretch, default), 1 AMD FSR 1, 2 AMD CAS.
constexpr int kModes = 3;
inline const char* Effect(int mode) {
  return mode == 1 ? "fsr" : mode == 2 ? "cas" : "bilinear";
}

// "a.bc" for hundredths, without floats: the same text in any locale.
inline std::string Hundredths(int h) {
  char b[16];
  std::snprintf(b, sizeof(b), "%d.%02d", h / 100, h % 100);
  return b;
}
inline int Clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// fsr_sharpness=: 0..100 (%). present_fsr_sharpness_reduction is in stops
// (0 sharpest, 2 softest): 2 * (100 - percent) / 100, so 100 % is the sharpest
// and 90 % the runtime's default reduction of 0.20.
constexpr int kSharpLo = 0, kSharpHi = 100, kSharpStep = 5, kSharpDefault = 90;
inline std::string SharpnessReduction(int percent) {
  return Hundredths(2 * (kSharpHi - Clamp(percent, kSharpLo, kSharpHi)));
}

// cas_sharpness=: 0..100 (%), present_cas_additional_sharpness 0.00..1.00. 0 is
// CAS's lightest sharpening and the runtime's default. CAS also sharpens FSR's
// picture when the screen is not larger than the game's picture.
constexpr int kCasLo = 0, kCasHi = 100, kCasStep = 5, kCasDefault = 0;
inline std::string CasSharpness(int percent) { return Hundredths(Clamp(percent, kCasLo, kCasHi)); }

// fxaa=: 0 off (default), 1 FXAA, 2 FXAA extreme quality: the GPU plugin's
// swap_post_effect, run on the game's picture before the upscaling.
constexpr int kFxaaModes = 3;
inline const char* PostEffect(int mode) {
  return mode == 1 ? "fxaa" : mode == 2 ? "fxaa_extreme" : "none";
}

}  // namespace afro::upscale

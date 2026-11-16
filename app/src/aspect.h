// Picture aspect ratio (ultrawide, Hor+). Used by the launcher (Settings -> Graphics),
// the in-game options (F1) and RecompLauncher (tools/recomplauncher.json): the
// afro_language.cfg key and the runtime cvars it sets at start-up.
//
// The game draws a 1280x720 picture (its back buffer and effect targets are
// hard-coded) with every camera at 16:9. For a wider screen the runtime renders
// that picture with more columns than rows (draw_resolution_scale_x > _y), the
// presenter shows it at the wide aspect (video_mode_width/height: only their ratio
// is used, the game itself keeps seeing 1280x720), and frame_pacing.cpp widens
// each 16:9 camera to match (afro_aspect): more of the scene at the sides, the
// menus and HUD stay 16:9 in the middle. All of it is set when the game starts.
#pragma once

#include <cmath>

namespace afro::aspect {

// aspect=: 0 16:9 (original, default), 1 21:9, 2 32:9, 3 Auto (the screen's shape).
constexpr int kModes = 4;
constexpr double k169 = 16.0 / 9.0;
constexpr double kMax = 32.0 / 9.0;  // widest tested picture
constexpr int kMaxScale = 7;         // the runtime's limit per axis

inline double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

// The picture's width / height. Auto takes the screen's shape; a screen not wider
// than 16:9 (16:10, 4:3) keeps the original picture, a wider one up to 32:9.
// 21:9 is 64:27 (2560x1080; 3440x1440 is within 1 %).
inline double Ratio(int mode, int screen_w, int screen_h) {
  switch (mode) {
    case 1: return 64.0 / 27.0;
    case 2: return kMax;
    case 3: {
      if (screen_w <= 0 || screen_h <= 0) return k169;
      const double r = double(screen_w) / double(screen_h);
      return r < k169 * 1.01 ? k169 : Clamp(r, k169, kMax);
    }
    default: return k169;
  }
}
inline bool IsWide(double ratio) { return ratio > k169 * 1.001; }

// Columns: at least as many guest pixels per screen pixel as the rows get
// (internal resolution scale * ratio / 16:9), at most the runtime's 7.
inline int ScaleX(int scale, double ratio) {
  const int s = int(std::ceil(scale * ratio / k169 - 1e-6));
  return s < scale ? scale : s > kMaxScale ? kMaxScale : s;
}

// The presenter's aspect, as a video mode 720 rows high (the runtime clamps the
// width to 4095, far above 32:9's 2560).
inline int VideoModeWidth(double ratio) { return int(std::lround(720.0 * ratio)); }

}  // namespace afro::aspect

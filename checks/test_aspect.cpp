// Aspect ratio setting (app/src/aspect.h): the picture's ratio, the render scale
// per axis and the presenter's video mode for each choice. Build: any C++17
// compiler, e.g.  clang++ -std=c++17 -I app/src checks/test_aspect.cpp
#include "aspect.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace afro::aspect;

static int failures = 0;
static void Check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
static bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

int main() {
  // Fixed choices; 16:9 (0) is the original picture whatever the screen.
  Check(Near(Ratio(0, 3440, 1440), 16.0 / 9.0), "16:9 stays 16:9 on an ultrawide screen");
  Check(!IsWide(Ratio(0, 5120, 1440)), "16:9 is not wide");
  Check(Near(Ratio(1, 0, 0), 64.0 / 27.0), "21:9 is 64:27 (2560x1080)");
  Check(Near(Ratio(2, 0, 0), 32.0 / 9.0), "32:9");
  // Auto: the screen's shape, 16:9 or narrower keeps the original, at most 32:9.
  Check(Near(Ratio(3, 1920, 1080), 16.0 / 9.0), "auto 1920x1080 -> 16:9");
  Check(Near(Ratio(3, 1920, 1200), 16.0 / 9.0), "auto 16:10 -> 16:9");
  Check(Near(Ratio(3, 1280, 800), 16.0 / 9.0), "auto Steam Deck 1280x800 -> 16:9");
  Check(Near(Ratio(3, 3440, 1440), 3440.0 / 1440.0), "auto 3440x1440");
  Check(Near(Ratio(3, 5120, 1440), 32.0 / 9.0), "auto 5120x1440 -> 32:9");
  Check(Near(Ratio(3, 7680, 1440), 32.0 / 9.0), "auto wider than 32:9 is capped");
  Check(Near(Ratio(3, 0, 0), 16.0 / 9.0), "auto without a screen size -> 16:9");
  Check(Near(Ratio(9, 3440, 1440), 16.0 / 9.0), "unknown value -> 16:9");
  // Columns: never fewer guest pixels per screen pixel than rows, at most 7.
  Check(ScaleX(1, 64.0 / 27.0) == 2, "21:9 at 1x renders 2x1");
  Check(ScaleX(2, 64.0 / 27.0) == 3, "21:9 at 2x renders 3x2");
  Check(ScaleX(3, 64.0 / 27.0) == 4, "21:9 at 3x renders 4x3");
  Check(ScaleX(1, 32.0 / 9.0) == 2, "32:9 at 1x renders 2x1");
  Check(ScaleX(2, 32.0 / 9.0) == 4, "32:9 at 2x renders 4x2 (5120x1440)");
  Check(ScaleX(3, 32.0 / 9.0) == 6, "32:9 at 3x renders 6x3");
  Check(ScaleX(1, 16.0 / 9.0) == 1, "16:9 keeps the scale");
  Check(ScaleX(7, 32.0 / 9.0) == kMaxScale, "scale capped at the runtime's 7");
  for (int s = 1; s <= 3; ++s)
    for (double r : {64.0 / 27.0, 3440.0 / 1440.0, 32.0 / 9.0})
      Check(ScaleX(s, r) >= s * r / k169 - 1e-9, "enough columns for the ratio");
  // Presenter: a video mode 720 rows high with the ratio's width.
  Check(VideoModeWidth(64.0 / 27.0) == 1707, "21:9 video mode 1707x720");
  Check(VideoModeWidth(32.0 / 9.0) == 2560, "32:9 video mode 2560x720");
  Check(VideoModeWidth(kMax) <= 0x0FFF, "within the runtime's 4095");
  if (failures) return EXIT_FAILURE;
  std::printf("aspect ratio setting: all checks passed\n");
  return EXIT_SUCCESS;
}

// Guest frame counter + optional frame cap, driven by AfroSwapHook (afro_hooks.toml),
// which runs right before the game's D3D Swap calls VdSwap (once per guest frame).
#include "frame_pacing.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

#include <cmath>
#include <cstring>
#include <string>

REXCVAR_DEFINE_INT32(afro_fps_cap, 0, "Afro",
                     "Guest frame cap in FPS (0 = off, the game's own pacing)");
REXCVAR_DEFINE_BOOL(afro_fps_log, true, "Afro", "Log guest FPS every 5 seconds");

// Diagnostics for the black-screen bug: the game's own debug-menu switches for
// its fullscreen effects (found via the "Graphics|Fullscreen Effects|..." menu
// registration at 0x826B8xxx; each setter is "stb r3,<addr>"). false = force off.
REXCVAR_DEFINE_BOOL(afro_fx_dof, true, "Afro", "Game fullscreen effect: depth of field");
REXCVAR_DEFINE_BOOL(afro_fx_bloom, true, "Afro", "Game fullscreen effect: bloom");
REXCVAR_DEFINE_BOOL(afro_fx_sepia, true, "Afro", "Game fullscreen effect: sepia");
REXCVAR_DEFINE_BOOL(afro_fx_blood, true, "Afro", "Game fullscreen effect: blood shader");
REXCVAR_DEFINE_BOOL(afro_fx_focus, true, "Afro", "Game fullscreen effect: focus (slow-mo)");
REXCVAR_DEFINE_BOOL(afro_fx_log, false, "Afro", "Log fullscreen effect state changes");

namespace {
struct FxFlag { const char* name; uint32_t addr; bool (*on)(); };
const FxFlag kFx[] = {
    {"dof", 0x828B0E8E, [] { return (bool)REXCVAR_GET(afro_fx_dof); }},
    {"bloom", 0x828B0EBC, [] { return (bool)REXCVAR_GET(afro_fx_bloom); }},
    {"sepia", 0x828B0EAB, [] { return (bool)REXCVAR_GET(afro_fx_sepia); }},
    {"blood", 0x828B0D1B, [] { return (bool)REXCVAR_GET(afro_fx_blood); }},
    {"focus", 0x828B0E94, [] { return (bool)REXCVAR_GET(afro_fx_focus); }},
};
// Blood effect runtime state (sub_826FC508): active byte, level mask, params.
constexpr uint32_t kBloodActive = 0x828B0E8F, kBloodMask = 0x828B0EA0,
                   kBloodParams = 0x828B0D20 + 172;
std::string g_fx_last;

uint32_t BE32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

void FxTick() {
  if (!rex::system::kernel_state()) return;
  auto* mem = rex::system::kernel_memory();
  if (!mem) return;
  auto at = [&](uint32_t a) { return mem->TranslateVirtual<uint8_t*>(a); };
  std::string st;
  for (auto& f : kFx) {
    uint8_t* b = at(f.addr);
    if (!f.on()) *b = 0;
    st += std::string(f.name) + "=" + std::to_string(*b) + " ";
  }
  st += "blood_active=" + std::to_string(*at(kBloodActive)) +
        " blood_mask=" + std::to_string(BE32(at(kBloodMask))) + " params=";
  bool bad = false;
  for (int i = 0; i < 13; ++i) {
    uint32_t u = BE32(at(kBloodParams + i * 4));
    float f; std::memcpy(&f, &u, 4);
    if (!std::isfinite(f)) bad = true;
    char buf[24]; std::snprintf(buf, sizeof buf, "%g,", f); st += buf;
  }
  if (bad) st += " NONFINITE";
  if (REXCVAR_GET(afro_fx_log) && st != g_fx_last) {
    REXLOG_INFO("afro fx: {}", st);
    g_fx_last = st;
  }
}

using clk = std::chrono::steady_clock;
std::mutex g_mu;
uint64_t g_frames = 0;
clk::time_point g_next;        // limiter deadline
clk::time_point g_win_start;   // stats window
uint64_t g_win_frames = 0;
double g_fps = 0, g_ft_ms = 0;
clk::time_point g_last;
}  // namespace

void AfroSwapHook() {
  FxTick();
  const int cap = REXCVAR_GET(afro_fps_cap);
  if (cap > 0) {
    const auto period = std::chrono::duration_cast<clk::duration>(
        std::chrono::duration<double>(1.0 / cap));
    auto now = clk::now();
    if (g_next.time_since_epoch().count() == 0 || now - g_next > period * 4) g_next = now;
    // Sleep coarse, then spin the last ~1.5 ms for accuracy.
    while (true) {
      now = clk::now();
      if (now >= g_next) break;
      auto left = g_next - now;
      if (left > std::chrono::microseconds(1500))
        std::this_thread::sleep_for(left - std::chrono::microseconds(1500));
      else
        std::this_thread::yield();
    }
    g_next += period;
  }
  std::lock_guard lk(g_mu);
  auto now = clk::now();
  if (g_frames) g_ft_ms = std::chrono::duration<double, std::milli>(now - g_last).count();
  g_last = now;
  ++g_frames;
  if (g_win_frames == 0) g_win_start = now;
  ++g_win_frames;
  double secs = std::chrono::duration<double>(now - g_win_start).count();
  if (secs >= 5.0) {
    g_fps = (g_win_frames - 1) / secs;
    if (REXCVAR_GET(afro_fps_log))
      REXLOG_INFO("afro: guest {:.1f} FPS (cap {})", g_fps, cap);
    g_win_frames = 0;
  } else if (g_fps == 0 && secs >= 1.0) {
    g_fps = (g_win_frames - 1) / secs;
  }
}

namespace afro {
rex::ui::FrameStats GuestFrameStats() {
  std::lock_guard lk(g_mu);
  rex::ui::FrameStats s;
  s.fps = g_fps;
  s.frame_time_ms = g_ft_ms;
  s.frame_count = g_frames;
  return s;
}
}  // namespace afro

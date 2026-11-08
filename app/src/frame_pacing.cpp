// Guest frame counter + optional frame cap, driven by AfroSwapHook (afro_hooks.toml),
// which runs right before the game's D3D Swap calls VdSwap (once per guest frame).
// Present-interval / Experimental dt hooks also live here (see afro_hooks.toml).
#include "frame_pacing.h"
#include "player_hp_invuln.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

#include <cmath>
#include <cstring>
#include <cstdint>
#include <string>

REXCVAR_DEFINE_INT32(afro_fps_cap, 0, "Afro",
                     "Guest frame cap in FPS (0 = off, the game's own pacing)");
REXCVAR_DEFINE_BOOL(afro_fps_log, true, "Afro",
                    "Log guest FPS / dt / present interval every ~2 seconds");

REXCVAR_DEFINE_INT32(afro_present_interval, 2, "Afro",
                     "Guest D3D present interval (2 = original 30 FPS, 1 = unlocked ~60)");

REXCVAR_DEFINE_STRING(afro_tick_mode, "original", "Afro",
                      "Tick mode request: original|force30|force60|varying (flags unmapped)");

REXCVAR_DEFINE_BOOL(afro_fx_dof, true, "Afro", "Game fullscreen effect: depth of field");
REXCVAR_DEFINE_BOOL(afro_fx_bloom, true, "Afro", "Game fullscreen effect: bloom");
REXCVAR_DEFINE_BOOL(afro_fx_sepia, true, "Afro", "Game fullscreen effect: sepia");
REXCVAR_DEFINE_BOOL(afro_fx_blood, true, "Afro", "Game fullscreen effect: blood shader");
REXCVAR_DEFINE_BOOL(afro_fx_focus, true, "Afro", "Game fullscreen effect: focus (slow-mo)");
REXCVAR_DEFINE_BOOL(afro_fx_log, false, "Afro", "Log fullscreen effect state changes");

// Session test aid: block protagonist char-HP loss only. Default OFF.
// Not saved. Enemies are not affected. See docs/PLAYER_INVULN_v0017.md.
REXCVAR_DEFINE_BOOL(afro_player_invuln, false, "Afro",
                    "Test: player char HP does not decrease (session; not a full god-mode)");

namespace {
struct FxFlag { const char* name; uint32_t addr; bool (*on)(); };
const FxFlag kFx[] = {
    {"dof", 0x828B0E8E, [] { return (bool)REXCVAR_GET(afro_fx_dof); }},
    {"bloom", 0x828B0EBC, [] { return (bool)REXCVAR_GET(afro_fx_bloom); }},
    {"sepia", 0x828B0EAB, [] { return (bool)REXCVAR_GET(afro_fx_sepia); }},
    {"blood", 0x828B0D1B, [] { return (bool)REXCVAR_GET(afro_fx_blood); }},
    {"focus", 0x828B0E94, [] { return (bool)REXCVAR_GET(afro_fx_focus); }},
};
constexpr uint32_t kBloodActive = 0x828B0E8F, kBloodMask = 0x828B0EA0,
                   kBloodParams = 0x828B0D20 + 172;

// Guest timing globals written by sub_826A0AE8:
//   0x828B1C04 = primary sim dt (fixed-step feed for movement) — safe to force to 1/60
//   0x828B1BF0 = measured frame dt — leave alone
//   0x828B1C08 = sometimes a copy of measured dt, sometimes 1.0 time-scale — NEVER force
constexpr uint32_t kDtAddr = 0x828B1C04;
constexpr uint32_t kDtMeasuredAddr = 0x828B1BF0;
constexpr uint32_t kDtScaleAddr = 0x828B1C08;

// Player singleton + the combat HP writer (see docs/PLAYER_INVULN_v0017.md):
//   *(uint32*)0x828E629C = player character
//   player+0x280 = char HP float stored by sub_82666218 at 0x826662D8 and 0x82666308
//   player+0x284 = max HP (ceiling for restores; never written by this code)
//   player+0x7D4 = HP meter object; meter+0x4 is secondary (not the restore target)
//   meter+0x3C = retail "block set-when-new<=0" flag (not sufficient on its own)
// sub_82674030 / VA 0x826740D8 is a codegen label collision, not this writer.
constexpr uint32_t kPlayerPtr = 0x828E629C;
constexpr uint32_t kPlayerHpMeterOff = 0x7D4;
constexpr uint32_t kPlayerHpFloatOff = 0x280;
constexpr uint32_t kPlayerHpMaxOff = 0x284;
constexpr uint32_t kMeterCurOff = 0x4;
constexpr uint32_t kMeterBlockLethalOff = 0x3C;
constexpr uint32_t kHpStoreA = 0x826662D8;
constexpr uint32_t kHpStoreB = 0x82666308;
// bl sub_82663180: the generated LR is 0x82666318, so the bl is the previous word.
constexpr uint32_t kMeterDeathCall = 0x82666314;
// Function entry. r3 is still the meter argument, before lfs/fcmpu/ble.
constexpr uint32_t kMeterDeathEntry = 0x82663180;
constexpr uint32_t kMeterHp0 = 0;       // float compared with 0
constexpr uint32_t kMeterFlag60 = 60;   // byte cleared on the lethal path
constexpr uint32_t kMeterFlag63 = 63;   // byte set on the lethal path
constexpr uint32_t kInvulnDetailCap = 8;
constexpr float kDt30 = 1.f / 30.f;
constexpr float kDt60 = 1.f / 60.f;

std::string g_fx_last;

uint32_t BE32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

float ReadGuestFloat(uint32_t addr) {
  auto* mem = rex::system::kernel_memory();
  if (!mem) return 0.f;
  const uint32_t u = rex::memory::load_and_swap<uint32_t>(mem->TranslateVirtual(addr));
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

void WriteGuestFloat(uint32_t addr, float f) {
  auto* mem = rex::system::kernel_memory();
  if (!mem) return;
  uint32_t u;
  std::memcpy(&u, &f, 4);
  rex::memory::store_and_swap<uint32_t>(mem->TranslateVirtual(addr), u);
}

int ClampedPresentInterval() {
  int v = REXCVAR_GET(afro_present_interval);
  if (v < 1) v = 1;
  if (v > 4) v = 4;
  return v;
}

bool Experimental60() { return ClampedPresentInterval() == 1; }

const char* TickModeName() {
  const std::string& m = REXCVAR_GET(afro_tick_mode);
  if (m == "force30") return "force30";
  if (m == "force60") return "force60";
  if (m == "varying") return "varying";
  return "original";
}

void TickModeOnce() {
  static bool reported = false;
  if (reported) return;
  reported = true;
  const char* mode = TickModeName();
  if (std::strcmp(mode, "original") == 0) {
    REXLOG_INFO("afro tick_mode=original (guest Force*/AllowVaryingTick flags untouched)");
    return;
  }
  REXLOG_WARN(
      "afro tick_mode={}: Force60HzTick byte mapped at 0x828E63A3 but only a debug-draw "
      "consumer found so far; Force30/Varying/Allow still soft-unmapped. No guest flags "
      "written; only afro_present_interval applies.",
      mode);
}

bool GuestPtrPlausible(uint32_t p) {
  // Guest heap/code window used by this title; reject null / low / clearly wild.
  return p >= 0x10000 && p < 0x90000000;
}

afro_invuln::Hold g_inv_hold;
uint32_t g_inv_clamps = 0;
uint32_t g_inv_restores = 0;
uint32_t g_inv_meter = 0;
uint32_t g_inv_hp_player = 0;    // stfs sites, r31 is the singleton
uint32_t g_inv_hp_other = 0;     // stfs sites, r31 is some other entity
uint32_t g_inv_call_player = 0;  // bl site, r31 is the singleton
uint32_t g_inv_call_other = 0;   // bl site, r31 is some other entity
uint32_t g_inv_gate_match = 0;   // death entry, r3 is the player meter
uint32_t g_inv_gate_miss = 0;    // death entry, r3 is not the player meter
uint32_t g_inv_lethal = 0;       // player meter+0 was <= 0 at the death entry
uint32_t g_inv_detail = 0;
bool g_inv_suppress = false;
bool g_inv_logged_on = false;
bool g_inv_logged_off = false;
bool g_inv_logged_ready = false;
bool g_inv_logged_miss = false;
bool g_inv_logged_meter = false;
bool g_inv_logged_hp_other = false;
bool g_inv_logged_call_other = false;
bool g_inv_logged_gate_miss = false;
bool g_inv_logged_lethal = false;

bool InvulnAllowDetail() {
  if (g_inv_detail < kInvulnDetailCap) {
    ++g_inv_detail;
    return true;
  }
  if (!g_inv_suppress) {
    g_inv_suppress = true;
    REXLOG_INFO(
        "afro invuln: further detail suppressed (cap {}); "
        "clamps/restores stay on the ~2s fps line",
        kInvulnDetailCap);
  }
  return false;
}

void InvulnResetSession() {
  g_inv_hold = {};
  g_inv_logged_ready = false;
  g_inv_logged_miss = false;
  g_inv_logged_meter = false;
}

std::string InvulnFpsSuffix() {
  const bool on = (bool)REXCVAR_GET(afro_player_invuln);
  const bool any = g_inv_clamps || g_inv_restores || g_inv_meter || g_inv_hp_player ||
                   g_inv_hp_other || g_inv_call_player || g_inv_call_other ||
                   g_inv_gate_match || g_inv_gate_miss || g_inv_lethal;
  if (!on && !any) return {};
  // hpP/hpO: char-HP stores whose base is / isn't the singleton.
  // callP/callO: the death call with r31 matching / not matching.
  // gateM/gateX: death entry whose meter is / isn't *(player+0x7D4).
  // lethal: that player meter was <= 0 before the compare.
  return "  invuln=" + std::string(on ? "on" : "off") +
         " clamps=" + std::to_string(g_inv_clamps) +
         " restores=" + std::to_string(g_inv_restores) +
         " meter_guards=" + std::to_string(g_inv_meter) +
         " hpP=" + std::to_string(g_inv_hp_player) +
         " hpO=" + std::to_string(g_inv_hp_other) +
         " callP=" + std::to_string(g_inv_call_player) +
         " callO=" + std::to_string(g_inv_call_other) +
         " gateM=" + std::to_string(g_inv_gate_match) +
         " gateX=" + std::to_string(g_inv_gate_miss) +
         " lethal=" + std::to_string(g_inv_lethal);
}

void PlayerInvulnTick() {
  const bool on = (bool)REXCVAR_GET(afro_player_invuln);
  if (!on) {
    if (g_inv_logged_on && !g_inv_logged_off && InvulnAllowDetail())
      REXLOG_INFO("afro invuln: disabled (player HP store hook off)");
    g_inv_logged_off = g_inv_logged_on;
    g_inv_logged_on = false;
    InvulnResetSession();
    return;
  }
  if (!g_inv_logged_on) {
    g_inv_logged_on = true;
    g_inv_logged_off = false;
    if (InvulnAllowDetail())
      REXLOG_INFO(
          "afro invuln: cvar ON — char stfs @0x{:08X}/@0x{:08X}; "
          "meter+0 guard before bl @0x{:08X} and at sub_82663180 entry @0x{:08X}. "
          "Death test is meter+0<=0, not f1. Enemies unchanged.",
          kHpStoreA, kHpStoreB, kMeterDeathCall, kMeterDeathEntry);
  }
  if (!rex::system::kernel_state()) return;
  auto* mem = rex::system::kernel_memory();
  if (!mem) return;

  const uint32_t player =
      rex::memory::load_and_swap<uint32_t>(mem->TranslateVirtual(kPlayerPtr));
  if (!GuestPtrPlausible(player)) {
    if (!g_inv_logged_miss) {
      g_inv_logged_miss = true;
      if (InvulnAllowDetail())
        REXLOG_INFO("afro invuln: player ptr not ready (0x{:08X})", player);
    }
    return;
  }
  const uint32_t meter = rex::memory::load_and_swap<uint32_t>(
      mem->TranslateVirtual(player + kPlayerHpMeterOff));
  // Secondary retail gate only. The combat writer does not honor it.
  if (GuestPtrPlausible(meter)) {
    *mem->TranslateVirtual<uint8_t*>(meter + kMeterBlockLethalOff) = 1;
  } else if (!g_inv_logged_meter) {
    g_inv_logged_meter = true;
    if (InvulnAllowDetail())
      REXLOG_INFO("afro invuln: meter ptr bad (player=0x{:08X} meter=0x{:08X})",
                  player, meter);
  }

  const float cur = ReadGuestFloat(player + kPlayerHpFloatOff);
  const float hp_max = ReadGuestFloat(player + kPlayerHpMaxOff);
  const float meter_hp =
      GuestPtrPlausible(meter) ? ReadGuestFloat(meter + kMeterCurOff) : 0.f;
  const afro_invuln::FallbackDecision decision =
      afro_invuln::DecideFallback(g_inv_hold, cur, hp_max);

  if (!g_inv_logged_ready && g_inv_hold.value >= 0.f) {
    g_inv_logged_ready = true;
    if (InvulnAllowDetail())
      REXLOG_INFO(
          "afro invuln: player=0x{:08X} hp_char={:.1f} hp_max={:.1f} hold={:.1f} "
          "meter={:.1f} (char HP is the restore source; meter/+0x{:X} is secondary)",
          player, cur, hp_max, g_inv_hold.value, meter_hp, kMeterBlockLethalOff);
  }

  if (decision.action != afro_invuln::FallbackAction::kWriteChar) return;
  if (!std::isfinite(decision.value)) return;
  // Char HP only. Do not write the meter and do not raise player+0x284.
  WriteGuestFloat(player + kPlayerHpFloatOff, decision.value);
  ++g_inv_restores;
  if (InvulnAllowDetail())
    REXLOG_INFO(
        "afro invuln: player HP fallback {:.1f} -> {:.1f} "
        "(hp_max {:.1f} player=0x{:08X})",
        cur, decision.value, hp_max, player);
}

void FxTick() {
  if (!rex::system::kernel_state()) return;
  auto* mem = rex::system::kernel_memory();
  if (!mem) return;
  auto at = [&](uint32_t a) { return mem->TranslateVirtual<uint8_t*>(a); };
  static bool reported = false;
  if (!reported) {
    reported = true;
    std::string off;
    for (auto& f : kFx) if (!f.on()) off += std::string(f.name) + " ";
    REXLOG_INFO("afro fx: forced off: {}", off.empty() ? "none" : off);
  }
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
    float f;
    std::memcpy(&f, &u, 4);
    if (!std::isfinite(f)) bad = true;
    char buf[24];
    std::snprintf(buf, sizeof buf, "%g,", f);
    st += buf;
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
clk::time_point g_next;
clk::time_point g_win_start;
uint64_t g_win_frames = 0;
double g_fps = 0, g_ft_ms = 0;
clk::time_point g_last;
std::atomic<int> g_seen_interval{-1};
std::atomic<int> g_dt_feed_logged{0};
// sub_82676300 timeType convert: mode0=*1/30, mode1=*1/60, mode2=passthrough, 3/4=other
std::atomic<uint64_t> g_cine_mode_counts[5]{};

// Experimental: primary sim dt at 0x828B1C04 + 1/30 const→1/60 loads.
// feb8ba6 also forced 1BF0/1C08 (1C08 is sometimes 1.0 scale) → ~4x cutscenes.
// 92f93d1 dropped const rewrites → intro still too fast (per-present 1/30 steps).
void ForceExperimentalPrimaryDt() {
  if (!Experimental60()) return;
  if (!rex::system::kernel_state()) return;
  WriteGuestFloat(kDtAddr, kDt60);
  if (g_dt_feed_logged.exchange(1) == 0) {
    REXLOG_INFO(
        "afro dt: Experimental feed {:.6f}s (1/60) -> 0x828B1C04 only "
        "(1BF0 measured + 1C08 scale untouched; 1/30 const loads -> 1/60 for anim/cutscene)",
        kDt60);
  }
}

void ForceDevicePresentInterval(uint32_t device) {
  if (device < 0x1000) return;
  auto* mem = rex::system::kernel_memory();
  if (!mem) return;
  const int v = ClampedPresentInterval();
  rex::memory::store_and_swap<uint32_t>(mem->TranslateVirtual(device + 0x3c),
                                        static_cast<uint32_t>(v));
  if (v == 1) {
    const uint32_t flags6c =
        rex::memory::load_and_swap<uint32_t>(mem->TranslateVirtual(device + 0x6c));
    if (flags6c & 0x200)
      rex::memory::store_and_swap<uint32_t>(mem->TranslateVirtual(device + 0x6c),
                                            flags6c & ~uint32_t(0x200));
  }
  const int prev = g_seen_interval.load(std::memory_order_relaxed);
  g_seen_interval.store(v, std::memory_order_relaxed);
  if (prev < 0) {
    const uint32_t flags6c =
        rex::memory::load_and_swap<uint32_t>(mem->TranslateVirtual(device + 0x6c));
    REXLOG_INFO(
        "afro present: device+0x3c interval={} (cvar afro_present_interval={}) "
        "device+0x6c=0x{:08X} bit0x200={} (forced each Swap)",
        v, REXCVAR_GET(afro_present_interval), flags6c, (flags6c & 0x200) ? 1 : 0);
  }
}

void PatchPrimaryDtStore(PPCRegister& base, PPCRegister& f, int32_t offset) {
  if (!Experimental60()) return;
  // Only the primary sim-dt slot (7172 == 0x1C04).
  if (offset != 7172) return;
  f.f64 = double(kDt60);
  if (base.u32 < 0x1000) return;
  auto* mem = rex::system::kernel_memory();
  if (!mem) return;
  uint32_t u;
  std::memcpy(&u, &kDt60, 4);
  rex::memory::store_and_swap<uint32_t>(mem->TranslateVirtual(base.u32 + offset), u);
  ForceExperimentalPrimaryDt();
}
}  // namespace

void AfroPresentIntervalHook(PPCRegister& r11) {
  r11.u64 = static_cast<uint64_t>(ClampedPresentInterval());
}

void AfroPresentIntervalHook2(PPCRegister& r11) {
  r11.u64 = static_cast<uint64_t>(ClampedPresentInterval());
}

void AfroPresentBitHook(PPCRegister& r10) {
  if (REXCVAR_GET(afro_present_interval) == 1)
    r10.u64 = r10.u32 & ~uint32_t(0x200);
}

void AfroPresentIntervalConsumeHook(PPCRegister& r29) {
  const int v = ClampedPresentInterval();
  r29.u64 = static_cast<uint64_t>(v);
  g_seen_interval.store(v, std::memory_order_relaxed);
  PlayerInvulnTick();
}

// sub_82666218 stfs f0,640(r31) at 0x826662D8 and 0x82666308 (char HP, +0x280).
// afro_hooks.toml omits after_instruction, so this runs BEFORE the store and
// f0 is what stfs writes. Same symbol at both sites. Not sub_82674030.
void AfroPlayerHpStoreHook(PPCRegister& r31, PPCRegister& f0) {
  if (!(bool)REXCVAR_GET(afro_player_invuln)) return;
  if (!rex::system::kernel_state()) return;
  auto* mem = rex::system::kernel_memory();
  if (!mem) return;
  const uint32_t player =
      rex::memory::load_and_swap<uint32_t>(mem->TranslateVirtual(kPlayerPtr));
  // Any other entity, including enemies, keeps the guest f0.
  if (!GuestPtrPlausible(player) || r31.u32 != player) {
    if (GuestPtrPlausible(r31.u32)) {
      ++g_inv_hp_other;
      if (!g_inv_logged_hp_other) {
        g_inv_logged_hp_other = true;
        REXLOG_INFO(
            "afro invuln: hp store not the singleton r31=0x{:08X} player=0x{:08X} f0={:.1f}",
            r31.u32, player, static_cast<float>(f0.f64));
      }
    }
    return;
  }
  ++g_inv_hp_player;

  const float cur = ReadGuestFloat(player + kPlayerHpFloatOff);
  const float hp_max = ReadGuestFloat(player + kPlayerHpMaxOff);
  const float incoming = static_cast<float>(f0.f64);
  const afro_invuln::StoreDecision decision =
      afro_invuln::DecideStore(g_inv_hold, incoming, cur, hp_max);
  if (decision.action != afro_invuln::StoreAction::kReplace) return;
  if (!std::isfinite(decision.value)) return;
  f0.f64 = static_cast<double>(decision.value);
  ++g_inv_clamps;
  if (InvulnAllowDetail())
    REXLOG_INFO(
        "afro invuln: player HP store {:.1f} -> {:.1f} "
        "(char {:.1f} hp_max {:.1f} hold {:.1f} player=0x{:08X})",
        incoming, decision.value, cur, hp_max, g_inv_hold.value, player);
}

// Writes player meter+0 only. f1 is intentionally not a parameter: sub_82663180
// compares the loaded meter+0 (f0) with 0, then ble to the lethal block.
void GuardPlayerMeter(uint32_t meter, uint32_t player) {
  if (!GuestPtrPlausible(meter) || !GuestPtrPlausible(player)) return;
  auto* mem = rex::system::kernel_memory();
  if (!mem) return;
  const uint32_t player_meter = rex::memory::load_and_swap<uint32_t>(
      mem->TranslateVirtual(player + kPlayerHpMeterOff));
  if (meter != player_meter) return;
  const float meter0 = ReadGuestFloat(meter + kMeterHp0);
  const float hp_max = ReadGuestFloat(player + kPlayerHpMaxOff);
  const float char_hp = ReadGuestFloat(player + kPlayerHpFloatOff);
  const afro_invuln::MeterDecision decision =
      afro_invuln::DecideMeter(g_inv_hold, meter0, char_hp, hp_max);
  if (!decision.write || !std::isfinite(decision.value) || !(decision.value > 0.f)) return;
  WriteGuestFloat(meter + kMeterHp0, decision.value);
  ++g_inv_meter;
  if (InvulnAllowDetail()) {
    const uint8_t flag60 = *mem->TranslateVirtual<uint8_t*>(meter + kMeterFlag60);
    const uint8_t flag63 = *mem->TranslateVirtual<uint8_t*>(meter + kMeterFlag63);
    REXLOG_INFO(
        "afro invuln: meter+0 {:.1f} -> {:.1f} lethal_skip={} "
        "meter+60={} meter+63={} player=0x{:08X}",
        meter0, decision.value, decision.lethal ? 1 : 0, flag60, flag63, player);
  }
}

uint32_t LoadPlayerSingleton() {
  if (!rex::system::kernel_state()) return 0;
  auto* mem = rex::system::kernel_memory();
  if (!mem) return 0;
  return rex::memory::load_and_swap<uint32_t>(mem->TranslateVirtual(kPlayerPtr));
}

// Before bl 0x82663180 in sub_82666218. r31 is the entity, r3 is the meter.
void AfroPlayerMeterBeforeDeathCall(PPCRegister& r31, PPCRegister& r3) {
  if (!(bool)REXCVAR_GET(afro_player_invuln)) return;
  const uint32_t player = LoadPlayerSingleton();
  if (!GuestPtrPlausible(player) || r31.u32 != player) {
    if (GuestPtrPlausible(r31.u32)) {
      ++g_inv_call_other;
      if (!g_inv_logged_call_other) {
        g_inv_logged_call_other = true;
        REXLOG_INFO(
            "afro invuln: death call not the singleton r31=0x{:08X} player=0x{:08X} r3=0x{:08X}",
            r31.u32, player, r3.u32);
      }
    }
    return;
  }
  ++g_inv_call_player;
  GuardPlayerMeter(r3.u32, player);
}

// First instruction of sub_82663180. r3 is the meter argument. Memory is
// repaired before lfs f0,0(r31) / fcmpu / ble loc_8266320C.
void AfroPlayerMeterDeathGate(PPCRegister& r3) {
  if (!(bool)REXCVAR_GET(afro_player_invuln)) return;
  const uint32_t player = LoadPlayerSingleton();
  if (!GuestPtrPlausible(player)) return;
  auto* mem = rex::system::kernel_memory();
  if (!mem) return;
  const uint32_t player_meter = rex::memory::load_and_swap<uint32_t>(
      mem->TranslateVirtual(player + kPlayerHpMeterOff));
  if (!GuestPtrPlausible(player_meter) || r3.u32 != player_meter) {
    ++g_inv_gate_miss;
    if (!g_inv_logged_gate_miss) {
      g_inv_logged_gate_miss = true;
      REXLOG_INFO(
          "afro invuln: death entry meter is not player+0x7D4 r3=0x{:08X} "
          "player_meter=0x{:08X} player=0x{:08X}",
          r3.u32, player_meter, player);
    }
    return;
  }
  ++g_inv_gate_match;
  const float meter0 = ReadGuestFloat(r3.u32 + kMeterHp0);
  if (!std::isfinite(meter0) || meter0 <= 0.f) {
    ++g_inv_lethal;
    if (!g_inv_logged_lethal) {
      g_inv_logged_lethal = true;
      REXLOG_INFO(
          "afro invuln: player meter+0 is {:.1f} at the death entry "
          "(<=0 is the lethal compare) player=0x{:08X}",
          meter0, player);
    }
  }
  GuardPlayerMeter(r3.u32, player);
}

void AfroDtStore1C04a(PPCRegister& r28, PPCRegister& f0) { PatchPrimaryDtStore(r28, f0, 7172); }
void AfroDtStore1C04b(PPCRegister& r28, PPCRegister& f31) { PatchPrimaryDtStore(r28, f31, 7172); }
void AfroDtStore1C04c(PPCRegister& r28, PPCRegister& f31) { PatchPrimaryDtStore(r28, f31, 7172); }

// Constant-pool 1/30 (0x820EB774) -> 1/60 when Experimental. Anim/cutscene timelines
// that step once per present (or convert frame counts to seconds) need this; leaving
// them at 1/30 with interval=1 made the intro unwatchably fast after 92f93d1.
void AfroDt30Rewrite(PPCRegister& f) {
  if (!Experimental60()) return;
  f.f64 = double(kDt60);
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("afro dt: 1/30 const load -> 1/60 (anim/cutscene present-step scale)");
  }
}
void AfroDt30_f0(PPCRegister& f0) { AfroDt30Rewrite(f0); }
void AfroDt30_f1(PPCRegister& f1) { AfroDt30Rewrite(f1); }
void AfroDt30_f9(PPCRegister& f9) { AfroDt30Rewrite(f9); }
void AfroDt30_f31(PPCRegister& f31) { AfroDt30Rewrite(f31); }


// sub_82676300 timeType converter (anim/NTG property eval).
// mode0: *=1/30 (frameIn30hz), mode1: *=1/60, mode2: raw, 3/4: other.
// 7fa0f73 already rewrites mode0's 1/30 load; if intro is STILL fast with that,
// cutscene advance is likely pure frame-index / per-present (not this converter).
void AfroCineTimeConvertHook(PPCRegister& r11, PPCRegister& f31) {
  if (!Experimental60()) return;
  const uint32_t mode = r11.u32;
  if (mode <= 4)
    g_cine_mode_counts[mode].fetch_add(1, std::memory_order_relaxed);
  static std::atomic<int> logged_mask{0};
  const int bit = 1 << (mode <= 4 ? int(mode) : 4);
  int prev = logged_mask.load(std::memory_order_relaxed);
  while ((prev & bit) == 0) {
    if (logged_mask.compare_exchange_weak(prev, prev | bit, std::memory_order_relaxed)) {
      static const char* kModeName[] = {
          "*1/30(frameIn30hz)", "*1/60", "raw", "lookup", "div", "other"};
      const char* name = mode <= 4 ? kModeName[mode] : kModeName[5];
      REXLOG_INFO(
          "afro cine: timeConvert mode={} ({}) in={:.6f} (once-log; counts in afro fps)",
          mode, name, float(f31.f64));
      break;
    }
  }
}

void AfroSwapHook(PPCRegister& r31) {
  TickModeOnce();
  FxTick();
  PlayerInvulnTick();
  ForceDevicePresentInterval(r31.u32);
  ForceExperimentalPrimaryDt();
  const int cap = REXCVAR_GET(afro_fps_cap);
  if (cap > 0) {
    const auto period = std::chrono::duration_cast<clk::duration>(
        std::chrono::duration<double>(1.0 / cap));
    auto now = clk::now();
    if (g_next.time_since_epoch().count() == 0 || now - g_next > period * 4) g_next = now;
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
  if (secs >= 2.0) {
    g_fps = (g_win_frames - 1) / secs;
    if (REXCVAR_GET(afro_fps_log)) {
      const float dt = ReadGuestFloat(kDtAddr);
      const float dt_m = ReadGuestFloat(kDtMeasuredAddr);
      const float dt_s = ReadGuestFloat(kDtScaleAddr);
      const int seen = g_seen_interval.load(std::memory_order_relaxed);
      const uint64_t m0 = g_cine_mode_counts[0].exchange(0, std::memory_order_relaxed);
      const uint64_t m1 = g_cine_mode_counts[1].exchange(0, std::memory_order_relaxed);
      const uint64_t m2 = g_cine_mode_counts[2].exchange(0, std::memory_order_relaxed);
      const uint64_t m3 = g_cine_mode_counts[3].exchange(0, std::memory_order_relaxed);
      const uint64_t m4 = g_cine_mode_counts[4].exchange(0, std::memory_order_relaxed);
      REXLOG_INFO(
          "afro fps: guest {:.1f} FPS  dt@0x828B1C04={:.6f}s  dt@0x828B1BF0={:.6f}s  "
          "dt@0x828B1C08={:.6f}s  feed={}  present_interval cvar={} seen={}  "
          "tick_mode={}  host_cap={}  cine_modes(2s) 0={} 1={} 2={} 3={} 4={}{}",
          g_fps, dt, dt_m, dt_s, Experimental60() ? "1/60@1C04+const" : "retail",
          REXCVAR_GET(afro_present_interval), seen < 0 ? -1 : seen, TickModeName(), cap,
          m0, m1, m2, m3, m4, InvulnFpsSuffix());
    }
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

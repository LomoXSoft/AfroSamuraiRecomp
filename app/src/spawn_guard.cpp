// Spawn diagnostics for the game's spawn script nodes (EntSpawnManager and
// EntSpawnPoint). Hooks in afro_hooks.toml; addresses are the Europe disc's.
//
// Layout read from the game code (Europe addresses):
//   EntSpawnPoint (vtable 0x820E0A64, ctor sub_826C9D60, 256 bytes)
//     +224 props (192-byte data object): +96 animation string, +108 jumpSequence,
//          +112 jumpSequenceSpeed, +116 jumpSequenceHeight, +120 activeRadius,
//          +124 outerActiveRadius, +128 invertActiveRadius, +129 state,
//          +130 useOnlyOnce, +176 nextDestination port
//     +236 jump delay (s), +244 handle of the spawned character (*handle = 0 when
//          gone), +248 jump pending
//     Spawn sub_826CA410, update/jump sub_826C9F78, eligible sub_826C8DC8
//   EntSpawnManager update sub_826CA638 (r3 = manager + 224, "m" below)
//     m+28 props: +108 groupID, +112 state, +113 useSpawnPointCloseToAfro,
//          +114 waitForEvent
//     m+72 spawned, m+100 kill count used by kill-count waves, m+104 spawn delay,
//     m+108 left to spawn, m+92/96 timers
//   Character: +0x260 position (vfunc 10), +0x280 HP, +680 AI controller
//     (AI+436 handle of the waypoint, AI+440 resumeAfterDisturbed, AI+441 reached,
//     AI+442 following its waypoints)
//   EntWayPoint: +0x70 position, +224 props (+96 destinationRadius)
//   Arrive check sub_827E2D28: |dy| <= 1.5 and the horizontal distance < radius
//   *(0x828E56E8) = Afro (used by the spawn point distance checks)
#include "afro_region.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

REXCVAR_DEFINE_BOOL(afro_spawn_diag, false, "Afro",
                    "Diagnostic: log the spawn managers, spawn points and the characters "
                    "they spawned once a second");
REXCVAR_DEFINE_BOOL(afro_spawn_guard, true, "Afro",
                    "Prologue fix: an enemy that fell off its path to the courtyard fight and "
                    "can no longer reach its waypoint joins the fight (the original game "
                    "soft-locks there)");
REXCVAR_DEFINE_INT32(afro_spawn_guard_s, 10, "Afro",
                     "Prologue fix: seconds an enemy stands still off its path before it joins the fight");

namespace {

using clk = std::chrono::steady_clock;

constexpr uint32_t kAfroEntity = AfroGameAddr(0x828E56E8);
constexpr uint32_t kLevelPath = AfroGameAddr(0x82A13CA0);
// Class checks before following a remembered pointer (MSVC vtables).
constexpr uint32_t kSpawnPointVt = AfroGameAddr(0x820E0A64);  // EntSpawnPoint
constexpr uint32_t kSpawnMgrIfVt = AfroGameAddr(0x820E0A48);  // EntSpawnManager + 224

struct SpawnRec {
  uint32_t sp = 0;
  uint32_t ent = 0;
  clk::time_point t_spawn{};
  // Guard: where the character stood when it last moved, and how long since.
  float ax = 0, ay = 0, az = 0;
  double still_s = 0;
};

std::mutex g_mu;
std::vector<uint32_t> g_mgrs;
std::vector<uint32_t> g_points;
std::vector<SpawnRec> g_spawns;
clk::time_point g_next_log{};
clk::time_point g_last_frame{};
std::string g_level;
uint32_t g_afro = 0;

bool Tracking() { return REXCVAR_GET(afro_spawn_diag) || REXCVAR_GET(afro_spawn_guard); }

// Host pointer for a committed guest address, else null. An object the game
// freed can leave garbage pointers behind; reading an unmapped guest page
// would stop the game, so every read goes through this check.
uint8_t* Host(uint32_t a) {
  auto* mem = rex::system::kernel_memory();
  if (!mem || a < 0x40000000u || a >= 0xFFFF0000u) return nullptr;
  auto* heap = mem->LookupHeap(a);
  rex::memory::HeapAllocationInfo info{};
  if (!heap || !heap->QueryRegionInfo(a & ~0xFFFu, &info) ||
      !(info.state & rex::memory::kMemoryAllocationCommit))
    return nullptr;
  if ((a & 0xFFFu) > 0xFFCu) {  // a 4-byte read that crosses into the next page
    rex::memory::HeapAllocationInfo next{};
    if (!heap->QueryRegionInfo((a + 4) & ~0xFFFu, &next) ||
        !(next.state & rex::memory::kMemoryAllocationCommit))
      return nullptr;
  }
  return mem->TranslateVirtual<uint8_t*>(a);
}
uint32_t U32(uint32_t a) {
  auto* p = Host(a);
  return p ? rex::memory::load_and_swap<uint32_t>(p) : 0;
}
uint8_t U8(uint32_t a) {
  auto* p = Host(a);
  return p ? *p : 0;
}
float F32(uint32_t a) {
  const uint32_t u = U32(a);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

void AddUnique(std::vector<uint32_t>& v, uint32_t x) {
  if (std::find(v.begin(), v.end(), x) == v.end()) v.push_back(x);
}

bool IsSpawnPoint(uint32_t sp) { return U32(sp) == kSpawnPointVt; }

// The character a spawn point holds, or 0 once it is gone.
uint32_t HeldCharacter(uint32_t sp) {
  if (!IsSpawnPoint(sp)) return 0;
  const uint32_t h = U32(sp + 244);
  return h ? U32(h) : 0;
}

std::string LevelPath() {
  // A string object; try each word as a pointer to text.
  for (uint32_t off = 0; off < 16; off += 4) {
    const uint32_t p = U32(kLevelPath + off);
    auto* s = Host(p);
    if (!s) continue;
    std::string out;
    for (int i = 0; i < 96 && s[i]; ++i) {
      if (s[i] < 32 || s[i] > 126) { out.clear(); break; }
      out.push_back(char(s[i]));
    }
    if (out.size() > 3) return out;
  }
  return {};
}

void LogAll() {
  const uint32_t afro = U32(kAfroEntity);
  REXLOG_INFO("afro spawn: level='{}' afro=0x{:08X} pos=({:.1f},{:.1f},{:.1f}) mgrs={} points={} spawns={}",
              LevelPath(), afro, F32(afro + 0x260), F32(afro + 0x264), F32(afro + 0x268),
              g_mgrs.size(), g_points.size(), g_spawns.size());
  for (uint32_t m : g_mgrs) {
    if (U32(m) != kSpawnMgrIfVt) continue;
    const uint32_t pr = U32(m + 28);
    REXLOG_INFO(
        "afro spawn:  mgr 0x{:08X} on={} grp={} close={} wait={} spawned={} c80={} c88={} c48={} "
        "kills={} left={} delay={:.2f} t92={:.1f} t96={:.1f} f180={}{}{}",
        m, U8(pr + 112), U32(pr + 108), U8(pr + 113), U8(pr + 114), U32(m + 72), U32(m + 80),
        U32(m + 88), U32(m + 48), U32(m + 100), int32_t(U32(m + 108)), F32(m + 104), F32(m + 92),
        F32(m + 96), U8(m + 180), U8(m + 181), U8(m + 182));
  }
  const auto now = clk::now();
  for (const SpawnRec& r : g_spawns) {
    const uint32_t pr = U32(r.sp + 224);
    const uint32_t held = HeldCharacter(r.sp);
    const double age = std::chrono::duration<double>(now - r.t_spawn).count();
    if (held) {
      const uint32_t ai = U32(held + 680);
      const uint32_t dh = ai ? U32(ai + 436) : 0;
      const uint32_t wp = dh ? U32(dh) : 0;
      const uint32_t wpp = wp ? U32(wp + 224) : 0;
      REXLOG_INFO(
          "afro spawn:  sp 0x{:08X} ent 0x{:08X} age={:.0f}s pend={} delay={:.2f} st={} jump={} once={} "
          "r={:.1f}/{:.1f} inv={} pos=({:.1f},{:.1f},{:.1f}) hp={:.1f} ai=0x{:08X} follow={} "
          "reached={} resume={} wp=0x{:08X} wait={} auto={} alert={:.1f} dest_r={:.1f} kill={} "
          "invinc={} jump={}",
          r.sp, held, age, U8(r.sp + 248), F32(r.sp + 236), U8(pr + 129), U8(pr + 108),
          U8(pr + 130), F32(pr + 120), F32(pr + 124), U8(pr + 128), F32(held + 0x260),
          F32(held + 0x264), F32(held + 0x268), F32(held + 0x280), ai, ai ? U8(ai + 442) : 0,
          ai ? U8(ai + 441) : 0, ai ? U8(ai + 440) : 0, wp, wpp ? U8(wpp + 112) : 0,
          wpp ? U8(wpp + 113) : 0, wpp ? F32(wpp + 108) : 0.f, wpp ? F32(wpp + 96) : 0.f,
          wpp ? U8(wpp + 114) : 0, wpp ? U8(wpp + 180) : 0, wpp ? U8(wpp + 132) : 0);
    }
  }
}

}  // namespace

// sub_826CA410 (EntSpawnPoint spawn), after the character exists and the jump
// is pending: r31 = spawn point, r30 = character.
void AfroSpawnPointSpawned(PPCRegister& r30, PPCRegister& r31) {
  if (!Tracking()) return;
  std::lock_guard lk(g_mu);
  SpawnRec r;
  r.sp = r31.u32;
  r.ent = r30.u32;
  r.t_spawn = clk::now();
  // A spawn point holds one character at a time: replace its old record.
  g_spawns.erase(std::remove_if(g_spawns.begin(), g_spawns.end(),
                                [&](const SpawnRec& o) { return o.sp == r.sp; }),
                 g_spawns.end());
  g_spawns.push_back(r);
  if (!REXCVAR_GET(afro_spawn_diag)) return;
  const uint32_t pr = U32(r.sp + 224);
  REXLOG_INFO("afro spawn: spawned ent 0x{:08X} at sp 0x{:08X} pos=({:.1f},{:.1f},{:.1f}) jump={} "
              "delay={:.2f} r={:.1f}/{:.1f} inv={}",
              r.ent, r.sp, F32(r.ent + 0x260), F32(r.ent + 0x264), F32(r.ent + 0x268),
              U8(pr + 108), F32(r.sp + 236), F32(pr + 120), F32(pr + 124), U8(pr + 128));
}

// sub_826CA638 (EntSpawnManager update) after its prologue: r31 = manager + 224.
void AfroSpawnMgrTick(PPCRegister& r31) {
  if (!REXCVAR_GET(afro_spawn_diag)) return;
  std::lock_guard lk(g_mu);
  AddUnique(g_mgrs, r31.u32);
}

// sub_826C9F78 (EntSpawnPoint update) after its prologue: r26 = spawn point.
void AfroSpawnPointTick(PPCRegister& r26) {
  if (!REXCVAR_GET(afro_spawn_diag)) return;
  std::lock_guard lk(g_mu);
  AddUnique(g_points, r26.u32);
}

namespace {

// Height the arrive check (sub_827E2D28) allows between a character and its
// waypoint.
constexpr float kArriveDy = 1.5f;
// Moving less than this does not count as moving (idle animations, pushes).
constexpr float kStillDist = 0.25f;

// The original game's soft-lock in the prologue's courtyard fight: an enemy
// that should walk along a ledge to its waypoint and jump down from there
// ends up below the ledge instead (Afro standing where it lands). It keeps
// following a waypoint it can never reach, so it neither comes down nor
// fights, and the wave never ends. Such an enemy is told to stop following
// its waypoints (AI+442 = 0), the state it is in after reaching its last one,
// and fights normally.
void GuardTick(double dt) {
  if (g_level.find("combatintro") == std::string::npos) return;
  for (SpawnRec& r : g_spawns) {
    const uint32_t ch = HeldCharacter(r.sp);
    const uint32_t ai = ch ? U32(ch + 680) : 0;
    const uint32_t h = ai ? U32(ai + 436) : 0;
    const uint32_t wp = h ? U32(h) : 0;
    if (!wp || !U8(ai + 442) || U8(ai + 441) || !(F32(ch + 0x280) > 0.f)) {
      r.still_s = 0;
      continue;
    }
    const float x = F32(ch + 0x260), y = F32(ch + 0x264), z = F32(ch + 0x268);
    const float dy = y - F32(wp + 0x74);
    const float moved = std::hypot(x - r.ax, y - r.ay, z - r.az);
    if (std::fabs(dy) <= kArriveDy || moved > kStillDist) {
      r.ax = x, r.ay = y, r.az = z;
      r.still_s = 0;
      continue;
    }
    r.still_s += dt;
    if (r.still_s < REXCVAR_GET(afro_spawn_guard_s)) continue;
    r.still_s = 0;
    if (auto* p = Host(ai + 442)) *p = 0;
    REXLOG_INFO("afro spawn guard: enemy 0x{:08X} was {:.1f} off the height of its waypoint 0x{:08X} for {} s; "
                "it joins the fight",
                ch, std::fabs(dy), wp, REXCVAR_GET(afro_spawn_guard_s));
  }
}

}  // namespace

namespace afro {
// Once per guest frame (AfroSwapHook).
void SpawnGuardFrame() {
  if (!Tracking()) return;
  const auto now = clk::now();
  double dt = g_last_frame.time_since_epoch().count()
                  ? std::chrono::duration<double>(now - g_last_frame).count()
                  : 0.0;
  g_last_frame = now;
  dt = std::clamp(dt, 0.0, 0.1);  // a menu or a load does not count as waiting
  std::lock_guard lk(g_mu);
  // Records point into the level's objects: forget them when the level changes.
  // A new Afro object means a load (new area, checkpoint, restart).
  std::string level = LevelPath();
  const uint32_t afro = U32(kAfroEntity);
  if (level != g_level || afro != g_afro) {
    if (REXCVAR_GET(afro_spawn_diag))
      REXLOG_INFO("afro spawn: level '{}' -> '{}', afro 0x{:08X} -> 0x{:08X}, {} records cleared",
                  g_level, level, g_afro, afro, g_spawns.size());
    g_level = std::move(level);
    g_afro = afro;
    g_mgrs.clear();
    g_points.clear();
    g_spawns.clear();
  }
  if (REXCVAR_GET(afro_spawn_guard)) GuardTick(dt);
  if (!REXCVAR_GET(afro_spawn_diag) || now < g_next_log) return;
  g_next_log = now + std::chrono::seconds(1);
  LogAll();
}
}  // namespace afro

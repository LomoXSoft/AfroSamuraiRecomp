#pragma once
// Pure player-HP policy for afro_player_invuln.
// No guest memory access. Callers apply it only when the cvar is on and the
// store base is the player singleton. The ceiling is the live max field
// (player+0x284). This header has no full-HP constant.

#include <cmath>

namespace afro_invuln {

constexpr float kHpEps = 0.01f;

struct Hold {
  float value = -1.f;  // < 0: no legitimate char HP seen this session
};

inline bool FiniteHp(float v) {
  return std::isfinite(v) && v >= 0.f && v < 10000.f;
}

// Field logs show a player max on a large scale (100). Reject 0, negatives,
// NaN, and absurd values so a garbage word cannot become the ceiling.
inline bool MaxFieldSane(float hp_max) {
  return std::isfinite(hp_max) && hp_max >= 10.f && hp_max < 10000.f;
}

// True when hp_max may cap `reference`. A live value far above the field
// (beyond the known modest overshoot, about +20 and within 1.25x) contradicts
// the HP being protected, so the field is not used as a ceiling.
inline bool MaxAgrees(float hp_max, float reference) {
  if (!MaxFieldSane(hp_max) || !std::isfinite(reference)) return false;
  if (reference <= hp_max + kHpEps) return true;
  if (reference > 0.f && reference <= hp_max + 20.f && reference <= hp_max * 1.25f)
    return true;
  return false;
}

inline float CapAtMax(float value, float hp_max) {
  if (!std::isfinite(value)) return value;
  if (value > hp_max && MaxAgrees(hp_max, value)) return hp_max;
  return value;
}

inline void NoteCharHp(Hold& state, float cur, float hp_max) {
  // Pull a modest over-max hold back down. Do not follow a max that
  // contradicts the hold; that would reintroduce the damage we are blocking.
  if (state.value >= 0.f && state.value > hp_max && MaxAgrees(hp_max, state.value))
    state.value = hp_max;
  if (!FiniteHp(cur) || cur <= 0.f) return;
  const float sample = CapAtMax(cur, hp_max);
  if (state.value < 0.f || sample > state.value) state.value = sample;
}

enum class StoreAction { kLeave, kReplace };

struct StoreDecision {
  StoreAction action = StoreAction::kLeave;
  float value = 0.f;
};

// incoming: f0 about to be stored at entity+0x280.
// cur: value already at entity+0x280. hp_max: entity+0x284.
// The meter current is intentionally not an input.
inline StoreDecision DecideStore(Hold& state, float incoming, float cur, float hp_max) {
  NoteCharHp(state, cur, hp_max);
  StoreDecision leave;
  leave.action = StoreAction::kLeave;
  leave.value = incoming;

  if (!(state.value >= 0.f)) {
    if (FiniteHp(incoming) && incoming > 0.f)
      state.value = CapAtMax(incoming, hp_max);
    if (!(state.value >= 0.f)) return leave;
  }

  // Heals may raise the hold, but not past a sane max. An over-max incoming
  // must not become the new hold.
  if (std::isfinite(incoming) && incoming > state.value &&
      (!MaxFieldSane(hp_max) || incoming <= hp_max + kHpEps)) {
    state.value = incoming;
  }

  const float floor = CapAtMax(state.value, hp_max);

  if (std::isfinite(incoming) && MaxFieldSane(hp_max) && MaxAgrees(hp_max, floor) &&
      incoming > hp_max + kHpEps) {
    StoreDecision d;
    d.action = StoreAction::kReplace;
    d.value = hp_max;
    state.value = hp_max;
    return d;
  }

  if (!std::isfinite(incoming) || incoming + kHpEps < floor) {
    StoreDecision d;
    d.action = StoreAction::kReplace;
    d.value = floor;
    return d;
  }
  return leave;
}

enum class FallbackAction { kNone, kWriteChar };

struct FallbackDecision {
  FallbackAction action = FallbackAction::kNone;
  float value = 0.f;
};

// meter+0 repair for the death predicate in sub_82663180 (fcmpu f0 vs 0, ble).
// char_hp is the only sample that may raise the hold. meter0 is not.
// f1 is not an input: it is not the branch predicate.
struct MeterDecision {
  bool write = false;
  float value = 0.f;
  bool lethal = false;
};

inline MeterDecision DecideMeter(Hold& state, float meter0, float char_hp, float hp_max) {
  NoteCharHp(state, char_hp, hp_max);
  MeterDecision d;
  if (!(state.value > 0.f)) return d;
  const float floor = CapAtMax(state.value, hp_max);
  if (!(floor > 0.f)) return d;
  const bool lethal = !std::isfinite(meter0) || meter0 <= 0.f;
  const bool dropped = !std::isfinite(meter0) || meter0 + kHpEps < floor;
  if (!lethal && !dropped) return d;
  d.write = true;
  d.value = floor;
  d.lethal = lethal;
  return d;
}

// Post-frame repair of player+0x280 only. Does not target the meter and does
// not write player+0x284.
inline FallbackDecision DecideFallback(Hold& state, float cur, float hp_max) {
  NoteCharHp(state, cur, hp_max);
  FallbackDecision none;
  if (!(state.value >= 0.f) || !FiniteHp(cur)) return none;
  const float floor = CapAtMax(state.value, hp_max);
  if (MaxFieldSane(hp_max) && MaxAgrees(hp_max, floor) && cur > hp_max + kHpEps) {
    FallbackDecision d;
    d.action = FallbackAction::kWriteChar;
    d.value = hp_max;
    state.value = hp_max;
    return d;
  }
  if (cur + kHpEps < floor) {
    FallbackDecision d;
    d.action = FallbackAction::kWriteChar;
    d.value = floor;
    return d;
  }
  return none;
}

}  // namespace afro_invuln

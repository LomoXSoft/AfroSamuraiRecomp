#include "../app/src/player_hp_invuln.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

int g_fails = 0;

void Expect(bool cond, const char* msg) {
  if (cond) return;
  std::cerr << "FAIL: " << msg << "\n";
  ++g_fails;
}

void ExpectNear(float got, float want, const char* msg) {
  if (std::isfinite(got) && std::fabs(got - want) < 0.001f) return;
  std::cerr << "FAIL: " << msg << " got " << got << " want " << want << "\n";
  ++g_fails;
}

// Mirrors the hook guard: disabled and non-player stores do not call the policy.
float ApplyIfPlayer(bool enabled, bool is_player, afro_invuln::Hold& hold, float incoming,
                    float cur, float hp_max) {
  if (!enabled || !is_player) return incoming;
  const afro_invuln::StoreDecision d =
      afro_invuln::DecideStore(hold, incoming, cur, hp_max);
  if (d.action == afro_invuln::StoreAction::kReplace) return d.value;
  return incoming;
}

}  // namespace

int main() {
  using afro_invuln::DecideFallback;
  using afro_invuln::DecideStore;
  using afro_invuln::FallbackAction;
  using afro_invuln::Hold;
  using afro_invuln::StoreAction;

  // Disabled: enemy or player, the value about to be stored is unchanged.
  {
    Hold h;
    ExpectNear(ApplyIfPlayer(false, true, h, 40.f, 100.f, 100.f), 40.f, "disabled leaves damage");
    Expect(!(h.value >= 0.f), "disabled does not learn a hold");
  }
  // Enabled, other entity: unchanged even if that entity's HP is falling.
  {
    Hold h;
    ExpectNear(ApplyIfPlayer(true, false, h, 10.f, 80.f, 80.f), 10.f, "enemy store unchanged");
    Expect(!(h.value >= 0.f), "enemy store does not learn the player hold");
  }

  // Enabled player: a hit must not lower char HP, and must not restore to 110.
  {
    Hold h;
    float cur = 100.f;
    const float stored = ApplyIfPlayer(true, true, h, 90.f, cur, 100.f);
    ExpectNear(stored, 100.f, "first hit keeps 100");
    ExpectNear(h.value, 100.f, "hold is 100");
    cur = stored;
    for (int i = 0; i < 597; ++i) {
      const float incoming = cur - 10.f;
      const float next = ApplyIfPlayer(true, true, h, incoming, cur, 100.f);
      ExpectNear(next, 100.f, "repeated hit stays at max");
      if (next > 100.f + 0.001f) Expect(false, "restore exceeded hp_max");
      cur = next;
    }
  }

  // Stuck 110 must not become the hold or the stored value when max is 100.
  {
    Hold h;
    const auto d = DecideStore(h, 110.f, 100.f, 100.f);
    Expect(d.action == StoreAction::kReplace, "110 store is replaced");
    ExpectNear(d.value, 100.f, "110 store capped at hp_max");
    ExpectNear(h.value, 100.f, "110 does not become the hold");
  }
  {
    Hold h;
    const auto d = DecideStore(h, 90.f, 110.f, 100.f);
    Expect(d.action == StoreAction::kReplace, "damage from a 110 char value is replaced");
    ExpectNear(d.value, 100.f, "restore from bogus 110 char is hp_max");
    Expect(!(d.value > 100.f + 0.001f), "no 110 restore");
  }
  {
    Hold h;
    const auto d = DecideFallback(h, 110.f, 100.f);
    Expect(d.action == FallbackAction::kWriteChar, "fallback caps 110 char HP");
    ExpectNear(d.value, 100.f, "fallback write is hp_max");
  }

  // Meter is not an input. A 110 meter beside char 100 / max 100 does not raise
  // the fallback target: the caller passes char HP only.
  {
    Hold h;
    const auto primed = DecideStore(h, 100.f, 100.f, 100.f);
    Expect(primed.action == StoreAction::kLeave, "full HP store stays");
    const auto d = DecideFallback(h, 100.f, 100.f);
    Expect(d.action == FallbackAction::kNone, "no fallback when char is at the hold");
    ExpectNear(h.value, 100.f, "hold ignores an unseen meter of 110");
  }

  // Char steps down after a full-HP sample and the mid-asm hook did not stick.
  // Fallback restores the hold (capped at hp_max), never a meter value of 110.
  {
    Hold h;
    const auto primed = DecideFallback(h, 100.f, 100.f);
    Expect(primed.action == FallbackAction::kNone, "full HP does not need a fallback write");
    ExpectNear(h.value, 100.f, "fallback hold primed at char HP");
    int writes = 0;
    for (int hp = 90; hp >= 0; hp -= 10) {
      const auto d = DecideFallback(h, static_cast<float>(hp), 100.f);
      Expect(d.action == FallbackAction::kWriteChar, "fallback restores a dropped char HP");
      ExpectNear(d.value, 100.f, "fallback target is the capped hold");
      ++writes;
    }
    Expect(writes == 10, "one fallback per dropped step");
  }

  // Heal up to max is allowed. A later hit cannot undo it.
  {
    Hold h;
    float cur = ApplyIfPlayer(true, true, h, 80.f, 80.f, 100.f);
    ExpectNear(cur, 80.f, "establishing store kept");
    cur = ApplyIfPlayer(true, true, h, 100.f, cur, 100.f);
    ExpectNear(cur, 100.f, "heal to max kept");
    const float hit = ApplyIfPlayer(true, true, h, 70.f, cur, 100.f);
    ExpectNear(hit, 100.f, "hit after heal stays at healed hold");
  }

  // A store above max is cut to max, including a large overshoot.
  {
    Hold h;
    const float stored = ApplyIfPlayer(true, true, h, 150.f, 100.f, 100.f);
    ExpectNear(stored, 100.f, "150 store capped at hp_max");
    ExpectNear(h.value, 100.f, "150 does not become the hold");
  }

  // Garbage max far below the live HP must not collapse the player to that max.
  {
    Hold h;
    const float stored = ApplyIfPlayer(true, true, h, 40.f, 100.f, 1.f);
    ExpectNear(stored, 100.f, "garbage max does not cap a real HP");
  }

  // NaN incoming is replaced with the hold once a hold exists.
  {
    Hold h;
    ApplyIfPlayer(true, true, h, 100.f, 100.f, 100.f);
    const float stored = ApplyIfPlayer(true, true, h, std::nanf(""), 100.f, 100.f);
    ExpectNear(stored, 100.f, "NaN store replaced with hold");
  }

  // No positive HP seen yet: do not invent a restore from a zero store.
  {
    Hold h;
    const auto d = DecideStore(h, 0.f, 0.f, 100.f);
    Expect(d.action == StoreAction::kLeave, "zero store with no hold is left");
    Expect(!(h.value >= 0.f), "zero does not seed the hold");
  }

  // Session clear drops the hold. The next sample is whatever HP is live then.
  {
    Hold h;
    ApplyIfPlayer(true, true, h, 100.f, 100.f, 100.f);
    h = {};
    const float stored = ApplyIfPlayer(true, true, h, 40.f, 40.f, 100.f);
    ExpectNear(stored, 40.f, "after reset the live HP is kept");
    ExpectNear(h.value, 40.f, "hold restarts from the new sample");
    const float hit = ApplyIfPlayer(true, true, h, 30.f, stored, 100.f);
    ExpectNear(hit, 40.f, "after reset, damage is blocked at the new hold");
  }

  // Max that contradicts the hold (100 held, max smashed to 50) is not a ceiling.
  {
    Hold h;
    ApplyIfPlayer(true, true, h, 100.f, 100.f, 100.f);
    const float stored = ApplyIfPlayer(true, true, h, 40.f, 100.f, 50.f);
    ExpectNear(stored, 100.f, "contradictory lower max does not cut the hold");
  }

  // Meter+0 is the death predicate. f1 is not an input. A zero meter restores
  // to the char hold, capped at hp_max, and never to an unseen 110.
  {
    Hold h;
    const auto d = afro_invuln::DecideMeter(h, 0.f, 100.f, 100.f);
    Expect(d.write, "meter 0 is repaired");
    Expect(d.lethal, "meter 0 is the lethal predicate");
    ExpectNear(d.value, 100.f, "meter 0 restores to char hold");
    ExpectNear(h.value, 100.f, "meter 0 does not change the hold");
  }
  {
    Hold h;
    const auto d = afro_invuln::DecideMeter(h, 10.f, 100.f, 100.f);
    Expect(d.write, "meter below the hold is repaired before it can reach 0");
    Expect(!d.lethal, "positive meter is not the lethal predicate");
    ExpectNear(d.value, 100.f, "dropped meter restores to the hold");
  }
  {
    Hold h;
    const auto d = afro_invuln::DecideMeter(h, 100.f, 100.f, 100.f);
    Expect(!d.write, "meter already at the hold is left alone");
  }
  {
    Hold h;
    const auto d = afro_invuln::DecideMeter(h, 110.f, 100.f, 100.f);
    Expect(!d.write, "meter above the hold is not rewritten to 110 or cut here");
    ExpectNear(h.value, 100.f, "a 110 meter does not become the hold");
  }
  {
    Hold h;
    const auto d = afro_invuln::DecideMeter(h, 0.f, 0.f, 100.f);
    Expect(!d.write, "no positive char sample means no invented meter restore");
  }
  {
    Hold h;
    ApplyIfPlayer(true, true, h, 100.f, 100.f, 100.f);
    const auto d = afro_invuln::DecideMeter(h, std::nanf(""), 100.f, 100.f);
    Expect(d.write && d.lethal, "NaN meter is lethal");
    ExpectNear(d.value, 100.f, "NaN meter restores to the hold");
  }

  if (g_fails) {
    std::cerr << g_fails << " failure(s)\n";
    return 1;
  }
  std::cout << "player hp policy: all checks passed\n";
  return 0;
}

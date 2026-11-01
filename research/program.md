# Ironclad autoresearch program

Modelled on karpathy/autoresearch (`program.md`): the human writes this file,
the agent iterates on ONE editable surface, a FROZEN harness scores every
attempt, keep only strict improvements, log everything, never pause to ask.

## Goal

Maximise **Ironclad win rate** in `sts_lightspeed` with an agent that uses
**no hidden information** (no peeking at RNG, draw order, future rewards).
"Win" = beat the Act 3 boss (lightspeed has no Act 4/Heart without keys).

- **North-star metric:** A0 win rate on the `test` split (500 seeds), with a
  95% Wilson CI. Every claim is judged on that number, nowhere else.
- **Loop metric:** paired Δscore on `val` (300 seeds) vs current best,
  score = floor/51 + win.

### Milestones (concrete, in order)

| # | Target | Action when hit |
|---|---|---|
| M0 | ✅ leak-free agent beats the cheating lightspeed reference | done: val 20.3% vs scum ~2% (dev) |
| M1 | val win ≥ 25% | run `test` once (test budget: max 5 runs total) |
| M2 | val win ≥ 30% | run `test`; re-run cheating-combat diagnostic |
| M3 | test win ≥ 35% at A0 | start an A10 track: new `best` re-baselined at `--asc 10` |
| M4 | test win ≥ 50% at A0 | move the main ladder to A20 (double boss) |

### Current state (update after every batch)

- **FINAL (loop stopped: 2 consecutive batches without a keep).** Best: `hallpot0`
  — val 35.7%; **TEST 169/500 = 33.8%, 95% CI [29.8%, 38.1%]** (test runs used: 4/5).
  Baselines on test: fair0 0.4%, cheating lightspeed agent 0.8%. See `research/REPORT.md`.
- Milestones: M1 ✅ (test 33.8% ≥ 25%), M2 ✅ on test (≥ 30%), M3 ✗ (35% inside CI, not reached).
- Keep chain: legacy policy 3.0% → END_TURN rollout w 0.2 (10%) → 0.05 (14%)
  → [Runic Dome guardrail: 11%] → events (13.7%) → KB boss relics (18.3%)
  → AoE bonus (20.3%) → scaling bonus (23.0%) → map: prefer events, w_event 1.5 (30.0%) → near-miss combo (32.0%) → no hallway potions in search (35.7%).
- Diagnostic: lightspeed's CHEATING combat + our policy = 9.6% — our leak-free
  search already beats it, so the "combat ceiling" probe is uninformative.
- Learned: random rollouts beat every "smart" rollout bias tried (play-order,
  block-aware, attack-bias). Deck-building additions (AoE, scaling) work;
  block bonus, card thresholds, Neow ranking, elite tweaks ≈ neutral/negative.

### Phase 2 (2026-11-01): simulator holes filled instead of routed around

Core-sim fixes (all in `research/patches/sts_lightspeed.patch`, each marked `// FIX`):
- `BattleContext::player.cc` was never initialised → Entropic Brew and Dead Branch drew
  from a garbage class pool (the "Ambrosia crash"). Root cause of the earlier Entropic ban.
- Blood Potion heal inverted in combat (40% base / 20% Bark → 20% / 40%).
- Iron Wave applied Dexterity/Frail twice. Dark Shackles *added* 9 Strength and applied
  Shackled only *with* Artifact (both inverted). Disarm+ was −2 (→ −3). Impatience always drew.
- Smoke Bomb was a no-op → now escapes non-boss fights with no rewards (`afterEscape`).
  Toy Ornithopter had no effect → heals 5 per potion (in and out of combat).
- Events: Designer In-Spire switch fall-through (opt 3 → also opt 4/5), "Adjustments" variant
  free; Mushrooms "Eat" gave +99 gold (→ heal 25% + Parasite); Mysterious Sphere "Leave" gave
  +99 gold (→ nothing); Mausoleum gave no relic; Woman in Blue was free (→ 20/30/40 gold,
  validity gated); Scrap Ooze damage didn't escalate; Colosseum was a stub and disabled →
  implemented (slavers → leave / Taskmaster+Nob for 100g + rare + uncommon relic) and enabled.
- `Monster::lastMoveWasRolled` flag (rollMove vs scripted setMove) — needed for honest Dome.

Agent fidelity:
- Entropic Brew and Runic Dome un-banned. Under Dome the search hides intents: the
  observation hash drops `moveHistory[0]`, and determinisation re-samples the pending move
  **only if it came from an RNG roll** (scripted sequences like the Automaton's are
  predictable anyway; re-rolling them double-spawned orbs → the Phase-2 crash/hang bug).
- Known top-of-draw-pile cards (Headbutt/Warcry) are kept fixed in determinisation.
- Smoke Bomb unusable vs bosses (real rule); escape costs `escape_pen` (default 30) in value.
- Leak test gained `--leak_dome`. Trace build (`-DFAIR_TRACE`) asserts the alive-count
  invariant after every executed action.

Still deliberately disabled (documented, not hidden): Match & Keep (separate action space,
rare), Prismatic Shard (lightspeed implements only Ironclad + colorless cards, so an
off-colour deck would hit "unimplemented card").

Re-baseline after the player.cc fix alone: `rb2` val 34.7% (n=297) vs 35.7% before — neutral.
**Re-baseline on the fully fixed sim: `rb3` val 38.0%, floor 40.5, score 1.174 (n=300, 0 crashes)**
— +0.043 vs rb2 (P=0.89): correct event outcomes / Dome allowed were net positive. rb3 is the
reference for all later paired comparisons.

Throughput work (bit-identical games, verified on 4 seeds): allocation-free action enumerator
(~13%) + lightspeed's fixed-size card piles (`sts_card_manager_use_fixed_list`, MAX_GROUP_SIZE
64→128) (~10% more) ≈ 1.3× faster. Protocol note: the compute budget is defined as the
wall-clock of the original implementation at sims=500; verified speedups may be converted
into proportionally more simulations (logged as such, timed on the same seeds, idle machine).

Compute accounting finding: `budget_actions=7500` (equal *total* actions, no cutoff) lost
−0.065 (P=0.03) vs equal *sims*: a fixed per-decision action budget starves the long, complex
fights that matter most. Equal-sims stays the protocol; cutoff variants are compared at
sims scaled by their measured cost-per-simulation ratio instead.

Rollout cutoff + heuristic leaf: `bud_rt3` −0.285 (P=0.00). A learned leaf value would have
to beat an *unbiased* terminal rollout and be trained on search-visited (random-play-degraded)
states, i.e. an expert-iteration loop — shelved as out of scope for this session; the
evidence and the design (features at turn start, ~130 dims incl. encounter one-hot, target =
search value at battle end, MLP 130→64→1 exported as a .inc) are recorded here for a follow-up.

### Idea backlog (top = next; strike through when tried, log in results.tsv)

1. ~~Diagnostic (never a result): cheating combat + best out-of-combat policy →~~ (tried)
   measures how much a better combat search could still gain vs strategy work.
2. ~~`neow_pol=1` (Neow ranking; BOSS_RELIC excluded by guardrail).~~ (tried)
3. ~~Elites more aggressive: `elite_hp=0.45`; `w_elite_hi=3`.~~ (tried)
4. ~~Deck needs: `scaling_bonus=1.5`; `block_bonus=1.5`.~~ (tried)
5. `aoe_bonus=3` (if aoe4 fails); `card_size_pen` 0 / 0.2; `shop_card_thresh=6`.
6. Rest: `rest_heal_frac` 0.4 / 0.6. Potions: `potion_v` 2 / 8. Value: `hp_w=2`.
7. ~~Rollout: block-aware heuristic rollout (code): play block when incoming~~ (tried)
   intent damage > block, else attacks. Then `end_turn_w=0.02`.
8. Upgrade priority beyond KB tier (Bash, key powers first).
10. scaling_bonus 3 / scaling_want 3; upgrade scaling cards first.
11. card_size_pen 0 / 0.2; shop_card_thresh 6; rest_heal_frac 0.4/0.6; potion_v 2/8; hp_w 2.
9. Budget scaling `sims=1000` — informational only, never "keep" (compute rule).

### Resume procedure (after context loss / new session)

1. Read this file, `research/results.tsv`, `research/best_name.txt`.
2. `pgrep -x fair_eval` — if evals are running, wait for them; never start a 2nd batch.
3. Rebuild: `research/setup.sh` (or `make -C /home/user/sts_lightspeed/build fair_eval`)
   and run the leak test: `fair_eval --leaktest --seed_start 100000 --count 3`.
4. Take the next backlog items, run them with `research/exp.sh` (auto keep/discard),
   ≤ 6 per batch, in the background.
5. After each batch: update *Current state*, strike backlog items,
   commit + push `autoresearch/ironclad`.
6. When a milestone is hit, run `SPLIT=test` once for the best agent and record it.

## Why the built-in agent does not count

`ScumSearchAgent2` copies the live `BattleContext` (including all RNG state and
the draw-pile order) and replays the best action sequence it found. That's
save-scumming against a known future. It's used here only as a **cheating
upper reference**, never as a result.

## Editable surface (agent may change)

- `research/agent/` — Python policy (map, rewards, shop, rest, events, boss
  relics) and combat-search *parameters/eval functions*.
- `research/cpp/` — the leak-free combat searcher (C++). Any change here must
  pass the leak test before an eval counts.

## Frozen (agent must NOT change)

- `research/eval/` — the harness, seed lists, scoring, leak test.
- `sts_lightspeed` core game logic (src/game, src/combat). A bug fix there
  requires a separate commit with a reason, and invalidates all prior results.
- Compute budget per eval (search simulations per decision, wall clock).

## Eval protocol (frozen)

- Seeds: `dev` = 100 seeds (screening), `val` = 300 seeds (keep/discard gate),
  `test` = 500 seeds (touched only at milestones, max ~5 times total).
- Paired comparison: candidate vs current best on the **same seeds**.
- Primary metric: win rate. Screening metric: mean floor reached
  (lower variance). Score = mean over seeds of `floor/55 + win`.
- Keep rule: candidate score > best score on `val` AND paired bootstrap
  P(improvement) ≥ 0.9. Otherwise discard (git reset).
- Leak test: the action returned for a state must be identical when hidden
  state (draw-pile order, all RNG streams) is re-randomised, with the agent's
  own search seed held fixed. Any failure → result invalid.
- Fixed compute: same simulation budget per combat decision for candidate and
  baseline unless the experiment is explicitly "budget scaling" (logged as such).

## Loop

1. Pick ONE idea (smallest change that tests a hypothesis). Commit.
2. Run `dev` eval. If clearly worse (score drop > 2 SE), discard.
3. Run `val` eval, paired against best. Apply keep rule.
4. Append a row to `research/results.tsv`:
   `commit  split  n  winrate  mean_floor  score  p_improve  status  description`
5. Keep → new best. Discard → `git reset --hard` to best.
6. Crash/timeout (> 2× expected wall clock) → simple fix attempt, else discard.
7. Never stop to ask. Out of ideas → re-read `results.tsv` failures, combine
   near-misses, try more radical changes.

## Stop condition

Stop when the backlog is empty AND the last 5 idea *families* all failed on
`val`, or the day's time runs out. Then run `test` once on the best agent and
write `research/REPORT.md`.

## Known statistical caveat

With a P ≥ 0.9 keep rule over dozens of trials, some keeps are false
positives (winner's curse on `val`). That's why the north star is `test`,
which the loop never optimises against.

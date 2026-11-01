# Ironclad autoresearch program

## Month program (started 2026-11-01) — what the agent is, and where it is going

**What it is today (phase 1–2): not RL.** Combat = online MCTS with uniformly random rollouts
and a hand-written terminal value; out-of-combat = hand-written parametric rules seeded by the
KB tier lists; the improvement loop = hypothesis → frozen eval → paired keep/discard.
Nothing is learned from data. Test (500 seeds, A0): 33.8% before the sim fixes; val now 38.0%.

**North star for the month:** Ironclad **A20 Heart** win rate on held-out seeds (the Epoch
SpireClimb target; best public LLM ≈ 20%, strong humans 50–95%). Intermediate: A0 ≥ 50%,
A10 ≥ 35%, A20 (no Heart) ≥ 25%, A20 Heart ≥ 15%. Every number: 500 seeds, 95% Wilson CI,
leak test green, compute budget fixed as wall-clock-equivalent of the original 500-sim search.

| Phase | Days | Deliverable | Gate to next |
|---|---|---|---|
| A. Learning infrastructure | 1–3 | C++ feature extractor; state/outcome logger (real + search-visited states); torch trainer; weight export; C++ MLP inference; two integration points: leaf value at cutoff and UCB prior from a policy head | Equal-compute A/B harness runs end-to-end on a trivially-trained model |
| B. Expert iteration for combat | 3–12 | Loop: self-play with current search → train V (and π over hand actions) → plug in → paired eval → repeat. Replace random rollouts only when the learned leaf beats them at equal compute | Combat agent beats rb3 at equal compute, P ≥ 0.9, on val; confirmed on test |
| C. Learned run-level value | 7–16 | V_run(deck, relics, HP, gold, floor, act, boss) = P(win) from logged runs (thousands/day on 4 cores); card/path/shop/rest/boss-relic decisions = argmax V_run after the choice (one-step lookahead), replacing KB heuristics where it wins; counterfactual-rollout data augmentation at decision points | Strategic layer beats the heuristic layer at equal compute |
| D. Ascension ladder + Act 4 | 12–24 | Keys (ruby/emerald/sapphire) and Act 4 enabled in policy; A10 then A20 tracks with their own `best`; Heart-specific prep | A20 Heart runs complete without sim crashes; ≥ 15% |
| E. Fidelity (continuous) | — | Invariant checkers in trace builds; fuzz runs on every sim change; fix root causes, never route around | 0 crashes/hangs per 1,000 games |
| F. Stretch | 24–30 | Silent via lightspeed card implementation (large), or publish engine + seed sets + protocol | — |

**Durability rules (ephemeral container):** everything that matters is committed to
`autoresearch/ironclad` (code, patch, results.tsv, run TSVs, trained weights as `.inc`).
`research/setup.sh` rebuilds the simulator from the pinned commit + patch. The resume
procedure below is what a fresh container executes. Scheduled tasks re-wake this session.

**Honesty rules carried forward:** no hidden information in any agent input (leak tests gate
every build); fixed compute per comparison; `test` touched only at milestones; the simulator
is the environment, not a stand-in for the real game — results are "in lightspeed".

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

### Current state (update after every batch) — 2026-11-01 07:35 UTC
- **Best: `priormlp_d0` — val 61.3% win, floor 44.6, score 1.488 (+0.044 vs chest1, P=0.900): the
  first learned component kept — the MLP move prior (49% of available information) used as a PUCT
  prior at the root only, ~zero extra cost.** `priormlp_d0_c1` (prior_c 1.0) 64.0% (+0.031 vs the
  new best, P=0.82 — follow-up batch 21); depth-1 use −0.01 at equal wall-clock and at +compute.
- Previous best `chest1`: val 57.7%, **TEST (run 5/5): 273/500 = 54.6%, 95% CI [50.2%, 58.9%]** →
  milestone M4 (A0 ≥ 50%) confirmed on held-out seeds.
  Test budget: the 5-run cap is spent; rule amended — one further test run per *confirmed*
  milestone (M-A10/M-A20/Heart), never for tuning.
  Chain since phase 1: rb3 (fixed sim) 38.0% → chest fix 57.7% → MLP move prior (root) 61.3%.
- Keep chain overall: legacy 3% → rollout END_TURN weight 10% → 14% → events 13.7% (after the
  Dome-honesty re-baseline) → KB boss relics 18.3% → AoE 20.3% → scaling 23.0% → map events
  30.0% → near-miss combo 32.0% → no hallway potions 35.7% → [sim fixes → 38.0%] → chests 57.7%.
- Losses by floor (chest1 run, 127 losses, computed): Act 1 hallway 4 · Act 1 boss 20 · Act 2
  hallway 13 · **Act 2 boss 35** · Act 3 hallway 20 · **Act 3 boss 35**. Bosses are 71% of losses.
- Learned components: bilinear move prior (neutral so far), V_run for picks (clean model
  awaiting test), bootstrapped V (sanity pending). Everything learned is gated by paired val.
- **Keys/Act 4 (`INFO_keys1`, val 300, keys=1 chest_pol=1): 35.0% of runs reach Act 4 (105/300;
  135 beat the Act 3 boss, 30 of them without all keys), and 16.3% (49/300) beat the Heart** —
  the program's first Heart wins; 47% conversion once in Act 4. A separate metric track: the
  A20-Heart north star is now reachable in principle. Never compared against A0 Act-3 numbers.

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

Throughput work: allocation-free action enumerator (bit-identical). **Idle-machine timing (8 games,
single thread, same sim lib): pre-enumerator build 20.2 s vs current 20.7 s — no measurable speedup;
the earlier "~13%" was contention noise.** Consequence: the sims=650 "equal wall-clock" conversion
(`spd_s650` +0.0375 P=0.86, `spd650b` +0.0225 P=0.75) is NOT protocol-justified; sims stays 500.
Those runs simply confirm that more compute helps a little. The fixed-size card piles (`sts_card_manager_use_fixed_list`, +10%) were tried and
**reverted**: piles can exceed any fixed capacity in degenerate rollouts (Dead Branch +
Corruption chains) and the Release library has asserts compiled out → silent stack corruption
(3/300 val games crashed in `spd_s650`; all 3 seeds run clean with `std::vector` piles).
Lesson recorded: every speedup must pass a ≥300-game crash sweep, not just a 4-seed identity check. Protocol note: the compute budget is defined as the
wall-clock of the original implementation at sims=500; verified speedups may be converted
into proportionally more simulations (logged as such, timed on the same seeds, idle machine).

Compute accounting finding: `budget_actions=7500` (equal *total* actions, no cutoff) lost
−0.065 (P=0.03) vs equal *sims*: a fixed per-decision action budget starves the long, complex
fights that matter most. Equal-sims stays the protocol; cutoff variants are compared at
sims scaled by their measured cost-per-simulation ratio instead.

Batch 12 (vs rb3): `bud_base` −0.065, `bud_rt3` −0.285, `bud_rt6` −0.173, `bud_rt2` −0.373,
`esc15` +0.007 (P=0.76, discard) — the whole fixed-action-budget / cutoff family loses.
Rollout cutoff + heuristic leaf: `bud_rt3` −0.285 (P=0.00). A learned leaf value would have
to beat an *unbiased* terminal rollout and be trained on search-visited (random-play-degraded)
states, i.e. an expert-iteration loop — shelved as out of scope for this session; the
evidence and the design (features at turn start, ~130 dims incl. encounter one-hot, target =
search value at battle end, MLP 130→64→1 exported as a .inc) are recorded here for a follow-up.

### Phase A status (2026-11-01 04:20 UTC)
- Done: `research/vf/vf_features.h` (204 observable-only dims), `mlp.h` (DIM→64→1 inference,
  weights from `research/cpp/vf_weights.inc`), `train_vf.py` (torch, battle-grouped holdout,
  exports .inc), `vf_log=1` logger in FairAgent (binary rows per decision point, labelled at
  battle end with win/final_hp/search-unit target/battle id), `leaf_vf=1` hook at rollout
  cutoff (`rollout_turns=0` = pure leaf eval). Build is bit-identical with everything off.
- Data: `train*` seed splits (300000+, 400000+, 500000+) are for generation only — never
  used for keep/discard. gen0 = 500 games of the current best agent.
- Phase B wiring done too: `policy_features.h` (action tokens: 384 cards + 48 potions + end turn
  + select; 4 target feats), bilinear prior `logit = (W s + b)·E[tok] + up·(h·E_up) + AB[tok] +
  T·tf` (K=16, cheap enough per expanded node), PUCT selection with first-play urgency
  (`prior=1`, `prior_c`), root-visit logger `pf_log=1` → `pol_<seed>.bin`, `train_policy.py`.
  Phase C wiring: `run_features.h` (666 dims), `rv_log=1` → `run_<seed>.bin`, `train_run.py`.
  All loggers off by default; build bit-identical with everything off (checked on seeds 100004/5).
- Data: gen0 (train seeds) and gen1 (train2 seeds), 500 games each, all three loggers.
  **Bug found and fixed before any model was used:** the fork harness closed the child's
  stderr, the loggers' `fopen` reused fd 2, and `std::cerr` diagnostics were written into
  the binary logs (garbage rows: 1,344 "battles", 15% win labels). Child stderr now goes to
  `/dev/null`; both datasets regenerated. The first (corrupted) V training produced NaNs —
  the trainer now drops non-finite rows and the exporter must never emit NaN.
- Second loader bug caught by the fork-path validation: `train_vf.load` globbed every `*.bin`
  and mixed in `run_*`/`pol_*` files (different strides) → excluded. Validation now asserts
  8 < rows/battle < 30 and 0.7 < win-label rate ≤ 1 before any training.
- Batch 13 (equal-compute controls, vs rb3): `spd_s650` +0.0375 (P=0.86, n=297 — 3 crashes
  from the fixed-pile bug, see above; re-run pending on the fixed build), `rt3_s750` −0.216.
- Calibration protocol for `sims` conversions: `research/tools/calibrate.sh` times N single-
  threaded games per config on an **idle** machine (checks load first) and prints the ratio.
- **Learned leaf V, result (gen0, 173k rows / 9.1k battles): val RMSE 12.4 vs 28.2 predict-mean,
  win-acc 96.6% vs 94.4% base — a decent *battle-outcome* predictor, but as a search leaf it is
  catastrophic** (`leaf_vf=1 rollout_turns=0`: floors 6/11 at 500 sims, floor 1 at 3,000 sims,
  vs 33/23/44 baseline). Diagnosis: every row of a battle shares one label, so V learns
  battle-level features (encounter, HP, deck) and is nearly indifferent to within-turn progress;
  the search then ranks sibling actions by model bias and degenerates into passivity. Unbiased
  terminal rollouts carry exactly the action-conditional signal V lacks. Fix path (backlog, not
  now): bootstrapped targets — log the search's own root/edge values per decision and regress V
  on those (AlphaZero-style), iterate. **Phase B pivots to the move prior first** (keeps rollouts,
  uses learning to focus the tree), which is also the cheaper model at inference.
- Move prior trained on gen1 (147,620 decisions, 5.4 legal actions avg): val CE 0.2825 (from
  0.3133 at epoch 1; uniform-prior/entropy baselines now printed by the trainer). In a one-game
  probe `prior=1` reached the same floor with ~9% fewer rollout actions per decision.
- V_run first fit overfit (val log-loss 0.88–1.04 vs 0.679 base: rows within a run share one
  label and the one-hot deck/relic features identify the run). Regularised (input/hidden dropout
  0.3, wd 1e-3, hidden 32, run-grouped early stopping): **best epoch 3, val log-loss 0.567 vs
  0.679 base, acc 0.72** → exported. The trainer writes an untrained placeholder whenever the
  model fails to beat the base rate, so `rv_pick` can never silently degrade to noise.
- Batch 14 (vs rb3): `lowhp1` +0.010 (P=0.61), `lowhp2` −0.306 (strong risk aversion →
  passivity), `nodome` −0.003 (P=0.38: allowing Runic Dome is neutral for this agent, so the
  honest Dome handling costs nothing), `neow2` −0.017 (P=0.34), `maxhp05` −0.023 (P=0.25).
  **Batch 14: no keeps** — the hand-tuned value/policy knobs are exhausted at this level; gains
  must now come from structural changes (chest fix, learned prior/value, keys/Act 4).
  Batch 15 queued: `chest1`, `prior1`, `prior1_s450`, `rvpick1`, `INFO_keys1`.
- **Third data-integrity bug, found by cross-referencing run logs with the TSV:** 146/1,000
  generation games aborted (14.6%) — only with loggers on. gdb: `std::map::at(DRAW_REDUCTION)`
  threw inside the feature extractor; lightspeed sets that status as a bit with no map value, so
  `getStatus` throws although `hasStatus` is true. Guarded (absent → 0, valueless → 1); the 8
  previously-crashed seeds now complete. Datasets regenerated a third time (gen0/gen1, pf v2
  format with root/edge values); prior, V_run and a bootstrapped V retrained on them.
  Models trained before this fix saw truncated games (biased toward fights without that debuff);
  the batch-15 `prior1`/`rvpick1` runs use those earlier weights and are therefore *lower bounds*.
  (calibrate N on an idle machine; don't assume) vs rb3. Train prior on gen1 → `prior=1` at
  equal wall-clock. Train V_run on gen1 → one-step-lookahead card picks (Phase C integration TBD).

### Policy hole found while implementing keys (2026-11-01 05:20 UTC)
lightspeed's TREASURE_ROOM action is `0 = open chest, else = leave`; its legacy agent passes
`true` (=1) meaning "take chest", so it never opened a chest — and FairAgent inherited the
handler. **Every run so far skipped all treasure-room relics** (and the sapphire key).
`chest_pol=1` fixes it (Cursed Key: open only large chests unless `chest_cursed_open=1`);
queued as `chest1` in batch 15. Expect a sizeable keep; everything downstream re-baselines on it.

### Phase D design (keys, Act 4, ascension) — implemented as `keys=1` (with `chest_pol=1`)
- lightspeed already has Act 4 (`transitionToAct(4)` when all three keys are held; encounters
  SHIELD_AND_SPEAR, THE_HEART). The current policy never takes keys: rewards `KEY` action,
  rest-site option 2 (ruby key), Act-1/2 burning-elite emerald key, treasure-room sapphire key.
- Add `keys=1`: map DP gives the burning-elite node a bonus; rewards policy takes `KEY` when
  offered; rest policy takes ruby key when HP ≥ `key_rest_hp` (default 0.6) and the deck has
  no urgent upgrade; treasure room takes the sapphire key once per run. Then eval at A0 with
  `win = beat the Heart` as a *separate* metric column (keep "beat Act 3" too).
- Ascension tracks: implemented as `TRACK=a10|a20 exp.sh` (per-track `best_<track>_{name,params}`,
  `--asc` passed through); `research/tools/new_track.sh a10` seeds a track by running the A0 best
  params as the baseline. A10 seeding is queued after batch 19. A20 adds Ascender's Bane and the
  double boss: expect card-removal priority to matter more.

### Batch 15 (2026-11-01 05:58 UTC) — **`chest1` keep: val 57.7% win (from 38.0%), floor 44.2, Δscore +0.270, P=1.000**
Opening treasure chests is the largest single gain of the program (every earlier run had skipped
all chest relics). `prior1` −0.010 (P=0.39) and `prior1_s450` −0.004 (P=0.45) vs chest1: the
biased-data bilinear prior is neutral (it buys ~10% rollout actions, nothing more). Clean retrain
(gen0+gen1, 298k decisions): prior val CE 0.2822 vs uniform 0.2913, entropy floor 0.2620 — it
captures only ~31% of the available information; a stronger action model is needed for Phase B.
V_run (clean, 1,000 runs): val log-loss 0.561 vs 0.693 base, acc 0.70 (early-stopped epoch 4).
Bootstrapped V (root-mean targets): val RMSE 9.73 vs 33.9 predict-mean — **as a pure leaf it still
collapses** (20 dev seeds: 500 sims 0%/floor 16, 1,500 sims 5%/20, 3,000 sims 25%/26 vs baseline
60%/46.7). Leaf replacement is closed as a negative result at this data scale, for both target types.
`rvpick1` (biased-data V_run one-step lookahead): **−0.343** (win 32.7%). Diagnosis: an
observational run-value model learns "bigger deck / more relics ⇒ survived longer ⇒ wins", so
adding almost any card looks good → deck bloat. Strategic picks need *interventional* evaluation
(simulate upcoming fights with vs. without the card), not a correlational P(win) — see Phase C v2.
val ≥ 50% ⇒ milestone **M4 criterion on val**; test confirmation queued (test run 5/5).

### Phase C v2 — counterfactual card evaluation (`cf_pick`), to implement after the swap
For each card-reward option (and skip): copy the run state, add the card, and play `cf_fights`
sampled encounters from the current act's pool with the real combat search at a small budget
(`cf_sims`), using common random numbers across options (same encounters, same search seeds) so
the comparison is paired. Score = mean(win ? HP fraction at end : −0.5). Take the best option if it
beats skipping by `cf_margin`. Cost ≈ 4 options × 3 fights ≈ 0.25 s per reward → val eval ≈ 3×
slower; acceptable for experiments. This is the interventional estimate the observational V_run
cannot provide; no hidden information (the encounter pool is public knowledge, HP rolls are fresh).

MLP move prior (state 204 ⊕ token emb 16 ⊕ target 4 → 64 → 1; 298k decisions): val CE 0.2769,
**49% of the available information captured** (bilinear: 31%; uniform 0.2913, entropy floor 0.2620).
Used at root/depth-1 only (`prior_mlp=1`, `prior_mlp_depth`). 20-seed sanity probe (dev 100000+,
under load): baseline 60%/floor 46.7/5.7 s; depth-1 65%/46.3/7.8 s (+37% time — chance branching
makes depth-1 nodes numerous); depth-2 65%/45.5/13.7 s; **depth-1 with prior_c=1.0: 80%/49.0/8.6 s**
(n=20, SE ≈ 11 pts — suggestive only). Root-only probe (fresh dev seeds 100020+): baseline 70%/45.5;
root-only prior_c 1.5: 65%/42.5; root-only prior_c 1.0: 75%/46.1 at ≈ baseline cost (5.9 vs 5.8 s);
depth-1 prior_c 0.7: 85%/46.9 (+45% time). All n=20 — only val (batch 18) can separate these.
The in-chain idle calibration refused to run (probe binaries were active); redo when idle. Batch 18 queued (val, vs best): `priormlp_d0`, `priormlp_d0_c1`, `priormlp_d1_c07_s365`
(equal wall-clock), `priormlp_d1_c07` (informational, +compute). The remaining 51% is largely irreducible at 500-sim targets
(root visit distributions are noisy) — distilling from higher-sim searches is the next data lever.
`cf_pick` probe (12 dev seeds, under load): win 0.50 / floor 42.8 vs baseline 0.58 / 44.9 (one game
apart — noise at n=12), cost 2.3× per game (19.8 s vs 8.5 s). Val experiments queued after batch 16:
`cfpick1` (3 fights × 60 sims) and `cfpick2` (4 × 100). Hypothetical fights at 60 sims are played
weakly, which biases the estimate toward cards that help a weak player; more sims costs more.

### Batch 16 results (vs chest1, val 300)
`prior2` +0.002 (P=0.52), `prior2_s450` +0.022 (P=0.73): the clean bilinear prior is neutral at equal
compute. `rvpick2` −0.333, `rvpick2_m` −0.621: **observational V_run picking is closed** — a
correlational P(win | deck) cannot be used counterfactually (bigger deck ⇒ survived longer ⇒ "good").
`spd650b` +0.0225 (P=0.75; earlier `spd_s650` +0.0375, P=0.86): a consistent small gain from 30% more
simulations, below the keep bar at n=300. Decision rule: adopt sims=650 only if the idle-machine
timing shows the current build is ≥1.3× faster than the original (equal wall-clock = protocol), not
as a "keep". Queued in the post-batch-18 chain together with the pre-speedup build timing.

### Batch 17 results (val 300)
`cfpick1` (3 fights × 60 sims) **−0.267** (win 37.7%), `cfpick2` (4 × 100) **−0.224** (42.3%) vs
chest1 — counterfactual card evaluation as implemented hurts badly. Diagnosis: argmax over 3 noisy
candidate estimates + 1 skip estimate suffers winner's curse (something always looks better than
skipping) → takes far too many cards → deck bloat; weakly-played hypothetical fights also reward
cards that help weak play. Closed in this form. A viable variant would use it only as a *veto* on
the KB's single top candidate vs skip, with ≥8 fights and a margin — cost ≈ 2× again; backlog.
Heart track: `wb3` (w_burning 3) −0.015 (P=0.10; Act 4 36.7%, Heart 14.7% vs 16.3%) and `krh05`
(ruby key down to 50% HP) −0.036 (P=0.01; Act 4 38%, Heart 13%) — both discard: reaching Act 4 more
often is easy, converting it is the constraint (Act-4 fights need a stronger deck, not more keys).

### Batch 21 (PUCT c with the root MLP prior, vs priormlp_d0 c=1.5)
`pm_c10` 64.0% (+0.031, P=0.82 — reproduces `priormlp_d0_c1` exactly, deterministic harness),
`pm_c07` 62.3%, `pm_c05` 62.7%: none clears the bar; c stays 1.5. The c=1.0 point is consistently
higher but within noise at n=300. Chain continues: gen2 (500 self-play games at 2,000 sims with the
root-prior agent) → retrain prior → `priormlp2_d0` → A10 seed → boss-prep batch.

### Boss-aware deck preparation (`boss_prep=1`) — implemented, queued as batch 20
Bosses are 71% of chest1's losses and the act boss is visible from the first floor. Card valuation
gets per-boss adjustments: AoE vs Slime Boss / Automaton / Collector / Donu&Deca; block vs Guardian
/ Champ / Automaton / Time Eater; no powers vs Awakened One; fewer 0-cost cards vs Time Eater.
Experiments: `bossprep1`, `bossprep2` (A0), `bossprep_h` (Heart), `bossprep_a10` (A10 track).

### Batch 16 plan — DONE (results above); items 1–4 executed, item 5 became the Heart track
1. `prior2 prior=1` and `prior2_s450` with the clean-data prior (gen0+gen1 v2, ~300k decisions).
2. `rvpick2 rv_pick=1` with the clean-data V_run; `rvpick2_m rv_margin=0.02` (only take a card
   when V_run improves by ≥2 pts — guards against noise in near-ties).
3. Bootstrapped V (`vf_boot_weights.inc`): copy to `vf_weights.inc`, build a *side* binary, run
   `leaf_vf=1 rollout_turns=0` at sims 500 / 1500 / 3000 on 20 dev seeds first (cheap sanity:
   must not collapse like the outcome-regression V did); only then a val experiment at
   calibrated sims. Keep the live `vf_weights.inc` untrained until a leaf model passes sanity.
4. If `chest1` keeps (expected), everything above is already measured on top of it (exp.sh
   promotes sequentially); re-run `spd_s650` on the fixed build for a clean throughput→sims number.
5. Keys track (now live: 35% Act 4, 16.3% Heart at A0): create `best_heart` files; `HEART=1 exp.sh`
   runs with keys=1 and scores win = Heart; first ideas: `w_burning` 3, `key_rest_hp` 0.5,
   Act-4 prep in card picks when all keys held (AoE for Shield & Spear, burst/scaling for the Heart),
   skip the sapphire key when the chest relic is rare, then A10/A20 ladders on both tracks.

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

0. Fresh container? `ls /home/user/sts_lightspeed/build/fair_eval || research/setup.sh`;
   `git branch --show-current` must be `autoresearch/ironclad` (else `git fetch origin
   autoresearch/ironclad && git checkout autoresearch/ironclad`). Python deps: `pip install
   torch --index-url https://download.pytorch.org/whl/cpu` (CPU wheel) for `research/vf/`.
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

## Operational lessons (self-inflicted, recorded so the loop stops repeating them)
- `pkill -f` / `pgrep -f` with a pattern that appears in the invoking shell's own command line
  matches itself: killed two jobs and deadlocked four queued waiters (they could never see "0
  processes"). Always `pgrep -x <exact name>` / `pkill -x`; never `-f` from a chain.
- A silently failing compile inside a `cmd | grep | head` pipeline let a chain continue with a
  stale binary and commit NaN weights. Always `cmd 2>err || { report; exit 1; }`.
- Every data pipeline needs a validation assert at the consumer (rows/battle range, label rates,
  finiteness) — three corrupt-data bugs were caught only there.

## Stop condition

Stop when the backlog is empty AND the last 5 idea *families* all failed on
`val`, or the day's time runs out. Then run `test` once on the best agent and
write `research/REPORT.md`.

## Known statistical caveat

With a P ≥ 0.9 keep rule over dozens of trials, some keeps are false
positives (winner's curse on `val`). That's why the north star is `test`,
which the loop never optimises against.

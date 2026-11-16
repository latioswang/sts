# pD review — adversarial review of Plan D (evidence first)

2026-11-16. Read-only; nothing built or run. **[computed]** = my arithmetic on cited numbers; **[estimate]**, **[inferred]** = mine. FA / FS = `research/cpp/FairAgent.cpp` / `FairSearch.cpp`; EM = `research/eval/eval_main.cpp`; GC = `sts_lightspeed/include/game/GameContext.h`; pm = `research/program.md`.

## Verdict in five lines

1. The measurements are new and cheap, and several belong in any plan. The plan claims more: that they pick the build. They cannot.
2. Every table quantity is measured through today's pilot (random playout, hand-set terminal value), so none separates a weak deck from a weak evaluator, the split the combat branch depends on.
3. By the plan's own priors, 75% of outcomes and every conflict start the same P1. Ten days redirect the first build in at most a quarter of cases.
4. Of P0's compute, 76% buys precision on two questions that about 5 core-hours settle well enough; two more readouts are predictable from existing data.
5. Cut P0 to a three-day triage. The build behind it is under-scoped, and its dollar figures are about 8 times too low on this fleet.

## Fatal or near-fatal flaws

**F1. The decision table is not identified.**
- `C16` is our search given the true future, with the same playout and terminal value. It bounds information plus depth, not a better engine. The one cheating row, on lightspeed's search, scored below the fair agent (9.6%, r1 §4); the 50.1% clairvoyant agent uses 32,000–384,000 simulations (s6 §1a). "`C16` under 15% ends combat-engine-first" does not follow: random rollouts cannot win boss fights and the player's HP is invisible in a loss (d1 §5; c1 §6.2). Clairvoyance leaves that in place.
- `Doomed` and `Own` inherit the defect: a fight the evaluator cannot judge is "doomed" at any budget. The plan holds this confound against F42b and F50.
- `G_deck ≥ 2 × G_hp` compares arbitrary doses (full heals against two relics per act); the doses set the ratio. An HP gift prices HP, not who lost it.
- `Ref`: the 85% / 75% thresholds have no par. The unit is 40 decks per cell, 24 hand-written, not "4,000 fights". With a between-deck spread of 25–30 points [estimate], `Ref − Own` has an SE of 6–7 points: "under 10" and "20 or more" lie 1.5 SE apart [computed].
- The rows are neither exclusive nor exhaustive, and "if patterns conflict, unified-staged is the default" absorbs every miss.

**F2. P0 cannot fail and rarely redirects.** Its gate is "validity only"; its kill is unreachable given the default. The claimed benefit (T1 37% → 45%) is asserted, not derived.

**F3. The favoured build meets its first hard kill in week 7** ("A20 < 11% after 15M runs"). The pre-step (card-only PPO at A10, +3 points) tests the surface with the least measured headroom: the honest 8-playout oracle at cards is −0.020 ± 0.008 (A10) and −0.009 ± 0.004 (A20); only paths show any, +0.014 (h2 §1). No consequence is stated if it fails.

**F4. The build is under-scoped.**
- `research/vf` holds supervised trainers only. Hooks act in place: card skip is a threshold (FA:929-936), the shop buys a removal first (FA:944-947), events return one index (FA:1296-1299), the rest handler issues actions 0–2 (FA:1301-1362).
- Workers are separate 4-vCPU sessions merged hourly through git (pm:59-65), with a 2-hour limit and hourly restarts (h4 §5). 15M runs in synchronised rounds need one large machine, an owner decision the plan does not request.
- P1's 18 days cover more than Plan A's 10-day loop, which its reviewer put at 18–22 (pA_review): nearer 30 [estimate].

## Corrections (novelty, code, arithmetic)

| Mechanism | In the history | "Different this time"? |
|---|---|---|
| Clairvoyant own search (D1) | Never (00 §3 k); specified as E0 at under 2 core-hours (s3 §5) | New; the "ceiling" reading fails (F1) |
| K-world shared tree | Never (00 §3 f). Neighbours: tree reuse −0.0088, −0.0015; one particle per node is "suboptimal regardless of the computation time" (w2 §A2) | New; see code |
| Search to 64× (D2) | Never above 4× (00 §3 j) | New; low information |
| Reference decks, fatal-fight replays, replicates, profile | Never (00 §3 l, u; d1 §6.6) | Yes |
| Gifts (D3c) | Elites without a gift are known: REST→ELITE −0.083 ± 0.027 per decision (pm:336); avoidance kept (+0.037, +0.019; r1 §1); veto +0.0003 | Only the gifted arms |
| PPO policy, privileged critic, restarts | Never (00 §3 n, o). Neighbours negative: A20 value retrains on newer own games −0.0186, −0.0189 (pm:487-489); ports 0 of 9, against "ascension sampled uniformly" | Untested, not favoured |
| Fight terminal = critic (P2) | Never (00 §3 a) | Yes; closed neighbours undeclared |
| Outcome-trained entity boundary value (P3) | Closed line; one 500-game outcome test (00 §3 b) | Target and representation differ; declared |

**Code.**
- Evidence item 6 is wrong for A20. `map_model=1` makes the path MLP choose (FA:796-822); `elite_hp=0.8` and `w_elite_act1=-2` reach it only as one input (FA:815). That model takes elites at any HP, and a veto was neutral (h1 §3).
- Checked and true: `determinise` unconditional (FS:470); `cheat_combat` (FA:146); `stats.simulations` counted (FA:177), never printed (EM:217-220); seed (EM:51); alarm (EM:194); `bc.init(g, enc)` (FA:431); public battle and step calls (FairAgent.h:53-55); injector (Scenario.cpp:152-197).
- K-world: `determinise` and the rollout share one generator (FS:470, FS:426), so worlds need their own. One node is added per simulation (FS:497-498) and a decision runs about 500 (c1 §4): K = 16 leaves 31 per world, with a turn's first decision four to six plies above END_TURN. At 1× the arm barely reaches the boundary [inferred]. Only clairvoyance is budgeted at 4× and 16×, so depth and information stay confounded.
- Forced elites: `chooseMapX` is private (FairAgent.h:82) and the path MLP overrides the weights. A driver must replace the path model on those nodes, removing a +1.3-point component with it.
- Injector: `obtainRelic` fires pickup effects (Scenario.cpp:158); run logs lack relic counters and bottled cards; log parsing is unbudgeted. "150 lines" is 3–4 days [estimate].
- A leak test built on `reseedAllRngs` (FA:1137-1150) leaves `monsterList`, `eliteMonsterList`, `secondBoss` and `bossRelics` untouched (GC:130, 190-194).

**Arithmetic.**
- P0 compute holds: D1 ≈ 30 and D2 ≈ 140 core-hours at constant game length (c2 §5), against 53 and 177; the table sums to 302 [computed].
- Money: $0.1785 per box-hour is an AWS list price. The only in-house datum is 10 worker sessions for 2 h ≈ $30 (pm:109), $1.5 per box-hour. P0 ≈ $130; the plan ≈ $2,600–3,400, not $320–410 [computed].
- Budget: the caps equal the action count of 500 simulations (c1 §4). "1,000 simulations of 6 µs" is 6 ms, twice the budget; a 1.5–5 µs boundary value leaves 400–800 simulations, not 700–1,000 [estimate].
- Play time: 100–160 strategic states (c2 §2, §6.4) plus the terminal table come to 3–6% of an A20 game, not 1–2% [computed]. Tolerable.
- D5: d1's win correlation of 0.43 in a Beta model gives 21% of seeds won at least once in 8, or 2.8 × the mean, between the plan's 1.7 × and 3 × [computed]. Both outcomes of that modifier are already in §2.3.
- D1's A20 Heart cell has 6 wins in 2,000 (d1 §1): nothing resolves.

## Evidence against the thesis

- The record is not "about equal". A20's second doubling adds +0.8 points; the Heart fight stays at 47–49% lost at 2× and 4×; no stage stops being a death floor (d1 §3). The surveys call determinisation "no step change left" (w2 §A1) and expect clairvoyance to show "how little is left" (s3 §6). D1 and D2 take 230 of 302 core-hours.
- Decision-level headroom is nil except paths (h2 §3); twelve phases of value work moved A20 by 1–2 points (h4 §6.4). That coordinated change was never tested argues for testing P1's loop early, not for D3.
- The agent's caution was measured as correct for this pilot (pm:336). The profile gap is probably an effect [inferred]; D4 cannot turn it into a lever.
- T1's "For" column is three self-reported outside numbers. spire-engine has its own engine and a learned combat policy; this project's fork switch alone halved A20 (6.2 → 3.0%, r1 §4). Mentor's 30% re-ran at 16.8% (s6 §5); its 22.8 points for hidden information are StS2, chance nodes, 8,000 simulations (s4 §1).
- Teaching the HP-after-fight head from clairvoyant and 16× play prices risk for a pilot the agent does not have: the direction in which Mentor's surrogate failed (survival 0.64–0.68 against 0.29, s4 §1).

## Rule problems

- K-world is "adopted in any branch" on a P0 reading: eight cells, no confirmation slice, an SE of about 1.2 points at A10 against a +2 bar [computed with d1's 0.43 seed correlation].
- P2 changes what the closed terminal-constants row changed (00 §2); §6 names only R6 and F5. §7's balance-trained playout weights touch the closed rollout-policy row.
- D2 has no crash ceiling. Crashes rose with search before the last fixes (3 of 2,000 at 2×, h1 §2); crashed and timed-out seeds leave the pairing, and long games are the winning ones.
- Fight quantities are gated on fights, not decks (F1).
- Sound: clairvoyance and 8–64× search are declared; gates from P1 on use 2,000–4,000 seeds; no budget change.

## A faster kill test

Three days, at most 20 core-hours, pre-registered.
1. Day 1: `diag_clair` and the seed salt. 1,000 fresh A20 seeds: fair 1×, clairvoyant 1× and 4×; 500 seeds fair 16×. About 3–5 core-hours [computed].
2. Days 2–3: an in-process driver copies the run before every boss fight and re-fights it 16 times at 1× and 4×, 2,000 seeds. About 12 core-hours [estimate].
3. Reading.
   - Clairvoyant 4× under 15% and fair 16× under 12%: information and budget are not the constraint. Drop K-world, the 64× curve and D3c.
   - Top-quartile arrival states under 76% at the act-2 boss (the average T1 needs [computed from d1 §2]): no drafting policy reaches T1 with this pilot. The open question is then the evaluator, which only a build answers.
   - Clairvoyant 4× at 25% or more: the full P0 is justified.

If the first two hold, Plan D is another plan with a three-day preface.

## What survives

- The diagnostic binary; the clairvoyance number; the curve to 16×; replicates for sizing gates; Heart traces; the per-run strategic log.
- The fight bench as first gate for any combat change, built from the agent's own arrival states.
- Win points on paired seeds, a sealed 2,000-seed slice per setting, P5's claim rules.
- An out-of-combat leak test, provided it re-rolls the encounter lists.
- Honest search in training, no fight surrogate, no network above a few microseconds in the tree.
- The explicit, conditional request to reopen the evaluator.

## Revised probabilities for the three tiers

| Tier | Plan | Mine | Reason |
|---|---|---|---|
| T1, A20 Act 3 ≥ 25% | 45% | 15% | Needs every stage's death rate cut by about 40% [computed from d1 §2]; measured levers are worth 1–2 points each. P0 adds information, not strength; the schedule probably ends before P3 does |
| T2, A20 Heart ≥ 20% | 12% | 3% | Needs act-4 reach 2.5% → 35–40% and conversion 14% → 50–60% in a fight that search does not move; no fair specialised agent exceeds 8% (s6 §3) |
| T3, A20 Heart ≥ 45% | 3% | 0.5% | The clairvoyant hybrid's level, at 30–400 times this budget |

A20 ≥ 15% (P1's gate): 30% [estimate].

# pA review — adversarial review of Plan A (strategy first)

2026-11-16. Read-only; nothing built or run. **[estimate]** = my judgment, **[computed]** = my arithmetic on cited numbers, **[inferred]** = my reading. FA = `research/cpp/FairAgent.cpp`; GC = `sts_lightspeed/include/game/GameContext.h`.

## Verdict in five lines

1. Whole-run policy-gradient training of the out-of-combat policy is new here and deserves a bounded test, but the plan never measures its premise: that the frozen pilot can turn better decks and more risk into wins.
2. Its headroom arithmetic is contradicted by the project's two measured decision-to-play conversions (about one fifth of the additive sum). Corrected, it predicts A20 near 10–16%, the plan's own "most likely failure", not 25%.
3. P0 cannot be killed and P1 can pass without testing the thesis.
4. The closed training loop does not exist on this fleet; engineering time is understated about twofold [estimate].
5. Run a one-week pilot-ceiling test first; fund P1 only if the ceiling allows T1.

## Fatal or near-fatal flaws

**F1. The premise is unmeasured, and in-house interventions point against it.**
- T1 needs every stage's death rate cut by about 40% [computed from d1 §2: scaling the seven A20 stage death rates by 0.6 gives 24.6%]. At the act-2 boss the average entrant would have to die less often (23.8%) than today's entrants with 85%+ HP (25%, 18 of 71, d1 §4). One search doubling cuts stage death rates by a relative 5–13% (d1 §3).
- At A20, switching a map choice from rest to elite costs −0.083 ± 0.027 score per decision (h2, F34). The A20 keeps came from elite avoidance (`a20_eh08` +0.037, `a20_noel1` +0.019, r1 §1); the best still carries `w_elite_act1=-2` (`best_a20_params.txt`).
- The pilot pays 11–16% of max HP per hallway fight and about 25% per elite (d1 §4). The expert profile presupposes cheaper fights; the profile gap is probably an effect of the pilot, not a lever [inferred].
- The plan's modal outcome (plateau at 10–13%, §5) lies below its own P2 gate (15%). The tests that would settle the premise (same pilot on strong decks, perfect-information ceiling; d1 §6.1, §6.5) are deferred to §7.

**F2. "Gain = sum of one-step advantages" fails on the plan's own evidence item 7.** The path model's +0.0133 per decision over 10–13 path decisions per run (h2 §2) predicts +0.13 to +0.17; play gave +0.030, a ratio of 0.17–0.23 [computed]. Decision-trained V: +0.0019 × about 12 card decisions predicts +0.023; play gave +0.0042, ratio 0.18 (h3). Discounted by 0.2, the plan's "+0.3 to +1.0 score" becomes +0.06 to +0.2: about +2.6 to +8.7 win points at the path model's exchange rate [estimate].

**F3. The first two weeks cannot falsify the thesis.**
- P0's kill needs all three diagnostics null, including "profile inside the expert band". d1 §7.2 already shows 761 heals against 596 other rest-site actions in losing A20 traces. The kill is unreachable.
- D-b's formula μ(1−ρ) is correct [checked]. But the HP strata of d1 §4 (25% deaths at 85%+ HP, 68% below 60%) imply ρ ≈ 0.10–0.25 [estimate]: a re-fight win rate of 0.45–0.54, inside the plan's undecided band. And a high ρ cannot separate strategy from attrition the pilot causes (00 map §7.2).
- P1's Go (+1.5 points against `f43a20_sf2`) can be met by D-c's 24 constants alone, or by the posture surface lifting the hallway potion ban, a combat-parameter effect. Its kill is conjunctive, after 2M runs. +1.5 is one search doubling; T1 needs +17.

**F4. The loop is not buildable in 10 engineering days here.**
- No policy-gradient code exists: `research/vf` holds supervised trainers only.
- Workers are separate cloud sessions pushing git branches merged hourly (`program.md:59-65`). They stop at a 2-hour limit and the coordinator restarts hourly (h4 §5); the last data phase took 7.5–8.6 h against an estimate of 3–4 (h2 §2). P2 needs about 400 synchronised collect-learn-redistribute rounds.
- Hooks act in place; they do not score options. Card skip is a threshold (FA:929-936); the shop is three early returns (FA:944-947, 948-975, 1000-1027); the rest handler issues only actions 0–2 (FA:1302-1362); events return one table index (FA:1296-1299). "All surfaces" needs option encodings for 52 event cases, Neow and keys, and a new inference path: the runtime loader takes a plain unbatched MLP on 666 or 670 inputs (`research/vf/vr_mlp.h:31`, `:49-63`).
- My estimate: P0 + P1 18–22 days, P2 20 or more. The mitigation, one 64–96-vCPU machine instead of ten sessions, is an owner decision the plan does not request.

## Corrections (novelty, code, arithmetic)

| Mechanism | In the history | Does "different this time" hold? |
|---|---|---|
| Whole-run learning from outcomes | Listed "never" (00 map §3 n). But F20/F21: 30,000 on-policy A20 games with ε = 0.2, value refit, greedy play, −0.019 twice; F4 (+120k on-policy games): nothing | Partly. Policy gradient, 30× data and a win return are new; the in-house prior is negative. At card rewards P1 uses F28's inputs (+0.0019 per decision) |
| Joint bias probe (D-c) | Black-box tuning never run | Yes. But "300-seed slices" is wrong for the nearest tests: F51 elite veto, 4,000 fresh seeds, +0.0003 |
| Posture presets | Save-for-boss potion rule A0 +0.035, A10 −0.091; per-fight potion values neutral; `hall_potions=0` is a keep; low-HP penalty +0.010 / −0.306, in the closed terminal-constants row (00 map §2) | State-conditional choice is new; the closure is not declared |
| Plan tokens | Boss-prep bonuses closed (−0.026 to +0.009) | New as conditioning. s2 §5 row 0 offers a 35 CPU-hour pre-test the plan omits |
| Privileged critic, restarts | Never; in-process forks only | Yes |
| One policy for A0–A20 | Ports 0 of 9 | Untested |

**Code.**
- "All hidden streams re-seeded, as `branchEval` does": `reseedAllRngs` (FA:1137-1150) re-seeds 14 streams and reshuffles the relic pools but leaves `monsterList`, `eliteMonsterList`, `secondBoss` and `bossRelics` (GC:130, 191-194).
- V_priv's inputs exist (GC:182-194). The posture hook is easy (`searchParamsFor`, FA:119-138).
- D-a and D-b need no new tool: `FAIR_TRACE` with `analysis/trace_stats.py` yields the profile; fatal-fight replays fit the in-process fork (`bfFight` FA:422-446, `branchEval2` FA:1473-1573).

**Arithmetic.**
- Costs check: 17,750 core-hours = 18.5 fleet-days; $43 to $360 per fleet-day; 0.78–1.8 core-seconds per run (c2 §5).
- Uniform A0–A20 gives A20 one run in 21: P2's 10M runs hold 0.48M A20 runs, half of P1's.
- Power: SE 0.0007 pools 1M runs for one context-free contrast. An update sees 25,000 runs; the same contrast has SE 0.0046 [computed] against effects of 0.001–0.005. About 85% of the proposed return's variance is the 7.6% win event [estimate].
- Play time: full-length runs have about 160 strategic screens (c2 §2), not 100: 4–10% of a game [estimate]. Tolerable.
- T1 is gated "on the milestone test split": 500 seeds (`eval/run.py:13`), SE 1.9 points.

## Evidence against the thesis

- Twelve phases of strategy-value work moved A20 wins by 1–2 points (h4 §6.4). Honest headroom at cards, boss relics and rest is at most +0.005 (h2); the path model took half of the only measured headroom for +1.3 points.
- A20 search scaling (6.0 / 8.5 / 9.3%) and the flat Heart fight (49 to 47%) fit a biased playout that cannot value block or scaling (c1 §6.2) as well as they fit "deck". That defect also teaches the drafter the wrong decks.
- spire-engine: self-reported, own engine, a learned combat policy with boss practice on human decks. Its gap to us is not attributable to strategy; its largest reported A20 step (+5.6 points) is a combat look-ahead (s6). Spire Mentor: 27–30% self-reported, 16.8% re-run.
- The best Ironclad A20 Heart figure (50.1%, s6) pairs a distilled strategy network with clairvoyant search of 32,000–384,000 simulations: pilot strength separates 0.3% from 50% [inferred].
- Language models: 33% is four characters, 60 runs, ±10 points; Ironclad 4 of 15 and 1 of 10. SpireBench measured their strategy as un-expert (57–75% rests, 0.9–1.4 elites), yet the best two clear Act 1 in 90% of 10 runs against our 78%. "Weak tactics, strong strategy" was not measured.
- Winner statistics: 11 winners in 240 traces; shop gold per run is survivorship.
- Evidence item 6 misdescribes A20: the HP-threshold rules are inert there (c2 §0.9); the path model takes elites at any HP and a veto was neutral (h1 §3).
- AlphaStar's anchor and z pulled toward human data; here both come from the agent.

## Rule problems

- **Leak test.** As specified it re-seeds streams. With `reseedAllRngs` it would pass an actor that reads the encounter lists, the second boss or the boss-relic offers, the fields P2 extracts for V_priv. It must re-roll them.
- **Closed line not declared.** Posture presets include the low-HP terms; §6 declares only the P3 utility.
- **Seeds.** T1 on 500 seeds; P4 gates state no seed counts.
- **P1 baseline** is ambiguous (`f43a20_sf2` or D-c's vector); the Grey zone adds a second look.

## A faster kill test

"Pilot ceiling": week 1, at most 100 core-hours, in-process, no trainer, no restart tool.
1. **Arrival-state ceiling.** 4,000 A20 seeds; at each boss start copy the state and re-fight 16 times with fresh dice at full budget (split halves). Pre-registered: if the top quartile of arrival states wins under 76% at the act-2 boss or under 70% at the second act-3 boss (the average rates T1 needs), no drafting policy reaches 25% with this pilot.
2. **Reference decks.** 30–50 expert-grade decks (knowledge-base archetypes; human A20 winners if the owner supplies logs) at the act-2 boss, the double boss and the Heart at 80% HP, 200 fights each. Under 50% at the Heart: T2 and T3 belong to the pilot.
3. **Forced expert profile.** One 4,000-seed arm: positive Act-1 elite weight, smith above 35% HP. A loss of 1 point or more, as F34 predicts, makes the profile gap an effect.
4. **D-c made binding:** under +0.5 points confirmed, no P1.
5. **s2's plan-spread test:** flat, drop plan tokens.

## What survives

- Wins as the keep metric, the score as guard; objective-matched training for A20 and the Heart.
- The strategic trace, per-fight logs, the profile audit, the extended leak test, D-b (in-process) and D-c.
- The zero-initialised residual hook, bit-identical at zero weights.
- Honest search in the training loop, no fight surrogate; privileged critic and restarts as training-only tools.
- Re-fitting the run policy after every pilot change; the kill at P3.

## Revised probabilities for the three tiers

| Tier | Plan | Mine | Reason |
|---|---|---|---|
| T1, A20 Act 3 ≥ 25% | 35% | 10% | Needs a 40% cut in every stage's death rate with the pilot frozen; discounted headroom gives 10–16%; the outside 21.9% includes a learned combat policy. The 10% covers a coordinated regime that one-step tests cannot see |
| T2, A20 Heart ≥ 20% | 10% | 2% | Needs about 35% keyed Act-3 clears (keys cost 4 floors at A20) and 60% Heart wins from a pilot at 6 of 42, in a fight that search does not move |
| T3, A20 Heart ≥ 45% | 2% | 0.3% | Needs a different pilot |

A20 ≥ 15%: 25% against the plan's 60%. If kill-test step 1 passes, T1 rises to about 25% [estimate].

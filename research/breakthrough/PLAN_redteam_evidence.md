# PLAN red team — evidence and odds

2026-11-16. Read-only; nothing built or run. **[computed]** = my arithmetic on cited numbers; **[estimate]** = my judgment. Files are in `research/breakthrough/` unless a path is given.

## Verdict in five lines

1. Finding 1 overstates `x2`: half of the clairvoyant ladder is cleanly strategy, and 80% of that half is two rule sets tuned under a pilot that sees the future.
2. The odds are about twice what explicit factors give: 9% for Ascension 20 Act-3 ≥ 25% and 3% for Ascension 20 Heart > 8% (plan: 20%, 15%).
3. The recipes add mechanism detail, not fair-pilot evidence; the uplift counts results the reviewers already had.
4. Stage 1's undefined band is where the plan itself expects to stall; stage 3 keeps a true +1-point upgrade one time in three; the 2×2 has confounded and unassigned cells.
5. Approve a corrected stage 0 only. On its likeliest reading the PPO replication should lead.

## Findings overstated

**1a. "Pilot held fixed … almost entirely out-of-combat."** The ladder in `x2` §4 sums to +33.3 points [computed]:

| Share | Points | Steps |
|---|---|---|
| Strategy only, same seeds | 16.7 (50%) | boss-relic table 3.3, guide rules 8.6, elite seeking 4.8 |
| Strategy only, pairing not stated | 2.7 | fix3 |
| Combat only, same seeds | 2.6 | no potion discards in search |
| Bundles with a combat or budget change, across seed blocks | 12.1 (36%) | tables + tree reuse 3.4; safety rules + playout target focus 6.9; four late changes 1.9; Heart budget ×2 + two rule sets −0.1 [assumes the potion step repeats] |
| Drift between rows | −0.8 | |

The pilot was not fixed: tree reuse, target focus, potion handling, the terminal score and the budget all changed (`x2` §1 against §4). The plan's sentence is `x2` §6(d), which §4 contradicts. "Search stopped paying after 4×" rests on 19.5 against 18.0% with n unstated and on 40-game Heart samples: unresolved, not null.

**1b. Transfer.** `x2` §6(d): "It does not show that this strategy reaches 50% with a fair pilot. No fair ablation exists." Guide rules and elite seeking are 13.4 of the 16.7 clean points, and `x2` §6(c) says elite seeking "should be re-measured". Here REST→ELITE costs −0.083 ± 0.027 score per decision (n = 49, `research/program.md:337`), and Ascension 20 keeps came from elite avoidance (`r1_ledger.md:52`). For the plan: Mentor's better run policy gained about 40 points under either pilot [computed, `x2` B §4; StS2, Ascension 0].

**1c. Habit rewards.** One sentence by the author, no ablation (`x1` §3, §7). 11 → 17% is the win reward, an objective change to a network that also plays the fights. Habits are shaping that makes PPO explore; with rules plus search most exist already (removal first, `00` §1), and one applied as a rule cost 81 → 46% (`x1` §4). Implication for us: close to nil.

**2.** "23–33 points": `x2` B §4 gives 22.8 and 25.5, at StS2 Ascension 0; 33 appears only in §6(d).

**3.** Importance unmeasured: the 50% agent scores fights with our formula (`x2` §2; `research/cpp/FairSearch.h:40`).

**4.** "+5.6 did work": unreleased, not re-measured, n unstated, about 20× cost (`x1` §1). It sits on a network that plays fights. Stage 2's policy does not, so stage 3(c) has nothing to guide it; a run value read after one turn is a learned leaf under another name.

**5.** Omits negative neighbours: on-policy refits lost −0.019 twice (`pA_review`); stage 1(a) is the `bf_*` model in table form (`c2_strategy_layer.md:51`; Ascension 20 +0.017, not replicated, `00` §2); stage 1(b) is the F26–F38 fork line, headroom ≤ +0.004 at boss relics and rests (`00` §2).

**6** checks out [computed, `d1` §1]. **7.** No review kept tables, forks or rules; `pD` was already a staged union and received 15%.

## Odds rebuilt

| Outcome | Plan | Reviewers | Mine | Factors ([estimate] unless marked) |
|---|---|---|---|---|
| Ascension 20 Act-3 ≥ 15% | 45% | 25%, 30% (`pA_review`, `pD_review`) | 30% | Needs every stage's death rate cut by 22% [computed, `d1` §2]; a search doubling gives 5–13%; twelve phases of strategy work gave 1–2 points (`pA_review`) |
| C1, ≥ 25% | 20% | 9, 10, 10, 15% | 9% (> 22%: 12%; > 27.5%: 4%) | 0.30 × P(≥ 20 given ≥ 15) 0.45 × P(≥ 25 given ≥ 20) 0.40 = 5.4%; plus 3 for a route that skips the chain |
| C2, Heart > 8% | 15% | none; Heart ≥ 20%: 1–3% | 3% | Needs about 18% keyed Act-3 clears and 50% Heart conversion [computed]; today 2.5% and 14% (`d1` §2). Roughly C1 × 0.3; the plan implies C1 × 0.75, though the Heart fight has not responded to search (`d1` §3) |
| Heart ≥ 20%; C3 | 5%; < 2% | 1–3%; 0.3–0.5% (at 45%) | 1%; 0.3% | |

- **Uplift.** Reviewers already cited 21.9 / 27.5%, the clairvoyant 50.1% and Mentor's 30 → 16.8% (`s6`). The recipes add a lottery ticket (10% that the public network doubles our rate under our pilot): +2 to +3 points. They also add negatives: no fair ablation, failed policy-gradient rounds on this simulator lineage (`x2` §4), 4.6 points lost in cloning (`x2` §1). Net: about zero.
- **The bar moves.** spire-engine was published a week ago and is on its third encoding (`x1` §1, §6b). I give 50% that 25% still leads in 15 weeks: about 5% for a claim that holds on delivery.

## Gate and decision-table problems

1. **Stage 0 has no gate** and its triage no thresholds. The reviewers' rule was dropped: top-quartile arrival states under 76% at the act-2 boss mean no drafting policy reaches 25% (`pA_review`, `pD_review`).
2. **The 2×2 cells are confounded.** "Their search" is clairvoyance plus 50–120× compute per game [computed, `x2` §1, `d1` §4] plus max backup. "Their strategy" is strategy plus key-taking plus porting error: observations must match "bit for bit", and a fresh build "does not reproduce the cloud games" (`x2` §6b, §7). No validity cell is pre-registered.
3. **Unassigned outcomes.** Their strategy at 1–2× ours; "a lot" undefined; "neither" with clairvoyance 4× ≥ 15%. The stop row uses an inference `pD_review` F1 rejected (clairvoyance keeps the random playout). The likeliest reading (row 2, about 60% [estimate]) puts stage 3 first, which the plan rates at "single points each".
4. **Stage 1.** Go is an absolute level on a bundle; (d) and (e) are pilot changes, so passing does not credit "strategy by measurement". No compute rule is stated, and 2× search alone gives +1.5 points (`d1` §3). The 11–15% band has no rule, and the plan's "most likely failure" is 12–18%. "After all five" has no deadline.
5. **Stage 2.** Go at ≥ 20% is met by +1 point if stage 1 ends at 19%. The kill compares with the teacher although cloning starts below it. Stage 3 changes the pilot under it, against finding 2.
6. **Stage 3 power.** The paired SE grows from 0.63 points at a 7.6% base to 0.95 at 20% [computed; win correlation 0.43, `d1` §3]. Under the project's keep rule a true +1-point upgrade is kept 58% of the time today (`pC_review`) and about 35% at 20% [computed]; 80% power needs about 8,000 seeds per slice. C1 then needs +5 points from three such upgrades.
7. **Stage 4 and C1.** At a 0.3% base (6 wins in 2,000) the win rate cannot be the keep metric; "campaign" is undefined. C1 has three bars: > 22% and > 27.5% in §1, ≥ 25% in §4 and §5.

## Omitted risks

- **Engine against engine.** C1 compares our simulator with theirs; our own fidelity switch took Ascension 20 from 6.2 to 3.0% (`r1_ledger.md:132`), more than the C1 margin. A bridge exists (`research/realgame/README.md`), yet real-game runs are planned for C3 only.
- **Competitor numbers.** Self-reported; 21.9% is the best checkpoint on its own evaluation seeds; 27.5% has no n (`x1` §7).
- **Development seeds.** The frozen test split holds 500 seeds and has been read (`research/eval/run.py:13`; `00` §2, F49). The sealed sets have no one-read rule; stages 3 and 4 read one at least three times.
- **C2's bar** is a Watcher figure; fair language models already report Ironclad Heart wins of 4/15 and 1/10 (`s6`).
- **Third-party network.** MIT allows use with notice. As teacher or warm start it makes our agent a descendant of tables measured in clairvoyant fights: leak-free at play time only. A claim must say so, beside the human rules of §6.5.
- **Resources.** Synchronised PPO rounds need one large machine (`pA_review` F4, `pD_review` F4); §6 does not ask. A 1M-parameter pass costs about 200 µs at batch 1 (`pC_review`), so stage 3(c) cannot fit 3 ms.

## Main route versus fallback: which should lead

**For the fallback.**
- The only fair existence proof at the bar: 93 / 75 / 22% with no search (`x1` §1).
- On an independent host, search plus a PPO run policy scored 16.8% against 15.6% for a zero-search policy (`s6` §1b; StS2, non-identical driver).
- It builds what stages 2 and 3(c) need anyway: PPO infrastructure, a run value trained through fights, a policy that plays them.
- 100–200 GPU-hours plus 500–1,500 CPU-hours (`x1` §6a), against 5,500 core-hours.
- "Discards the working agent" is sunk cost: that agent trails at every level.

**Against.**
- Habit weights, hyper-parameters, logs and human datasets are unpublished: about 35 tuning runs to rediscover (`x1` §3, §7).
- Our engine needs a batched environment, an entity encoder and scenario reset, with no serialiser (`pC_review`): 25–35 days before a meaningful run [estimate]. No RL code exists here (`pA_review` F4).
- Success is parity with a published agent, and a network that plays fights reopens a line the owner closed.

**My choice.** The fallback leads, behind two one-week checks: the corrected stage 0, and a reproduction spike on spire-engine's own engine (public code, rented GPU): does it pass 15% at Ascension 20? Stage 1(a)–(c) runs only if stage 0 returns row 1. I put C1 at 12–14% this way against 9% [estimate]: inside my error, but cheaper, and not resting on a clairvoyant ladder.

## Changes the plan must make

1. Rewrite finding 1 with the decomposition above; correct "23–33" to "23–26, StS2 Ascension 0".
2. Replace §5 with numbers no higher than 30 / 10 / 3 / 1%, or show the factors.
3. State one C1 bar, one play-time budget, one read per sealed set, and a cross-engine or real-game check.
4. Stage 0: add the validity cell (their pair reproduces about 50% in our build), report key-less Act-3 clears, restore the arrival-state thresholds, assign every cell, and make row 2 trigger the PPO branch.
5. Gate on paired gains per component at fixed 1× compute; define the 11–15% band; use 8,000 seeds per slice at stage 3 and a proxy metric at stage 4.
6. In stage 1, do (d) and (e) first and re-measure tables after each pilot change; specify stage 3(c) or delete it.
7. Add to §6: one large machine, a GPU decision now, and the reopening that a fight-playing network implies.

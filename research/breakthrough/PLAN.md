# Plan to reach state of the art — draft for the owner (2026-11-16)

Status: a plan. Nothing below has been run. It is the synthesis of 22 research digests, four independent plans
(`pA`–`pD`) and their four adversarial reviews, all in this folder. Numbers carry the file they come from;
**[estimate]** marks my own numbers, which are low-confidence.

## 1. What "state of the art" means (`s6_sts_sota_scoreboard.md`)

| Setting (Ironclad) | This project | Best fair specialised agent | Best of any kind | Expert humans |
|---|---|---|---|---|
| Ascension 0 | 85.4% | 93.1% | — | — |
| Ascension 10 | 52.5% | 75.2% | — | — |
| Ascension 20, Act-3 win | 7.6% | 21.9% (27.5% with a look-ahead) | — | — |
| Ascension 20, Heart | 0.3% | none for Ironclad; 8.0% for Watcher | 33.3% (language model, 60 runs, four characters) | 46–65%; best-documented 63% |

- The fair specialised leader is `spire-engine`: one 1.07M-parameter network, PPO on whole runs, no search, a few
  milliseconds per decision, its own simulator, self-reported (`x1`).
- An Ironclad agent reports 50.1% at the Ascension 20 Heart in the real game, but its combat search reads the true
  random state and draw order. It is not a fair comparator. It is built on the same simulator lineage as ours: its
  base commit `7476a81` is an ancestor of our fork (checked 2026-11-16), and its strategy network is public (`x2`).

Defensible claims, in order of difficulty:
1. **C1 — best fair specialised Ironclad agent:** Ascension 20 Act-3 win above 22% (above 27.5% to clear the
   look-ahead figure) on a sealed set of 2,000 or more unseen seeds.
2. **C2 — first fair Ironclad Heart figure above the best fair specialised Heart number:** Ascension 20 Heart above 8%.
3. **C3 — best agent of any kind:** Ascension 20 Heart above 33%, with real-game runs.

## 2. What the research established

1. **The strategy layer is a first-order lever.** With its pilot held fixed, the clairvoyant agent went from 16.8% to
   50.1% at the Ascension 20 Heart almost entirely through out-of-combat changes: measured stage-value tables, a
   boss-relic table (+3.3 points), rules from a human guide (+8.6), elite seeking (+4.8), fixes from loss traces.
   More search stopped paying after 4× (`x2` §4). `spire-engine` credits most of its Ascension 20 gain (about 11% →
   21.9%) to a win reward and out-of-combat "habit" rewards (`x1` §3).
2. **The pilot is first-order too, and the two are co-adapted.** Hiding the true future cost Spire Mentor 23–33
   points; a run policy wins with the pilot it was trained with and loses with another (`x2` part B). In our own
   data, switching a rest to an elite costs −0.083 score at Ascension 20 with our pilot (`pA_review`).
3. **Our fights do not see the run.** A fight is scored by 35 + HP + a flat potion credit; the run-value model is
   never asked what a fight outcome is worth. The repair was planned on 2026-11-02 and never started
   (`00_internal_map.md` §3 a). The first thing `x1` lists as worth copying is one run-level value used in fights.
4. **Our closed leaf-value line tested one design:** a small network on 204 aggregate features regressing the mean of
   random playouts. No system in the AlphaGo lineage trained a value that way (`s1`). Three outside projects also
   failed with learned combat values (`w1`, `x2`); a policy-guided look-ahead scored by the run value did work
   (+5.6 points, `x1`).
5. **Never tried here** (`00_internal_map.md` §3): any reinforcement learning; privileged critics; run value as the
   fight utility; a perfect-information ceiling of our own search; search above 4×; the same pilot on strong decks;
   an objective for the Ascension 20 Heart; comparison with expert behaviour.
6. **The score hides wins.** At Ascension 20 floor/51 + win is about nine-tenths floors (`h3`).
7. **Every single-mechanism plan was rejected by its reviewer** as a main line (revised odds of C1: 9–15% each).
   What all four reviews kept is the content of this plan.

## 3. Thesis

The missing strength is in both halves, and the cheapest large part is out of combat. It has to be earned against
our own fair pilot, because strategy and pilot are co-adapted. So: first split the gap with artefacts that already
exist; then rebuild the strategy layer by measurement and let fights price the run; then turn it into one learned
policy improved with the fair search in the loop; then upgrade the pilot with the designs that worked in fair
systems; then train for the Heart.

## 4. Stages

Gates are in win rate on fresh paired seeds (2,000 per slice, confirmed on a second slice), with the score as a
guard. Compute is CPU-only unless stated. Money: about $1.5 per 4-vCPU box-hour at the rate this project has
actually paid (`pD_review`); cloud list price is about one eighth of that.

| Stage | What | Effort | Compute | Gate / kill |
|---|---|---|---|---|
| **0. Split the gap** (week 1) | (a) The 2×2 on common Ascension 20 seeds: {their public strategy network, our strategy} × {their clairvoyant search, our fair search}. (b) Triage: our own search with the true future at 1× and 4×, fair search at 16×, each boss arrival state re-fought 16 times. (c) Instruments: per-stage death table and boss win rate by entry HP, strategic trace, replicate seeds, sealed seed sets, an out-of-combat leak test that also re-rolls encounter lists. (d) Behaviour audit against expert statistics. | 5–7 days | ≈ 100–150 core-hours (≈ $40–60) | No gate. Readings choose the emphasis of stage 1 (table below). |
| **1. Strategy by measurement, fights that price the run** (weeks 2–4) | (a) Stage-value tables: the causal effect of adding, upgrading or removing a card on each later fight, measured with our own pilot. (b) Whole-run forks on shared seeds for rare, heavy decisions: Neow, boss relics, elite routing, pre-boss rest, keys. (c) Rules from the knowledge base and from loss traces, each as its own paired test. (d) Fight end valued by the run-value model, starting with a known-answer test (in the last fight HP and potions are worth nothing). (e) Potion policy. | 15 days | ≈ 400 core-hours (≈ $150) | Go: Ascension 20 ≥ 15% and Ascension 10 ≥ 60%, confirmed. Kill: Ascension 20 < 11% after all five; then the pilot is the limit and stage 3 moves ahead of stage 2. |
| **2. One learned strategy policy** (weeks 4–9) | Clone the stage-1 teacher into one network that scores every option on every screen; improve it by PPO on whole runs with our fair search playing every fight (reduced budget in training, full budget at gates), a critic that sees hidden information in training only, restarts from the states where runs die. | 25 days | ≈ 1,600 core-hours (≈ $600); a rented GPU is optional (≈ $50–200) | Go: Ascension 20 ≥ 20% on 4,000 seeds. Kill: two legs with no confirmed gain over the stage-1 teacher. |
| **3. Pilot upgrades from fair systems** (weeks 6–11, overlaps) | (a) Root candidates compared on shared sampled futures, early stop kept. (b) Draw chance nodes with merged outcomes and widening. (c) A look-ahead guided by the stage-2 policy and scored by its run value. | 20 days | ≈ 800 core-hours (≈ $300) | Each alone on 2,000 + 2,000 seeds. **C1 is tested here: Ascension 20 ≥ 25% on the sealed set.** |
| **4. The Heart at Ascension 20** (weeks 10–15) | Keys and Act 4 inside the policy; Heart-labelled objective; practice from Act-4 entry states. | 15 days | ≈ 2,500 core-hours (≈ $900) | **C2: Ascension 20 Heart > 8% on the sealed set**, then 20%. Kill: < 3% after two campaigns. |

Total: about 15 weeks, 80 engineering days, 5,500 core-hours (≈ $2,000 at our rate) [estimate]. Reviewers found
the architects' effort estimates about twofold low; these figures already include that.

**How stage 0 steers stage 1** (read with care: every cell is measured through an existing pilot):

| Reading of the 2×2 at Ascension 20 | Meaning | Consequence |
|---|---|---|
| Their strategy with our fair search clears Act 3 at least twice as often as ours | The strategy lever works under our pilot | Stage 1 as written; their network is the benchmark our own must beat |
| Their strategy with our fair search is no better than ours, and our strategy gains a lot from their search | Information and pilot bind | Stage 1 keeps (d) and (e); stage 3 comes before stage 2 |
| Both | Both, multiplying | Order as written |
| Neither, and clairvoyance at 4× stays under 15% | The gap is elsewhere | Stop and report |

## 5. Odds

All [estimate], low confidence. The four reviewers gave 9–15% for C1 to plans that lacked the two source-level
recipes; those recipes are the reason these numbers are higher.

| Outcome | Probability |
|---|---|
| Ascension 20 Act-3 ≥ 15% (double today) | 45% |
| **C1**, Ascension 20 Act-3 ≥ 25% | 20% |
| **C2**, Ascension 20 Heart > 8% | 15% |
| Ascension 20 Heart ≥ 20% | 5% |
| **C3**, Ascension 20 Heart > 33% | under 2% |

Most likely failure: the strategy rebuild lifts Ascension 20 to 12–18% and stalls, because with a fair pilot that
loses 11–16% of max HP per ordinary fight the cautious strategy remains correct, and the pilot upgrades give
single points each.

A different route exists if this one stalls: replicate the `spire-engine` recipe end to end on our simulator
(entity-token transformer, PPO, practice fights, 100–200 GPU-hours ≈ $60–180, `x1` §6) and put our search on top.
It is not the first choice because it discards the working agent and needs a GPU; it stays as the fallback.

## 6. Owner decisions

1. Approve stage 0 (one week, CPU only). Later stages are approved one at a time, on results.
2. Choose the claim: C1 and C2 as the programme's targets, C3 as the direction (my recommendation).
3. Win rate as the keep metric at Ascension 20 and for the Heart.
4. Measurement-only use of clairvoyant search and of budgets up to 16×, never reported as agent results; cloning
   and running the public MIT-licensed `sts-ironclad-agent` for the 2×2.
5. Human knowledge in the strategy layer (guide rules, expert statistics), disclosed in any claim. Both leading
   systems use it.
6. The learned-evaluator line stays closed. Stage 3 (c) is a policy-guided look-ahead scored by a run value, not a
   leaf network on rollouts; it will be put to the owner again before it starts.
7. A GPU is not needed before stage 2 and is optional there.
8. Still open: draft PRs #20 and #21; the f28w5 force-push question.

## 7. What this plan does not do

Clairvoyant search as an agent; a value network trained on rollouts; a learned fight-outcome model in place of
fights during training; search results as a training target for the policy; best-line (max) backup; imitation of
mass human logs; a language model at decision time; further single-parameter tuning or budget moves between fights.

## 8. Files

`00_internal_map.md` (tried / never tried), `s6` (scoreboard), `x1`, `x2` (the two leading recipes), `c1`, `c2`
(the code as it is), `d1` (where runs end), `pA`–`pD` plans and `p?_review.md`, `h1`–`h7`, `r1`, `w1`–`w3`, `s1`–`s5`.

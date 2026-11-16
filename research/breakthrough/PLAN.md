# Plan to reach state of the art — revision 2 (2026-11-16, after the red team)

Status: a plan. Nothing below has been run. It rests on 22 research digests, four independent plans (`pA`–`pD`),
their four adversarial reviews, two source-level recipes (`x1`, `x2`) and two red-team reports on revision 1
(`PLAN_redteam_feasibility.md`, `PLAN_redteam_evidence.md`), all in this folder. **[estimate]** marks numbers that
are judgment, not measurement; all odds are low-confidence.

## 0. What changed from revision 1, and why

The red team was right on these points, and the plan is changed accordingly:
- **Odds were about twice too high.** They counted the two recipes as new evidence although the reviewers already
  had their headline results. Section 5 now uses numbers built from explicit factors.
- **Finding 1 was overstated.** Of the clairvoyant agent's +33 points, half (16.7) is cleanly strategy on shared
  seeds; a third sits in bundles that also changed the pilot; and 80% of the clean half is two rule sets tuned under
  a pilot that sees the future. In our own data an elite in place of a rest costs −0.083.
- **The 2×2 with the outside strategy network is dropped from stage 0.** It needs 6–12 days of porting, cannot be
  verified, and its key cell is one-sided: a null would follow equally from co-adaptation, an engine change or a
  port bug. A cheaper experiment with our own artefacts answers the same question (stage 0 below).
- **Stage 1 (a) and (b) of revision 1 repeat two lines this project already measured** (deck values from simulated
  fights; whole-run forks) as having almost no headroom with our pilot. Revision 1 did not say so.
- **The learned route should not sit as a fallback.** It is the only route with a fair existence proof at the bar
  and it works whichever half of the gap binds. It becomes one of two routes chosen after stage 0.
- Effort, compute and money were low; gates had an undefined band exactly where the plan expects to land.

Not accepted: switching to the learned route now. It needs a GPU and a choice of engine, both the owner's, and its
estimated advantage (12–14% against 9%) is inside the estimate's own error.

## 1. What "state of the art" means (`s6_sts_sota_scoreboard.md`)

| Setting (Ironclad) | This project | Best fair specialised agent | Best of any kind | Expert humans |
|---|---|---|---|---|
| Ascension 0 | 85.4% | 93.1% | — | — |
| Ascension 10 | 52.5% | 75.2% | — | — |
| Ascension 20, Act-3 win | 7.6% | 21.9% (27.5% with a look-ahead, measured once) | — | — |
| Ascension 20, Heart | 0.3% | no Ironclad figure; 8.0% for Watcher | 33.3% (language model, 60 runs, four characters) | 46–65% |

The fair leader, `spire-engine`, is one 1.07M-parameter network that plays fights and everything else, trained by
PPO on one consumer GPU, no search, its own simulator, self-reported, checkpoint chosen on its evaluation seeds
(`x1`). A 50.1% Ironclad Heart agent exists but its search reads the true random state; it is not fair (`x2`).

One bar per claim:
- **C1:** Ascension 20 Act-3 win above 22% on at least 2,000 unseen seeds read once, at our 1× budget, with either a
  run on the leader's own engine or real-game runs beside it.
- **C2:** a fair Ironclad Ascension 20 Heart rate above 8%.
- **C3:** Ascension 20 Heart above 33% with real-game runs. A direction, not a target.

## 2. What the research established

1. **Both halves matter and they are co-adapted.** Strategy changes under a strong pilot were worth at least 17
   points in the clairvoyant agent; hiding the future cost another system 23–26 points (Slay the Spire 2,
   Ascension 0); a run policy wins with the pilot it was trained with (`x2`). How much of our own gap is strategy
   and how much is pilot has never been measured.
2. **Our fights do not see the run.** A fight is scored by 35 + HP + a flat potion credit. Scoring it by the run
   value was planned on 2026-11-02 and never started (`00_internal_map.md` §3 a).
3. **Our closed leaf-value line tested one design**, a value regressed on random playouts, which no system in the
   AlphaGo lineage used (`s1`). Learned combat values also failed in three outside projects (`w1`, `x2`).
4. **Never tried here:** reinforcement learning of any kind, privileged critics, a network that plays fights with a
   run-level value, a near-perfect-pilot ceiling at a real budget, an objective for the Ascension 20 Heart.
5. **The score hides wins:** at Ascension 20 it is about nine-tenths floors (`h3`).
6. **Every reviewed plan was rejected as a main line**, with 9–15% for C1.

## 3. Stage 0 — split the gap with our own artefacts (8–9 working days, CPU, 45–95 core-hours)

| Item | What | Reading, fixed in advance |
|---|---|---|
| **E1** | 1,000 common Ascension 20 seeds with our strategy and four pilots: fair 1× (exists), fair 16×, our search given the true future at 4×, and the simulator's own clairvoyant searcher at its design budget of 40,000 simulations (one parameter, `cheat_combat`; it has only ever run at 500). | Our decks under the near-perfect pilot win **20% or less: strategy binds. 40% or more: pilot and information bind. Between: both.** It is a lower bound, since our strategy is tuned to a weak pilot. |
| **E2** | Each boss arrival state re-fought 16 times with both pilots. | Top-quartile arrival states under 76% at the act-2 boss with the fair pilot: no drafting policy reaches 25% with this pilot. |
| **E3** | A per-card value table built from the fork data already on disk (37,700 decisions), scored on held-out decisions. Free. | Does not beat the agent's own picks: the measured-tables idea needs a new argument before any compute. |
| **E4** | Instruments: per-stage death table, replicate seeds, sealed seed sets with a one-read rule, an out-of-combat leak test that re-rolls encounter lists, a longer hang guard. | — |
| **E5** (needs the owner's resources) | Reproduce the leader on its own engine from its public code: does it pass 15% at Ascension 20 on seeds it did not select on? 5–40 GPU-hours. | Below 15%: the existence proof is weaker than claimed. |

No clairvoyant result is ever reported as an agent result. Nothing in stage 0 stops the programme on the
clairvoyant numbers alone: our search with the true future keeps its random playouts and is not a ceiling.

## 4. After stage 0 — one route decision

| | **Route L: learn it (the leader's recipe)** | **Route M: rebuild our agent by measurement** |
|---|---|---|
| What | One network for fights and strategy with one run-level value, PPO on whole runs with practice fights; then a look-ahead on top; then the Heart objective, which the leader never trained for the Ironclad. Start on the leader's own engine, where the code runs today and a claim is like for like. | In this order: (1) fights scored by the run value and a potion policy, each tested alone; (2) strategy rules and tables as bundles, re-measured after each pilot change; (3) one strategy network cloned from that teacher and improved by PPO with our fair search playing the fights; (4) root comparison on shared futures and draw chance nodes. |
| Leads when | E1 says pilot and information bind, or both; and E5 reproduces. Works under either reading of E1. | E1 says strategy binds, and E5 fails or no GPU is available. |
| Needs | A GPU (100–200 GPU-hours, ≈ $60–180, plus a host machine); 25–35 days before a meaningful run if moved to our simulator; the owner's ruling that a fight-playing network is not the closed line. Its tuning (about 35 runs, reward weights unpublished) must be rediscovered. | CPU only; 19–28 days for its first two steps; about 15 weeks and 5,500 core-hours in all (≈ $2,300–4,100 at what this project has actually paid). One large machine for synchronised PPO rounds. |
| Main risk | Success is parity with a published agent; the bar may move in the meantime. | Stalls at 12–18%: with a fair pilot that loses 11–16% of max HP per ordinary fight, cautious strategy stays correct. |

Gates on either route: paired win-rate gains per component at fixed 1× compute; 2,000 + 2,000 seeds for effects of
two points or more and 8,000 per slice once the base rate nears 20%; a result between a go level and a kill level
means one pre-declared extension and then the kill; sealed sets read once.

## 5. Odds

All [estimate], low confidence, built from the factors in `PLAN_redteam_evidence.md`.

| Outcome | Route M | Route L |
|---|---|---|
| Ascension 20 Act-3 ≥ 15% (double today) | 30% | similar |
| **C1**, Ascension 20 Act-3 > 22% | 9–12% | 12–14% |
| **C2**, Ascension 20 Heart > 8% | 3% | 3–5% |
| Ascension 20 Heart ≥ 20% | 1% | 1% |
| **C3** | 0.3% | 0.3% |

The leader was published a week ago and is on its third version; there is roughly an even chance that 22% is no
longer the bar in fifteen weeks.

## 6. Owner decisions

1. Approve stage 0 (E1–E4: 8–9 working days, CPU only). Later work is approved on its results.
2. E5 and Route L need a GPU machine this session can reach, and network access to the leader's public repository.
   Decide now whether that is on the table; without it only Route M exists.
3. Which claim is the target. My recommendation: C1 and C2; C3 as a direction.
4. Win rate as the keep metric at Ascension 20 and for the Heart.
5. Measurement-only use of clairvoyant search and of 16× budgets; edits to the frozen harness for replicate seeds
   and a longer hang guard.
6. Whether human knowledge (guide rules, expert statistics, human winners' decks) may be used and disclosed. Both
   leading systems use it.
7. Whether a network that plays fights (Route L) is outside the closed leaf-value line.
8. Still open: draft PRs #20 and #21; the f28w5 force-push question.

## 7. What this plan does not do

Clairvoyant search as an agent; a value trained on rollouts; a learned fight-outcome model in place of fights during
training; search results as a policy training target; best-line (max) backup; importing a strategy network trained
against a clairvoyant pilot; a language model at decision time; further single-parameter tuning.

## 8. Files

`PLAN_redteam_feasibility.md`, `PLAN_redteam_evidence.md` (what was wrong with revision 1), `00_internal_map.md`
(tried / never tried), `s6` (scoreboard), `x1`, `x2` (the two leading recipes), `c1`, `c2` (our code as it is), `d1`
(where runs end), `pA`–`pD` and `p?_review.md`, `h1`–`h7`, `r1`, `w1`–`w3`, `s1`–`s5`.

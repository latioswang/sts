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
| M5 | ✅ val win ≥ 70% at A0 (pre-registered 2026-11-01 15:20 UTC, reached by `bfbonus10_g1` 73.3%) | done: **test 66.6% [62.4%, 70.6%]** (500 seeds), +12.0 pts vs chest1's 54.6% test, P=1.000 |

### Parallel workers (since 2026-11-02 14:20 UTC)
Each cloud session is its own VM (docs, code.claude.com cloud environments: "4 vCPUs, 16 GB of RAM, 30 GB of disk";
no cap on concurrent sessions is documented; parallel sessions share the account's rate limits). This VM is saturated
(load ≈ 4.0 on 4 vCPUs), so extra throughput comes only from extra sessions. Protocol: `research/workers/README.md`.
A worker clones this branch, rebuilds `fair_eval` with `research/setup.sh`, must pass three `repro_check.sh` gates
(A0, Heart, A20 bests bit-identical) before any experiment, logs rows to `research/results.d/<name>.tsv`
(`RESULTS_TSV`), and pushes only to `worker/<name>`; the coordinator merges hourly. Large data stays on the worker.
- `s2scale` (session_01USafwg3sueC62wyc3kzKBV): the pre-registered S2 scale-up, moved off the main queue
  (`research/workers/s2scale.md`). It regenerates the 2,500-game leaf set (deterministic, same seeds) because the
  GB-scale data cannot travel through git.
- `a10ports` (session_01USafwjBF1dhvLKK43LjNFt): A10 ports of three A0 keeps (val-300 rule) and A20 ports of two
  (pooled-500 rule) (`research/workers/a10ports.md`). Prediction: most fail, as on the Heart track.
- Coordinator (this session): Phase R on the main queue.
- **18:21 UTC:** the idle workers declined the R2 batch sent by message (a cross-session message is not owner
  authorization for uploads to an outside store) — the correct call. R2 runs on two fresh workers created with the batch as
  their initial task: `r2w1` (session_01USafwhNx9uZKoZZySmu4G4, W=1, seeds 350000+) and `r2w2`
  (session_01USafwgScPPSCxpwPyvHx9L, W=2, seeds 380000+). `s2scale` and `a10ports` stay idle (not archived).
- **18:29 UTC:** informational v1 cell 9 (9,000 runs, h=1024; ≈ 90 min) dropped so the coordinator's R2 share starts now;
  v1 is superseded and cells 7–8 already confirm it loses to the mean at 9,000 runs.
- **a10ports results (merged 15:22 UTC; gate bit-identical on all three bests), pasted from its log:**
  - `a10_mapd085` 26.7% win, floor 36.7, -0.039 vs base, P=0.011 → discard
  - `a10_potsave` 29.3% win, floor 37.4, +0.001 vs base, P=0.517 → discard
  - `a10_wea1` 28.7% win, floor 37.2, -0.010 vs base, P=0.331 → discard
  - No A10 keep, so no rep_dev replication was needed.
  - `a20_mapd085` 2.0% win, floor 25.5, +0.009 vs base, P=0.704 → info
    - pooled val,dev2,dev3 [act3]: n=500  a20_mapd085 win=0.032 floor=27.53  a20_restboss1 win=0.028 floor=27.41  dscore=+0.0062 dwin=+0.004 P(improve)=0.833
  - `a20_potv8` 0.0% win, floor 24.1, -0.037 vs base, P=0.002 → info
    - pooled val,dev2,dev3 [act3]: n=500  a20_potv8 win=0.016 floor=26.54  a20_restboss1 win=0.028 floor=27.41  dscore=-0.0287 dwin=-0.012 P(improve)=0.002
  
  Queue empty; no keeps on A10 or A20; best_a10_* / best_a20_* unchanged. (note.py shows the last stage row for A20 names; the pooled line is the verdict.)
  Verdict: none of the five ports keeps (prediction held). `a20_mapd085` pooled P is in the pre-registered
  near-miss band, so the worker runs `ext_pooled.sh a20_mapd085 a20_restboss1` (dev slice, pooled 600) next.
  Extension result (pasted from `research/runs/pooled600_a20_mapd085.txt`):
  `pooled val,dev2,dev3,dev [act3]: n=600  a20_mapd085 win=0.028 floor=27.32  a20_restboss1 win=0.027 floor=27.23  dscore=+0.0033 dwin=+0.002 P(improve)=0.704`
  `no`
  -> no keep; A20 best stays `a20_restboss1`. a10ports has no further pre-registered batch and is idle.
- **s2scale:** gate bit-identical; leafdata regenerated and its training-row count reproduces sv_11 exactly
  (worker log); leafdata2 generation running.
- **s2scale fits (pasted from its log):** 4.2x the leaf rows of sv_11 (`sv_11.meta`: 12.060 on 765,531 rows):
  `SV id=16 rows=3225306 hidden=42 layers=2 params=10459 holdout_rmse=11.736 predict_mean_rmse=34.737`
  `SV id=17 rows=3225306 hidden=96 layers=2 params=29089 holdout_rmse=11.216 predict_mean_rmse=34.737`
  `LEAKTEST checked=123 mismatches=0 PASS` (leaf_sv=16). Caveat from the worker: a different 50,000-state holdout than sv_11's, so
  the RMSEs are not strictly paired. 4x data moves the h=42 net only 12.06 -> 11.74 (capacity-limited); h=96 reaches
  11.22. Play tests `sv16`, then `sv17`, running on the worker. Worker incident: torch was missing (setup.sh did not
  install it); fixed in setup.sh (CPU wheel pinned to the coordinator's version).

### Current state (update after every batch) — 2026-11-06 04:19 UTC
- **2026-11-06 04:19 UTC: f1w23 batch reassigned to the coordinator VM (implementation only; data unchanged).** f1w23 replied that it would restart only on the owner's word given directly in its session (stopped 03:24 at chunk 108/300). Generation is deterministic per seed and the fork build passes the same gates bit-identically, so the coordinator regenerates the identical batch (`research/tools/f1_w23_local.sh`: the f1data.md lines for W=23 / A0 / SEED 5,000,000 / HOLD=a0, packs + pushes the three shard sets, then deletes the raw copies). The coordinator queue now waits for the local marker instead of pulling `rundata_f1_a0_hold_z` / `rundata_f1_w23_k32_z`. f1w23 told to stand down (no restart, no push) and archived. ETA ~5.5–6 h.
- **2026-11-06 04:16 UTC: hourly check.** f1w26 done and merged: `PACK research/vf/data/rundata_f1_w26/all -> research/vf/data/rundata_f1_w26_z/all: 30000 files, 30 shards, 6790.7 MB -> 88.8 MB (76.4x), verify OK`; `PUSH rundata_f1_w26_z DONE: 31 files`; `QUEUE EMPTY 03:25:28`. f1w25 (A10) at chunk 200/300, ETA ~05:05. f1w24 (A0) running. **f1w23 (A0 + A0 holdout) blocked since 03:24**: its runner hit the 2-hour limit and it waits for a restart OK despite the standing approval in the worker README; sent it a pointer to that approval (information, not authorization). If it still waits, the owner must confirm in its session. Coordinator queue: pulled `rundata_f1_a20_hold_z` (2 files) at 03:24, now waiting for `rundata_f1_a10_hold_z`.
- **2026-11-06 03:19 UTC: hourly check; F1 coordinator queue armed.** Merged f1w23 / f1w25 (gates OK, queues started) and f1w26: `SV GEN rundata_f1_w26 DONE: 30000 files`; `SUBSAMPLE ... rundata_f1_w26_k32/all: 30000 runs, rows 2533837 -> 937282 (k=32)`; holdout 1000 files; `PACK ... rundata_f1_a20_hold_z/all: 1000 files, 1 shards, 229.3 MB -> 3.0 MB (76.3x), verify OK`; `PUSH rundata_f1_a20_hold_z DONE: 2 files`. f1w24 at chunk 61/300. Implementation details of the pre-registered F1 training (no change to data, candidates, selection rule or arms): the trainer excludes holdout runs only from its DATA_DIR, and the A10 / A20 holdout games are inside the f1w25 / f1w26 train sets, so DATA_DIR = the three holdouts unpacked into one dir (`rundata_f1_hold3`, 3,000 runs) with `--holdout 3000` — every holdout run is then excluded from every train dir by run id; `research/tools/vr_select_mean.sh` scores each candidate on the three holdouts separately and picks the lowest mean RMSE (gate: beats predict-the-mean). Leak-test line dry-run with vr_151: A0 94 / A20 105 checked, 0 mismatches. Queue runner (setsid) waiting for the seven shard sets (max 12 h).
- **2026-11-06 02:15 UTC: hourly check.** F1 workers all running: f1w23 gates OK (3x bit-identical on the fork), queue started 01:25; f1w25 gates OK, queue started 01:24; f1w24 at SV chunk 20/300 (~7 h left, per its session); f1w26 (A20, short games) at chunk 94/300 (~1 h left). No coordinator jobs running. Symlink and fork build in place.
- **2026-11-06 01:16 UTC: Phase F1 pre-registered (before any of these games) — retrain V on fork data (switch step 5).** Why: on the fork the two tracks whose V carries the most legacy-specific signal fell most (A0 -5.5, A20 -3.35 vs A10 -1.1, Heart -0.9), and x1add's legacy gain shows no sign of transferring; V (vr_151 / vr_81 / vr_121) was trained only on legacy-simulator runs. Repro gates moved to the fork: `repro_check.sh` takes an optional split (seed start = lowest recorded seed); gates `fid_x1add 16 fs1210000n1000`, `fid_a10_r81 16 fs1211000n1000`, `fid_a20_r121s32f 16 fs1212000n1000` — all three OK bit-identical here (params files `research/runs/fid_*.params` written from the best params; README step 3 updated). Data (four workers, `research/workers/f1data.md`): 30,000 fork games each with the track best + `explore_eps=0.2` (X1 showed exploration data is what makes V usable on its own), `rv_log=1`, 32 states per run, shards; seeds 5,000,000–5,119,999 (unused): f1w23 / f1w24 A0 (SEED 5,000,000 / 5,030,000), f1w25 A10 (5,060,000), f1w26 A20 (5,090,000); holdouts = first 1,000 games of f1w23 (A0), f1w25 (A10), f1w26 (A20) — the trainer excludes holdout runs from every train dir by run id. Training (same trainer, `--state-weights`): ids 160 / 161 = h64 / h256 on fork data only (16 states per run); 162 / 163 = h64 / h256 on fork data + every legacy k32 subsample (A0 incl. X1, A10, A20; 6 states per run). Selection 160-163 -> `vr_171` by the mean RMSE over the three fork holdouts (each also reported; references vr_151, vr_81, vr_121). Evaluation (fresh-slice rule on the fork, weights inherited from each track best, leak test first): `f1a0` vr_model=171 vs x1add (500), `f1a10` vr_model=171 vs a10_r81 (500), `f1a20` vr_model=171 vs a20_r121s32f (1,000), `f1heart` vr_model=171 vs heart_relic20 (1,000). Predictions: vr_171 beats vr_151 on the A0 fork holdout MED; `f1a0` keep MED; `f1a10` keep LOW-MED; `f1a20` keep MED; `f1heart` keep LOW. Cost: ~5–8 h of worker time per set (A0 567 s / A10 462 s / A20 243 s per 1,000 games here, plus logging), then ~3–4 h of training and ~3 h of evaluation here. Workers started 01:17 UTC: f1w23 session_01USafwh2Ey2tQpePBs3WXi2, f1w24 session_01USafwiDY8nDJTUZwstM78r, f1w25 session_01USafwin5yYJ1SW91qgHDHH, f1w26 session_01USafwi7SnHWnrgLQsxgoAy; the hourly routine's prompt updated to the fork and F1.
- **2026-11-06 01:15 UTC: simulator switch step 4 done — honest levels on the fork** (`research/tools/sim_rebaseline.sh`; a container restart at ~00:45 lost the partial A10 arm, re-run from scratch on its allocated slice; resume guard added):
  ```
  fid_x1add         fs1210000n1000 a0  n=1000 crash=0 win=0.796 floor=48.02 score=1.7377±0.0168 act4=0.000  567s
  fid_a10_r81       fs1211000n1000 a10 n=1000 crash=0 win=0.361 floor=38.62 score=1.1182±0.0218 act4=0.000  462s
  fid_a20_r121s32f  fs1212000n1000 a20 n=1000 crash=0 win=0.030 floor=27.09 score=0.5607±0.0107 act4=0.000  243s
  fid_heart_relic20 fs1213000n1000 a0  n=1000 crash=0 win=0.118 floor=47.95 score=1.0130±0.0130 act4=0.580  870s  (win = Heart)
  ```
  Leak tests at each track's ascension on the fork: A0 94 / A10 105 / A20 105 / Heart 93 checked, 0 mismatches. vs legacy honest levels: A0 -5.5 (85.1 -> 79.6; prediction "1-5 lower MED" failed by 0.5), A10 -1.1 (37.2 -> 36.1; "±4 MED" held), A20 -3.35 (6.35 -> 3.0; "±3 MED" failed by 0.35), Heart -0.9 (12.7 -> 11.8; "±4 LOW" held). On the fork x1add (79.6%) is level with vr5relic20's fork numbers (80.7 / 79.1%, P1) — no sign that X1's legacy gain transfers [INFERRED, MED; unpaired]. These four are the fork bests (names unchanged); every fork experiment re-runs its base on its own fresh slice.
- **2026-11-06 00:31 UTC: SIMULATOR SWITCH — the loop now runs on the fidelity fork** (owner decision 2026-11-05 09:27, "finish X1 first"; X1 closed 00:29). Done: (1) pin — `research/patches/sts_lightspeed_fidelity.commit` = dfda16c (clean checkout, no patch on top); `research/setup.sh` now defaults to `STS_SIM=fidelity` (legacy still selectable). (2) build — `/home/user/sts_lightspeed_fid/build/fair_eval` (built 11-05 09:42 with `-DSTS_RESEARCH_DIR=/home/user/sts/research`, after the last `research/cpp` change at 09:11; `make` confirms up to date); the default binary path `/home/user/sts_lightspeed` is now a symlink to the fork (legacy tree kept at `/home/user/sts_lightspeed_legacy`), so `run.py` (frozen, untouched), `exp_fresh.sh` and every tool use the fork. Leak tests on the fork with each track best (asc 0): A0 x1add 94 checked / 0 mismatches, A10 a10_r81 94 / 0, A20 a20_r121s32f 94 / 0, Heart heart_relic20 93 / 0 — PASS (re-run at each track's own ascension inside the re-baseline). (3) results are tagged: `research/eval/sim.txt` = fidelity, and `log.py` appends `[sim=fidelity]` to every new results.tsv row (untagged rows = legacy). Results across simulators are never compared in a keep decision. **(4) Re-baseline pre-registered now, before any game** (`research/tools/sim_rebaseline.sh`, report only): each track best on its own new 1,000-seed slice — A0 `x1add` fs1210000n1000, A10 `a10_r81` fs1211000n1000, A20 `a20_r121s32f` fs1212000n1000, Heart `heart_relic20` fs1213000n1000 (counter -> 1214000). These become the bases every fork experiment is compared against (always re-run on that experiment's own fresh slice, as before). Predictions (fork vs the legacy honest levels A0 85.1% pooled, A10 37.2%, A20 6.35%, Heart 12.7%): A0 lower by 1-5 points MED (P1: vr5relic20 on the fork 80.7 / 79.1% vs 80.4% legacy honest — about equal, but x1add's V was trained on legacy data); A10 within ±4 points MED; A20 within ±3 points MED; Heart within ±4 points LOW (act-4 fidelity changes are unknown to me). (5) Then decide on retraining V on fork data (needs fork run data: cost and plan to be written after these numbers).
- **2026-11-06 00:29 UTC: Phase X1 closed.** Control on fs1209500n500 (container restart at ~00:20 lost the partial arm; resumed on the same slice with `FRESH_SLICE=1209500`, base re-used): `x1add n=500 win=0.866 floor=48.80 score=1.8229±0.0205`; `x1pure61 n=500 crash=0 win=0.796 floor=48.22 score=1.7415±0.0233 287s` vs x1add: dscore -0.0815, dwin -0.070, P 0.001 -> discard. Pre-registered comparison x1pure vs x1pure61 (difference of their dscores vs x1add, different slices): -0.0080 vs -0.0815 = +0.0735 (~6.6 win points), z ≈ 1.8 from the per-arm SEs (≈0.030, ≈0.026) -> one-sided P ≈ 0.97 [COMPUTED, MED]. Predictions: x1add keep LOW-MED — confirmed (new A0 best); x1pure keep LOW — held (discard, but a tie: -0.4 points); x1pure beats x1pure61 MED — confirmed. Conclusion: exploration data is what makes V usable on its own — with the old V, V-only picks cost 7 points; with vr_151 they cost none. x1add on three slices: 85.8 / 83.0 / 86.6% (pooled 85.1% over 1,500 games). Next (owner-decided): switch the loop to the fidelity simulator (plan in this file), starting with the re-baseline of all four bests on fresh slices.
- **2026-11-06 00:16 UTC: hourly check. X1 `x1pure`: discard, but a tie with KB+V.** Fresh slice fs1209000n500: `x1add n=500 crash=0 win=0.830 floor=48.45 score=1.7800±0.0221 443s` (base re-run; 85.8% on its selection slice — the expected drop); `x1pure n=500 crash=0 win=0.826 floor=48.25 score=1.7721±0.0226 461s` vs x1add: dscore -0.0080, dwin -0.004, P 0.395 -> discard (prediction "keep LOW" holds). Card picks and boss relics by vr_151 alone, no KB, are within 0.4 points of KB + V — every earlier pure-V replacement lost (-10 to -16 points, `vrb50_nobf` -0.090). `x1pure61` (control, vr_61) running since 00:09 on its own fresh slice; the pre-registered x1pure-vs-x1pure61 comparison is the difference of their dscores vs x1add (different slices, so weaker than paired). No workers active (all archived).
- **23:54 UTC: X1 — `x1add` KEEP: new A0 best (legacy sim).** Training (exploration holdout, 1,000 runs / 163,064 states): `vr_140` (explore only, h64) rmse 0.4673 corr 0.375 best_epoch 1; `vr_141` (explore only, h256) 0.4762 / 0.359, best_epoch 1; `vr_142` (explore + all A0/A10/A20 subsamples, h64, 413,100 runs) 0.4592 / 0.405, best_epoch 3; `vr_143` (same, h256) 0.4598 / 0.404, best_epoch 1; references vr_61 0.5536 / 0.221, vr_121 0.4631 / 0.388. `vr_select3` -> `vr_151` = vr_142 (byte-identical). On-policy fresh A0 holdout (1,000 runs / 166,753 states): vr_61 rmse 0.4769 corr 0.146, vr_121 0.4012 / 0.328, vr_151 0.3945 / 0.372. Leak test (vr_151, vr_pure, vr_pure_relic): 98 checked, 0 mismatches. Fresh slice fs1208500n500: `vr5relic20 n=500 crash=0 win=0.838 floor=48.15 score=1.7820±0.0226 448s`; `x1add n=500 crash=0 win=0.858 floor=48.63 score=1.8115±0.0212 443s` vs vr5relic20: dscore +0.0294, dwin +0.020, P 0.908 -> **KEEP** (best_name x1add, best_params vr_model=151; same compute). Prediction "x1add keep LOW-MED" confirmed. Caveat: P is just over the bar (0.908); this best has not been measured on any other slice — the honest re-baseline at the simulator switch measures it again. `x1pure` and `x1pure61` now run against x1add (the pre-registered "then A0 best").
- **23:19 UTC: Phase P1 closed (all pre-registered stages done).** Stage 3 question 2: `p3_v4s100 n=1000 crash=0 win=0.729 floor=47.46 score=1.6596±0.0181 427s` vs p3_ref500: dscore -0.0751, dwin -0.062, P 0.000. Sims curve with prior v4 (gap to 500 sims / prior v1, fidelity sim, A0): 50 sims -13.2 points (fs1206500n1000), 100 sims -6.2, 200 sims -4.4, 500 sims +0.6 (n.s., discard) (fs1207500n1000). Predictions: within 3 points at 200 sims (MED) failed, at 100 (LOW) failed, at 50 (LOW) failed; v4 beats v1 at 50 sims (MED) confirmed. Conclusions: (1) the agent keeps 500 sims and prior v1 — no strength change; (2) a cheaper agent exists only at a price: 100 sims + v4 is ~6 points weaker at ~4.6x less search time, 200 sims ~4.4 weaker at ~2.4x less — relevant for data generation at scale (more games per CPU hour), not for the bests; (3) prior depth 2 is useless for v4; (4) validation CE is a weak proxy for play strength at low sims. v4 h128 kept as an artifact (`research/models/pfm_p1_h128.inc`, side build `build_v4`), not compiled into the main build. p1w18 (stopped at 15/40 chunks, awaiting a restart approval since 12:07) archived: P1 is closed, its data is not needed; nothing of it is in the store.
- **23:16 UTC: hourly check.** P1 stage 3 question 2: `p3_v4s200 n=1000 crash=0 win=0.747 floor=47.49 score=1.6781±0.0179 811s` vs p3_ref500: dscore -0.0566, dwin -0.044, P 0.002 — gap 4.4 points at 0.41x the reference wall time (timings shared with X1 training). Prediction "within 3 points at 200 sims MED" failed (by 1.4). p3_v4s100 running. X1: vr_142 training since 22:42. p1w18 unchanged (awaits the owner).
- **22:58 UTC: P1 stage 3, question 1 — prior v4 at 500 sims: discard.** On the fresh slice fs1207500n1000 (fidelity sim):
  `p3_ref500 n=1000 crash=0 win=0.791 floor=48.13 score=1.7347±0.0167 1981s` (prior v1); `p3_v4s500 n=1000 crash=0 win=0.797 floor=48.22 score=1.7426±0.0165 1766s` vs p3_ref500: dscore +0.0079, dwin +0.006, P(improve) 0.668 -> discard (below the 0.7 near-miss band; no extension). Prediction "keep MED" failed: at 500 sims the search already corrects most prior errors, so the better prior is worth ~0.6 points there vs 8.5 at 50 sims. The fork agent keeps prior v1 at 500 sims. Question 2 (v4 at 200 / 100 sims, report only) running. X1: vr_141 (exploration only, h256) holdout rmse 0.4762 corr 0.359 (worse than vr_140 0.4673 and vr_121 0.4631; best epoch 1 for both exploration-only models); vr_142 (exploration + all subsamples, h64) training since 22:42.
- **22:10 UTC: P1 stage 2 result (fidelity sim, A0, fresh slice fs1206500n1000, 1,000 paired games each).**
  ```
  p1_ref500 n=1000 crash=0 win=0.807 floor=48.25 score=1.7531±0.0163 1604s   (500 sims, prior v1)
  p1_v1d0   n=1000 crash=0 win=0.590 floor=44.31 score=1.4588±0.0211  207s   vs p1_ref500 dscore -0.2942 dwin -0.217 P=0.000
  p1_v4d0   n=1000 crash=0 win=0.675 floor=45.90 score=1.5749±0.0199  227s   vs p1_v1d0  dscore +0.1161 dwin +0.085 P=1.000
  p1_v1d2   n=1000 crash=0 win=0.605 floor=44.92 score=1.4857±0.0207  736s   vs p1_ref500 dscore -0.2674 dwin -0.202 P=0.000
  p1_v4d2   n=1000 crash=0 win=0.675 floor=45.85 score=1.5741±0.0199 1255s   vs p1_v1d2  dscore +0.0884 dwin +0.070 P=1.000
  gaps vs ref500: v1d0 -21.7 pts, v4d0 -13.2, v1d2 -20.2, v4d2 -13.2
  ```
  Verdict on the predictions: prior v4 at 50 sims beats v1 at 50 sims (MED) — **confirmed** (+8.5 points, P=1.000; v4 closes 39% of the 50-vs-500 gap). Within 3 points of 500 sims (LOW) — **failed** (13.2 points). Prior depth 2 adds nothing for v4 (67.5% both, dscore -0.0008 vs v4d0) at ~5x the wall time (timings confounded: X1 V training shared the CPU), so depth 0 stays. Validation CE understated the play gain (v4 62% vs v1 49% captured, yet +8.5 win points).
- **22:10 UTC: P1 stage 3 pre-registered (before any of these games).** Question 1 (keep decision, fresh-slice rule, fork): does prior v4 h128 improve the 500-sim A0 agent? Arms on the new slice fs1207500n1000 (allocated now; counter -> 1208500): `p3_ref500` (A0 best params, main fork build = prior v1) vs `p3_v4s500` (same params, side build v4); keep iff dscore > 0 and P >= 0.9 — a keep makes v4 the compiled prior of the fork agent (applied at the simulator switch, with its own leak test and repro check). Question 2 (report only): v4 at 200 and 100 sims vs the same `p3_ref500` (where the sims curve meets 500/v1). Script `research/tools/p1_stage3.sh`. Predictions: `p3_v4s500` keep MED (2,000-sim targets should help at 500 sims too, but 500 sims already corrects most prior errors); v4 at 200 sims within 3 points of ref500 MED; v4 at 100 within 3 points LOW.
- **21:17 UTC: hourly check.** P1 arm 1 (fidelity sim, fs1206500n1000): `p1_ref500 fs1206500n1000 a0 n=1000 crash=0 win=0.807 floor=48.25 score=1.7531±0.0163 act4=0.000 1604s` (500 sims, compiled prior v1); p1_v1d0 running, then v4d0, v1d2, v4d2. X1 holdout scores (legacy sim, 1,000 exploration runs, 163,064 states): `vr_61 rmse=0.5536 corr=0.221`, `vr_121 rmse=0.4631 corr=0.388` (V trained on greedy-policy data predicts exploration states poorly — the case for X1). vr_140 training started 20:54 (140-143 next, then vr_select3 -> vr_151). p1w18 unchanged.
- **20:55 UTC: P1 prior v4 selected = h128.** `P1 v4 h=64: ... ep 4 val_ce=0.2441 (captured 60%) 6128s`; `P1 v4 h=128: ... ep 4 val_ce=0.2431 (captured 62%) 6028s`; `P1 v4 selected: h=128 (val_ce 0.2431)`. Side build `/home/user/sts_lightspeed_fid/build_v4/fair_eval` compiled `pfm_p1_h128.inc` (sha1 c464a7f73b5a, PFM_H=128; verified by cmp — the build's label line failed on a relative path after `cd`, fixed in `p1_side_build.sh`). `LEAKTEST checked=95 mismatches=0 PASS`. Paired arms on fs1206500n1000 running. X1: the memory guard matched its own command line (`pgrep -f` self-match) and never released; killed at 20:53 after confirming no trainer was running (rc 143, operational only) — X1 scoring/training of vr_140-143 now running next to the P1 arms (the evals are deterministic and use little memory).
- **20:17 UTC: hourly check.** P1 stage 2b h128: ep1 val_ce 0.2449 (59%), ep2 0.2439 (61%) — already below h64's final 0.2441; ep3-4 due ~20:40. Then selection, side build, leak test, five paired arms (~1 h). X1 queue held at the memory guard; no new worker commits; p1w18 still awaits the owner. Disk 22 GB free.
- **19:17 UTC: hourly check.** P1 stage 2b: prior v4 h64 done — ep3 val_ce 0.2444 (60%), ep4 0.2441 (60% captured; uniform 0.2859, floor 0.2165), 6,128 s, weights `research/models/pfm_p1_h64.inc`. h128 training started ~18:58 (RSS 7.0 GB); then selection by val_ce, side build, leak test and the five paired arms on fs1206500n1000. X1 queue still held at the memory guard (resumes when the P1 trainer exits). No new worker commits; p1w18 still awaits the owner.
- **18:20 UTC: hourly check.** X1 data complete: x1w14 merged (`SV GEN rundata_x1_w14 DONE: 30000 files`; k32 957,539 rows, 2566.2 MB -> 57.7 MB (44.5x), full 12984.5 MB -> 179.4 MB (72.4x), both verify OK and pushed; four runner stops resumed); x1w13/14/15 archived. The coordinator queue pulled `rundata_x1_w14_k32_z` and now waits at a memory guard (added 17:28; operational only): X1 scoring/training of vr_140-143 starts once the P1 trainer exits, because both together would exceed the 15 GB VM. The queue runner now runs under setsid (no 2-hour stops). P1 stage 2b h64: ep1 val_ce 0.2460 (58% captured), ep2 0.2451 (59%), 26 min/epoch, RSS 7.1 GB. p1w18 still awaits the owner in its own session (not needed for 2b).
- **17:21 UTC: P1 stage 2b restarted after an out-of-memory kill (implementation fix only; data, arms, selection unchanged).** The h64 trainer was killed by the memory cgroup at 13.9 GB RSS (15 GB VM) after the val baselines. Cause: the loader's per-decision lists (views of every raw file, ~5 GB incl. object overhead) stayed alive next to the torch copies, and the full-validation forward materialises the 3.3M-action input (~6 GB). Fix in `research/vf/train_policy_mlp.py`: free the lists once the tensors exist; validation CE computed in 200k-decision chunks (same quantity). Check on 300 games, 1 epoch: exported weights byte-identical to the old trainer, val_ce 0.2680 both. `p1_stage2b.sh` gains `SKIP_UNPACK=1` (relaunched with it; the 8,004 unpacked games kept).
- **17:19 UTC: hourly check.** X1: x1w15 done and merged (`SV GEN rundata_x1_w15 DONE: 30000 files`; k32 subsample 957,558 rows, packed 2566.3 MB -> 57.7 MB (44.5x), full 13007.0 MB -> 179.8 MB (72.4x), both verify OK and pushed); session archived. x1w14 at SV chunk 271/300 at 16:55 (ETA ~17:45 + pack/push); the coordinator queue waits on `rundata_x1_w14_k32_z`. P1 stage 2b: 8,004 training games unpacked, 2,931,776 decisions / 16,306,899 actions; val baselines uniform CE 0.2859, target entropy 0.2165; h64 training in epoch 1. p1w18 still awaits the owner in its own session (not needed for 2b).
- **16:34 UTC: P1 stage-2 amendment 2 (before the full training run).** One-set trial (`poldata_p1_w16`, 4,000 games):
  `decisions=1459511 actions=8134094`; `ep 1 val_ce=0.2481 (uniform 0.2858, floor 0.2166, captured 54%) 606s` — already
  above v1 (49% captured after 8 epochs at 500-sim targets). Six sets ≈ 8.8M decisions: 8 epochs x 3 sizes would take > a
  day. Fixed now: training set = every 3rd game (sorted file list) of each of the six stored sets (≈ 8,000 games, ≈ 2.9M
  decisions); prior v4 candidates hidden 64 and 128, 4 epochs each; selection, evaluation slice (fs1206500n1000) and arms
  unchanged.
- **16:32 UTC: disk-full incident (≈ 15:38–16:20), no data lost.** P1 stage 2 pulled and unpacked six of the seven policy
  sets (w18 not stored: trained on six as amended), but `p1_side_build.sh` copied `research/vf` *including*
  `research/vf/data` into `/home/user/p1side/v4` (12 GB) and the volume filled; the three trainer runs had already failed
  with empty logs (disk full at 15:38). Freed: `/home/user/p1side` (copies), the unpacked `poldata_p1` (rebuilt from the
  local shard sets), the per-file `rundata` (4.1 GB; its byte-verified shard copy `rundata_z` stays), the Xet cache and
  download bookkeeping -> 25 GB free. Fix: the side build copies only code (`research/vf` top-level files, never `data/`).
  Re-running stage 2 by hand: one-set trial training to size memory/time before the six-set run.
- **14:22 UTC: P1 amendment (before any training):** p1w18 declines to restart its runner without the owner confirming in its
  own session (correct under its rules: a coordinator message is not owner authorization). Stage 2 now waits for the six
  other sets (24,000 games) and includes `poldata_p1_w18_z` only if it is in the store by then; nothing else changes
  (selection rule, evaluation slice fs1206500n1000, arms). p1_stage2.sh restarted with this change.
- **14:21 UTC:** P1 data 4/7 stored (p1w17 `PUSH poldata_p1_w17_z DONE: 5 files`; session archived); p1w16 and p1w21 in their
  last chunks (≈ 35–40 min). **p1w18 stalled 12:07–14:20** at 15/40 chunks: its runner hit the 2-hour limit and it waited for
  a restart OK it already had (its task carries the owner's approval) — reminded by message; ≈ 3 h left after restart, so P1
  stage 2 (which waits for all seven sets, as pre-registered) starts ≈ 17:30. X1: x1w13 `PUSH rundata_x1_hold_z DONE: 2
  files`, `PUSH rundata_x1_w13_k32_z DONE: 32 files` (ahead of 17:00); coordinator queue pulling as sets land.
- **13:22 UTC: P1 data 3/7 in the store** (`PUSH poldata_p1_w19_z / w20_z / w22_z DONE: 5 files` each; p1w20 4,000 games packed
  146.4 MB verify OK; p1w22 146.1 MB) — workers p1w19, p1w20, p1w22 archived; p1w16–18, p1w21 finishing. p1_stage2.sh
  restarted after its 2-hour stop (it waits; pulls/unpacks are idempotent). X1 ≈ 17:00.
- **12:20 UTC:** P1 workers ≈ 2/3 through generation (p1w22: 27/40 chunks at 11:59, after one resumed 2-hour stop) — data
  ≈ 13:30–14:00; X1 ≈ 17:00. Both coordinator pipelines alive (runq restarted 12:05 after its 2-hour stop; p1_stage2.sh
  waiting). p1w18 log merged.
- **11:21 UTC: P1 stage 2 pipeline tested and armed** (`research/tools/p1_stage2.sh`, standalone; waits for the seven
  `poldata_p1_w*_z` sets, ≈ 14:30 UTC). End-to-end smoke test on 20 fork games (pf_log, sims 200): trainer runs, side build
  (`research/tools/p1_side_build.sh`: copy of research/ with the candidate prior, own build dir on the fork) compiles, leak
  test `checked=88 mismatches=0 PASS`, the side build plays different games (prior live), `research/cpp/pfm_weights.inc`
  untouched. Pre-registered selection: prior v4 = the hidden size (64 / 128 / 256) with the lowest validation CE; evaluation
  slice allocated now: fs1206500n1000 (all arms paired on it): ref 500 sims (v1), v1 vs v4 at sims 50 depth 0, v1 vs v4 at
  sims 50 depth 2 c3.0. X1 runner waiting for its first shard set (≈ 17:00).
- **10:18 UTC:** all 10 workers past their gates (X1: three repro_check `OK bit-identical`; P1, e.g. p1w16: `built fair_eval
  with STS_SIM=fidelity (dfda16c ...)`, `LEAKTEST checked=94 mismatches=0 PASS`, 20 gate games 0 aborted); logs merged
  (x1w15 not yet pushed; its session reports 20/300 chunks at 09:54, ≈ 7 h left). Correction: X1 data lands ≈ 17:00 UTC,
  not 14:00 (A0 games at 500 sims are slower than the A20 rate I used). P1 data (2,000-sim targets, 4,000 games per worker)
  ≈ 4–5 h.
- **10:01 UTC: P1 stage 1 done (fidelity simulator, 200 train games, seeds 400000–400199, tuning only).** Wins / 200
  (wall): 500-sim best 159 (231 s); 50 sims: prior off 95 (≈ 20 s); depth 0: c1.0 112, c1.5 119, c3.0 112 (≈ 26 s);
  depth 1: 122, 122, 118 (≈ 48 s); depth 2: 124, 129, **133** (≈ 80 s). Same seeds on the legacy simulator this morning: 50
  sims prior off 109, prior depth 0 143; 500 sims prior off 166. Reading: the fork is a harder game, most at low search
  (−14 / −24 wins at 50 sims, −7 at 500); the prior is still worth +24 wins at 50 sims on the fork; using it deeper helps
  (+14 wins at depth 2, c 3.0) but costs ≈ 3x time, so at equal speed-up nothing is gained — at 8.9x faster (depth 0)
  the gap to 500 sims is 20 points, at 2.8x faster (depth 2) 13 points. Prediction "stage 1 closes ≥ 3 of the gap" (MED):
  held only at 3x the cost; at equal cost it failed. The gap is the prior's quality, so stage 2 (2,000-sim targets) is the
  real test. Stage-2 configuration fixed now: prior v4 evaluated at sims=50 with depth 0 (the cheap setting) and depth 2
  c 3.0.
- **09:47 UTC: Phase P1 pre-registered** (cheaper search via a better move prior; fidelity simulator; stage 1 here, stage 2
  on seven new workers). `setup.sh` takes `STS_SIM=fidelity` (default stays legacy until X1 ends); fork pinned in
  `research/patches/sts_lightspeed_fidelity.commit` (dfda16c327770b5d62f344eda1ef9cced3e01c17); `runstore.py` packs any
  file pattern (`--glob 'pol_*.bin'`) and unpacks (round trip byte-identical).
- **09:27 UTC: owner decision — finish X1 on the current simulator, then switch** the loop to the real-game-matched fork
  (`latioswang/sts_lightspeed` commit `dfda16c`, branch `fidelity`, pinned; relayed by the bridge session, confirmed by the
  owner in this chat: "finish X1 first"). Fork source cloned to `/home/user/sts_lightspeed_fid` at dfda16c; building it was
  refused by the auto-mode classifier (external code) — owner approval needed at switch time. Switch plan: pin dfda16c in
  `research/setup.sh` (no patch on top), build `fair_eval` with `-DSTS_RESEARCH_DIR`, leak test, mark results by simulator,
  re-baseline all four bests on fresh slices, then decide on retraining V on new-sim data. X1 coordinator queue written
  (waits for the exploration shard sets).
- **09:24 UTC: MILESTONE M-A10 CONFIRMED.** `a10_r81 test a10 n=500 crash=0 win=0.368 floor=38.03 score=1.1137±0.0313`:
  184/500 = 36.8%, 95% Wilson CI [32.7%, 41.1%]; criterion (point estimate ≥ 35%, declared 08:53 before the run) met. Second
  A10 test use (the first, `a10_relic20` 32.2%, did not confirm). Consistent with the fresh-slice 37.2% (1,000 seeds).
  Speed measurement (train seeds 300100–300199, A0 best, 4 threads; timing only): sims 500 -> 110.2 s / 100 games, 83 wins,
  floor 48.5; sims 50 -> 10.5 s, 62 wins, floor 44.6; sims 5 -> 1.8 s, 33 wins, floor 34.7 (search is ≈ 98% of game cost).
- **09:16 UTC: honest levels of all four bests (fresh slices, report only):** A0 `vr5relic20` 80.4% (500; selection 84.7%);
  A10 `a10_r81` 37.2% (1,000; 44.0%); A20 `a20_r121s32f` 6.35% (2 x 1,000); Heart `heart_relic20` 12.7% Heart wins (1,000:
  `heart_relic20 fs1204500n1000 win=0.127 floor=48.05 score=1.0220 act4=0.606`; selection 15.0%). All three "below"
  predictions held. X1 workers started: x1w13 session_01USafwhnZj2ip6UeUk7vDAf, x1w14 session_01USafwiyaHFCjBt4jskZjMV,
  x1w15 session_01USafwhAjgv5r44RMxUeZ66. The second M-A10 test run (`a10_r81`) is next in this VM's queue.
- **09:15 UTC: Phase X1 pre-registered** (exploration data for V; three A0 workers starting).
- **08:53 UTC: A10 best re-measured on a fresh slice:** `a10_r81` on fs1203500n1000: win 0.372 (372/1000; 95% Wilson CI
  [34.3%, 40.2%]), floor 37.42, score 1.1058 (selection-slice val 44.0%; prediction "below" held). **New M-A10 claim declared
  now, before any test game:** `a10_r81` meets the preconditions of the first claim (val 44.0% ≥ 35%, dev replication +0.112
  P=0.952) plus an honest fresh-slice 37.2%; the rule allows one test run per milestone claim (the first was spent on
  `a10_relic20`: 32.2%, not confirmed). Test run: 500 test seeds, A10, report with 95% Wilson CI; confirmed iff the point
  estimate ≥ 35% (same criterion as before); queued after the Heart re-measurement; not used for any decision.
- **08:42 UTC: `a20_r121s32f` keep REPLICATES** on a second fresh slice fs1205500n1000: `a20_relic20` win 0.050 floor 28.60
  score 0.6098; `a20_r121s32f` win 0.065 floor 29.27 score 0.6377; dscore +0.0279, dwin +0.015, P=0.993. Across the two
  fresh slices (2,000 seeds): 6.35% vs 4.80% win. **A20 honest level ≈ 6.3%** (target 25%). A0 best re-measured on
  fs1203000n500: `vr5relic20` win 0.804 floor 48.13 score 1.7476 (selection-slice val 84.7%; prediction "below" held).
  A10 and Heart re-measurements running.
- **08:19 UTC: A20 KEEP under the fresh-slice rule — `a20_r121s32f`** (A20 best + vr_model=121, vr_bonus=32, vr_relic_w=32)
  on fs1202000n1000: base `a20_relic20` win 0.046 floor 27.93 score 0.5926; challenger win 0.062 floor 28.31 score 0.6158;
  dscore +0.0232, dwin +0.016, P=0.972 -> **new A20 best**. The same config was discarded as R8b on the selection seeds
  (−0.014, P=0.204): a false discard caused by the inflated base. Prediction (LOW) failed. Replication on a second fresh
  slice (fs1205500n1000, run.py vs `a20_relic20`, no promotion) queued first, then the best re-measurements.
- **08:16 UTC:** first fresh-slice decision: `a20_bm15f` on fs1201000n1000 — base `a20_relic20` win 0.049 floor 28.48;
  challenger dscore −0.0131, dwin −0.010, P=0.097 -> discard (on the selection seeds it read −0.026 / −1.8 points: about half
  of that was the base's selection gain). `exp_fresh.sh` fixed (BrokenPipe from `head -1` killed the first attempt after the
  base run; FRESH_SLICE resumes an allocated slice). `a20_r121s32f` running on fs1202000n1000.
- **07:50 UTC: fresh-slice keep protocol adopted** (see "PROTOCOL AMENDMENT"); two A20 re-tests queued under it.
- **07:49 UTC: SELECTION BIAS CONFIRMED (A20 diagnostic, 1,000 fresh seeds 1,200,000–1,200,999, report only).** Pasted:

      a20_relic20_fresh  fresh a20 n=1000 win=0.044 floor=28.88 score=0.6094±0.0119   (pooled-500 selection seeds: 0.056 / 28.93)
      a20_vr2b20_fresh   fresh a20 n=1000 win=0.043 floor=28.27 score=0.5965±0.0116   (pooled: 0.036 / 28.60)
      a20_bm2x_fresh     fresh a20 n=1000 win=0.045 floor=28.96 score=0.6120±0.0120   (pooled: 0.026 / 28.51)
      paired: relic20 vs vr2b20 dscore +0.0128 dwin +0.001 P=0.975; relic20 vs bm2x dscore −0.0027 dwin −0.001 P=0.355

  All three predictions held: `a20_relic20` 5.6% -> 4.4% on fresh seeds; the relic gain vs `a20_vr2b20` shrinks from +2.0 win
  points to +0.1 (score +0.013 still P=0.975: the keep is real but is a floor gain); `a20_bm2x`, "−3.0 points, P=0.005" on
  the selection seeds, is a dead heat on fresh seeds. **Consequence: every comparison against a best on the seeds that
  selected it is biased against the challenger (and every kept number is inflated)** — on A20 by ≈ 1–3 win points; the A10
  val -> test drop (38.7% -> 32.2%) has the same signature. Today's A20 discards B20/B20b/R8/R8b were judged against an
  inflated base; their fresh-seed effects are unknown except `a20_bm2x` (≈ 0). Protocol amendment below.
- **07:23 UTC: B20b closed (both lose: bm05 2.4%, s400 3.8% vs 5.6%); selection-bias diagnostic on 1,000 fresh A20 seeds
  pre-registered and queued** (report only).
- **07:17 UTC: Phase B20b pre-registered and queued** (A20 with less search: bosses x0.5; sims 400).
- **06:30 UTC: Phase B20 closed — no keep; extra boss search does not help A20.** Pasted (pooled val,dev2,dev3, n=500 vs
  `a20_relic20` win 0.056 floor 28.93): `a20_bm15` win 0.038 floor 28.51 dscore −0.0260 P=0.033; `a20_bm2` win 0.030 floor
  28.38 dscore −0.0364 P=0.003; `a20_bm2x` (+compute, informational) win 0.026 floor 28.51 dscore −0.0378 P=0.005. Boss-floor
  deaths: base 186, bm15 177, bm2 175, bm2x 184. Summed secs bm15 766, bm2 782, bm2x 1080 (= 1.38x bm2, matching the +47%
  calibration); the base's logged 465 s is from an earlier, faster session (this session's calibration: 1.57 s/game vs the
  base's recorded 0.93) so neutrality vs the base is shown only through the calibration. Predictions: bm15 LOW–MED (failed),
  bm2 LOW (held), bm2x positive MED (failed: more boss search at no cost elsewhere still lost 3 points). Reading: at 500 sims
  the A20 boss fights are not search-limited — the losses come from the deck/HP the run arrives with [INFERRED, MED]; the
  extra-search result is unexplained (a pathology at high simulation counts is possible; not investigated). Runner idle.
- **06:18 UTC: Phase B20 pre-registered and queued** (A20: search budget moved to boss fights, compute-neutral; boss fights
  are 39% of A20 losses and 47% of its search compute).
- **05:38 UTC: Phase R10 closed — no keep.** `a0_r121` dscore +0.0014, dwin +0.000, P=0.505; `a0_r121s` (scale-matched)
  dscore −0.0113, dwin −0.007, P=0.340 (vs `vr5relic20`) -> both discard. Predictions held (LOW–MED / LOW). Second failed test
  of the scale idea (after R8b): it is dropped. Pattern across R4, R7 (A20), R8, R10: a better holdout fit does not by itself
  improve play; the one exception (`a10_r81`) came with the relic hook. Runner idle.
- **05:28 UTC: Phase R10 pre-registered and queued** (A0 with the mixed-data `vr_121`, inherited and scale-matched weights);
  fresh-A0 fit table in "Phase R10".
- **05:16 UTC:** `rundata` pulled (`PULL rundata DONE: 10000 files`) and packed (`PACK ... 4233.9 MB -> 58.2 MB (72.7x), verify
  OK`). Scoring on it OOM'd (rc 137) — `score_vr.py` now scores in 200k-row chunks (A10 holdout check reproduces 0.7093 /
  0.472). `rundata` is in-sample for `vr_81` / `vr_121` (their A0 subsample covers all 10k runs), so a fresh A0 holdout is
  being generated (1,000 games, seeds 1,100,000–1,100,999, unused; 3/10 chunks at 05:16) for an uncontaminated comparison.
- **04:17 UTC:** idle 01:26–04:16 (the session's worker process restarted; the 02:15 and 03:16 wakes were delivered at
  04:16). Now pulling the A0 holdout `rundata` back (deleted locally on 11-03 for disk), packing it to shards (`rundata_z`)
  and scoring `vr_41` / `vr_61` / `vr_81` / `vr_121` on it (informational; decides whether an A0 cell with `vr_121` is worth
  pre-registering).
- **01:26 UTC: Phase R9 closed — no keep.** `vr9srel` dscore −0.0046, dwin −0.003, P=0.343; `vr9srelg` dscore +0.0026, dwin
  +0.003, P=0.559 (vs `vr5relic20`) -> both discard. Predictions LOW–MED / LOW held (no effect either way). Shop relics are
  a small decision at A0 (few shops with two affordable relics) [INFERRED, LOW]. Runner idle.
- **01:17 UTC: Phase R9 pre-registered and queued** (run value on shop relics; two A0 cells vs `vr5relic20`).
- **00:45 UTC: R8b — no keep; the A20 learned run value line is closed for now.** `pooled val,dev2,dev3 [act3]: n=500  a20_r121s32 win=0.050 floor=28.52  a20_relic20 win=0.056 floor=28.93  dscore=-0.0140 dwin=-0.006 P(improve)=0.204` (slices −0.011 / −0.019 /
  −0.017). Scale-matching moved the loss from −0.037 to ≈ −0.015 but not above zero: the weakened pull explains part of R8,
  not all. Prediction (LOW) held. A20 best stays `a20_relic20` (with the A0-trained `vr_41`). Runner idle.
- **00:42 UTC: Phase R8 — no keep** (`a20_r121` pooled 500: 3.6% vs 5.6%, P=0.009; fits pasted in "Phase R8 results");
  R8b (scale-matched weights, one cell) pre-registered and queued.
- **00:16 UTC: all R8 data in the store; workers archived.** Pasted: r8w10 `SV GEN rundata_a20_w10 DONE: 30000 files`, holdout
  file count 1000 (seeds 1000000–1000999), `PUSH rundata_a20_hold_z DONE: 2 files`, `PUSH rundata_a20_w10_k32_z DONE: 32
  files`, `PUSH rundata_a20_w10_z DONE: 31 files`; r8w12 `PUSH rundata_a20_w12_z DONE: 31 files`. Sessions r8w10, r8w12
  archived (r8w11 at 23:17); no active workers. Coordinator training resumed at cell 102.
- **00:09 UTC: R8 training under way** (container restarted ≈ 00:05 during cell 102; runner restarted, it resumes). Pasted:

      PULL rundata_a20_hold_z / w10 / w11 / w12 _k32_z DONE (2 / 32 / 32 / 32 files)
      SCORE vr_61 on rundata_a20_hold_z: runs=1000 states=93746 rmse=0.8800 corr=0.437 state_mean=0.737
      SCORE vr_81 on rundata_a20_hold_z: runs=1000 states=93746 rmse=0.4206 corr=0.545
      VR2 id=100 runs=80100 ridge lambda=0.01 holdout_rmse=0.3452 holdout_corr=0.579 predict_mean_rmse=0.4233
      VR2 id=101 runs=80100 hidden=64 best_epoch=1 holdout_rmse=0.3560 holdout_corr=0.546 predict_mean_rmse=0.4233

  The A0 model mis-levels A20 badly (0.880, worse than predicting the mean 0.423); the A10 model is about at the mean
  (0.421); A20-only ridge 0.345 — prediction 1 (A20-only beats both) holds.
- **23:17 UTC: R8 data arriving.** Pasted: r8w11 `SV GEN rundata_a20_w11 DONE: 30000 files`; `SUBSAMPLE ... 30000 runs, rows
  2859991 -> 942249 (k=32)`; `PACK ... rundata_a20_w11_k32_z: 30 shards, 2525.2 MB -> 46.9 MB (53.9x), verify OK`; `PUSH
  rundata_a20_w11_k32_z DONE: 32 files`; `PACK ... rundata_a20_w11_z: 7664.8 MB -> 101.9 MB (75.2x), verify OK`; `PUSH
  rundata_a20_w11_z DONE: 31 files` (session archived). r8w12: `SV GEN ... DONE`, `SUBSAMPLE ... rows 2861950 -> 942376`,
  `PUSH rundata_a20_w12_k32_z DONE: 32 files`; full set pushing. r8w10 (holdout) still running. Coordinator waits for
  `rundata_a20_hold_z`.
- **22:16 UTC:** R8 generation halfway (r8w10 at 150/300 chunks at 21:57 -> ends ≈ 23:40, then pack + push); coordinator
  queue waiting for `rundata_a20_hold_z` (runner alive).
- **21:16 UTC:** R8 workers past their gates (r8w11 log: three repro_check `OK bit-identical`, store lists `rundata_w1_k32_z`)
  and generating A20 games (r8w12 at 56/300 chunks at 20:54 -> generation ends ≈ 23:00). Coordinator queue waiting for
  `rundata_a20_hold_z`.
- **20:18 UTC: Phase R8 started** (pre-registered 20:16 UTC): workers r8w10 session_01USafwkB4NY1U4Sjop31yUk (also the A20
  holdout), r8w11 session_01USafwg72vBSu3xsVLMTaFH, r8w12 session_01USafwgGyDBXGvSYd6uMw5A, each 30,000 A20 games shipped
  as shards; this VM's queue waits for the four shard sets, then trains ids 100–104 -> `vr_121` and runs `a20_r121`.
  Open owner question: delete the store's per-file sets that now have `_z` shard twins (≈ 23 GB).
- **20:01 UTC: all 11 shard sets are in the store** as `<name>_z` (`ALL SHARD SETS PUSHED`; each count-verified). The store's
  per-file copies of the same data (`rundata_a10_*`, `rundata_w1..w6_k32`, `rundata_k32` was never there) are now redundant;
  deleting them is the owner's decision (asked).
- **19:21 UTC: Phase R7 results.** `a10_r81` (A10 best + vr_model=81) val-300 win 0.440, floor 39.95, dscore +0.0584, dwin +0.053,
  P=0.942 vs `a10_relic20` -> **KEEP (new A10 best)**; dev replication `a10_r81_dev`: win 0.340, dscore +0.1116,
  P=0.952 (info). `a20_r81` pooled val,dev2,dev3: n=500 win 0.040 floor 28.06 vs `a20_relic20` win 0.056 floor 28.93,
  dscore −0.0328, dwin −0.016, P=0.020 -> discard (slices −0.030 / −0.025 / −0.050). Predictions: A10 LOW–MED (held), A20
  LOW (held). Caveat on A10 val numbers: the M-A10 test showed val ≈ 6.5 points above test for the previous best.
- **19:16 UTC: Phase R7 pre-registered and queued** (A10/A20 bests with the A10-trained `vr_81`). Shard upload: 8 of 11 sets
  verified in the store (incl. the local-only `rundata_k32`, `rundata_w3_k32`); restarted 19:12 with skip markers for w4–w6.
- **18:26 UTC: Phase R6 closed — no keep.** `vr6rest` dscore −0.0290, dwin −0.023, P=0.089; `vr6rm` dscore −0.0554, dwin
  −0.047, P=0.018; `vr6relic10` dscore −0.0034, dwin −0.003, P=0.000 (vs `vr5relic20`) -> all discard. Predictions: rest
  LOW–MED (failed), removal LOW (held), relic10 LOW (held). Reading: the run value helps where the KB prior is weak and the
  choice is between different *items* (relics, cards); replacing hand rules for HP vs upgrades and removal order hurts —
  V is trained on the agent's own outcomes under those rules and is least reliable for HP trade-offs [INFERRED, LOW–MED].
- **18:17 UTC: Phase R6 pre-registered and queued** (run value on rest sites and removals, relic weight 10; A0 vs `vr5relic20`).
  Shard upload continues (2 of 11 sets confirmed at 17:45).
- **17:46 UTC: Phase R5 ports — KEEP on all three tracks; M-A10 NOT confirmed on test.** Pasted:

      a20_relic20 pooled val,dev2,dev3 [act3]: n=500 win=0.056 floor=28.93 vs a20_vr2b20 win=0.036 floor=28.60
                  dscore=+0.0262 dwin=+0.020 P=0.999 -> KEEP (slices +0.033 / +0.009 / +0.024)
      heart_relic20 pooled val,dev2,dev3 [heart]: n=500 win=0.150 floor=48.22 vs heart_vr2b20 win=0.148 floor=47.56
                  dscore=+0.0132 dwin=+0.002 P=0.904 -> KEEP (slices +0.020 / +0.005 / +0.001: marginal, mostly floor)
      a10_relic20_dev: dscore +0.0249, dwin +0.000, P=0.735 (info; > 0, so the declared test run ran)
      TEST a10_relic20 (500 test seeds, A10): 161/500 = 32.2%, 95% Wilson CI [28.3%, 36.4%], floor 37.80, 0 aborted

  The val 38.7% did not hold on test (32.2%; the CI includes 35% but the point estimate is below): **M-A10 (≥ 35%) is not
  confirmed.** Likely cause: selection on the val seeds — the A10 val split has been used for ~30 keep decisions, so the
  best-of-many val number is biased upward [INFERRED, MED]; the A10 dev slice also runs low (26–27%). The M-A10 test run is
  spent; the next A10 test run needs a new milestone claim under the same rule. Predictions: A10 keep MED-LOW (held),
  A20 keep LOW (beaten), Heart keep LOW–MED (held, marginal).
  Bests now: A0 `vr5relic20`, A10 `a10_relic20`, A20 `a20_relic20`, Heart `heart_relic20`.
- **17:34 UTC: A10 KEEP `a10_relic20`** (boss relics + 20 x run-value gain, `vr_41`): val-300 win 0.387, floor 39.69, dscore
  +0.0681, dwin +0.043, P=0.999 vs `a10_vr2b20` -> new A10 best; **38.7% ≥ the 35% A10 target on val.** Prediction (MED-LOW)
  held. **M-A10 sequence declared now, before any further game:** dev replication (`rep_dev.sh`); if the dev dscore > 0,
  the one test run allowed per confirmed milestone (500 test seeds, report with 95% Wilson CI; not used for any decision).
- **17:32 UTC:** R5 A0 cells done (relic20 KEEP and replicated on dev +6.0; relic40, shop, upgrades discard). Ports of the
  relic keep to A10 / A20 / Heart pre-registered and queued (see "Phase R5 results and ports").
- **R5 first cell: KEEP.** `vr5relic20` (boss relics: KB value + 20 x run-value gain) val-300 win 0.847, floor 48.38,
  dscore +0.0286, dwin +0.023, P=0.951 vs `vr3b40` -> **new A0 best**. Prediction (LOW) failed in the good direction.
  Remaining R5 cells now run against `vr5relic20` (standard greedy order: exp.sh compares with the current best and inherits
  its params, so `vr5shop` / `vr5upg40` test their hook on top of the relic keep; `vr5relic40` tests W=40 vs W=20). Dev
  replication of the keep queued (informational). Ports to A10/A20/Heart after the A0 cells.
- **17:18 UTC: Phase R5 pre-registered and queued** (run value on boss relics, shop cards, upgrades; four A0 cells vs
  `vr3b40`). Shard upload (`push_shards.sh`) still running.
- **17:14 UTC: all 11 local run-data sets are shards** (`shard_local.sh`: pack, byte-verify, swap; e.g. `SWAPPED
  rundata_w6_k32 -> shards (58M)`); `research/vf/data` 2.2 GB, disk 26 GB free (was 1.5 GB). `push_shards.sh` uploads
  them as `<name>_z` with a count check (backs up the local-only `rundata_k32`, `rundata_w3_k32`).
- **17:08 UTC: compressed run-data shards (`research/vf/runstore.py`) — bit-identity shown, local data converting.**
  Format: `index.tsv` (name, shard, offset, nbytes) + `shard_NNNNN.z` (zlib-6 of up to 1,000 consecutive run files, by
  name); readers (`train_vr2.py`, `score_vr.py`, `subsample_runs.py`) call `runstore.list_runs / size / read`, which mirror
  `sorted(glob)`, `os.path.getsize`, `np.fromfile` for either layout. Pasted:

      PACK rundata_a10_hold:   1000 files, 1 shard,   348.2 MB -> 4.8 MB  (72.5x), verify OK
      PACK rundata_a10_w7_k32: 30000 files, 30 shards, 2549.8 MB -> 54.0 MB (47.3x), verify OK   (w8, w9: same)
      SCORE vr_61 on rundata_a10_hold/sh: rmse=0.7093 corr=0.472 state_mean=1.216   (identical to per-file)
      ridge (cell 70 from shards, id 90): model file and meta byte-identical to vr_70
      STS_DEBUG_HASH: arr, runs, trn, val, hold, it, iv, ih, wrow, mu, sd — all identical across layouts
      h=64 (cell 71 from shards, id 91): NOT identical (0.5554 vs 0.5576); per-file retrain (id 92) == vr_71 exactly;
      shards with the decompressed-shard cache freed after loading (id 93) == vr_71 exactly

  Finding: the net fit depends on memory layout (an extra 85 MB cache alive during training changed the result by 0.002
  RMSE with identical inputs and seeds) — float-level nondeterminism of the CPU kernels, reproducible only for an
  identical allocation pattern. So R4 cell gaps of ~0.002 (71 vs 70, 0.5576 vs 0.5569) are within this noise.
  `runstore.release()` after loading keeps old fits reproducible. Test models 90–93, 99 deleted.
  Local conversion (`research/tools/shard_local.sh`: pack, byte-verify, swap into `all/`, delete per-file copy) running;
  then push the shard sets to the store under new names `<name>_z` (`rundata_k32`, `rundata_w3_k32` exist only here).
  Deleting the store's per-file copies stays the owner's decision.
- **16:50 UTC: Phase R4 closed — no keep.** Extension of `a10_vr4b20`: dev `a10_vr4b20_dev` 26.0% win, floor 36.82, +0.0147,
  P=0.593 (info); `pooled val,dev [act3]: n=400  a10_vr4b20 win=0.338 floor=38.31  a10_vr2b20 win=0.325 floor=37.71
  dscore=+0.0243 dwin=+0.013 P(improve)=0.802` -> discard. Prediction "A10 card-pick keep (LOW–MED)" failed. Three A10
  run-value variants in a row came out positive and unresolved (pooled 400 P = 0.870, 0.855, 0.802; +1.3 to +1.8 pts win):
  consistent with a small real effect below what 400 games resolve, or with noise; post hoc, so it is not evidence for a keep
  and the rule is not changed for these results. Fit scaling: more A10 data barely helps (0.548 -> 0.544 at 2x); the A0 data
  helps more than doubling A10. Runner idle (queue empty).
- **Next:** the compressed-shard data format (decided 13:16 UTC), then the next research phase.
- **16:48 UTC: R4 fits and first card-pick test.** Pasted:

      SCORE vr_61.txt on rundata_a10_hold: runs=1000 states=129910 rmse=0.7093 corr=0.472
      VR2 id=70 runs=80100 ridge holdout_rmse=0.5569 holdout_corr=0.508 predict_mean_rmse=0.6468
      VR2 id=71 runs=80100 hidden=64  best_epoch=1 holdout_rmse=0.5576 holdout_corr=0.510 predict_mean_rmse=0.6468
      VR2 id=72 runs=80100 hidden=256 best_epoch=1 holdout_rmse=0.5623 holdout_corr=0.503 predict_mean_rmse=0.6468
      VR2 id=73 runs=251100 hidden=64  best_epoch=2 holdout_rmse=0.5494 holdout_corr=0.535 predict_mean_rmse=0.7521
      VR2 id=74 runs=251100 hidden=256 best_epoch=1 holdout_rmse=0.5442 holdout_corr=0.539 predict_mean_rmse=0.7521
      VR SELECT: vr_74 -> vr_81; VR GATE OK: 0.544239 < 0.752132; LEAKTEST checked=87 mismatches=0 PASS
      a10_vr4b20 val-300: win 0.363 vs 0.343, floor 38.81, dscore +0.0275, P=0.785 -> discard (near miss)

  Predictions: A10-only beats `vr_61` on the A10 holdout — yes (0.557 vs 0.709); A0 + A10 beats A10-only — yes (0.544 vs
  0.557). Doubling A10 data (R4-local 40k -> 80k runs) moved the A10-only cells ≤ 0.011 and the best cell 0.548 -> 0.544.
  **Near-miss extension of `a10_vr4b20` declared 16:48 UTC, before any dev seed** (dev 100, pooled 400; keep needs pooled
  P >= 0.9 unrounded and dev-alone dscore > 0); queued after `a10_vr4b40` (running).
  `a10_vr4b40` val-300: win 0.347 vs 0.343, dscore +0.0130, P=0.639 -> discard (below the 0.7 near-miss band; no extension).
- **16:17 UTC:** runner stopped 16:15 at its 2-hour limit mid w9 pull (≈ 85% on disk) with 524 MB free: the Xet chunk cache
  had grown to 1.1 GB during the pull; deleted it (cache only) -> 1.6 GB; w9 bookkeeping kept so the resumed pull skips stored
  files; runner restarted 16:15; disk monitor armed (< 400 MB). Lesson for the shard format: per-file pulls cost bookkeeping
  (≈ 0.38 GB per 30k files) plus cache, on top of the data.
- **15:20 UTC:** w8 pulled (30,001 files; 50/50 sampled files read as whole 670-float rows); w9 pull running. Disk was 2.6 GB
  free against ≈ 2.9 GB per pulled set: deleted the download bookkeeping `_hf/.cache` of w7 and w8 (30,001 `.metadata` +
  30,001 `.lock` files each, no data) -> 3.3 GB; a queue line after the w9 pull removes its bookkeeping and the Xet chunk
  cache (≈ 0.75 GB) before training.
- **14:19 UTC:** old local-only sets moved to the store (`gen0` 1,500 / `gen1` 1,500 / `svdata` 2,500 files, counts match;
  local copies freed). Runner restarted 14:14 after its 2-hour stop, mid w8 pull (resumes; stored files are re-checked).
  Then the w9 pull and R4 training (ids 70–74).
- **13:18 UTC:** `rundata_a10_w7_k32` pulled (30,001 files); w8 pull starting; old-data push (gen0/gen1/svdata) running.
- **Next infrastructure step (decided 13:16 UTC, after R4 training):** compressed shards. Measured on 300 files each:
  full rows zlib-6 72.7x, lzma 284.9x; k32 subsample zlib 46.8x, lzma 117.3x; sparse (index+value) ≈ 9.8x; column-major + zlib
  25x (worse than row-major: the redundancy is zeros + near-identical consecutive states). zlib decompresses ≈ 500 MB/s.
  Plan: 1,000 runs per zlib shard (stdlib only), written at subsample / push time (the C++ logger is unchanged); trainer,
  scorer, subsampler and data_sync read shards; switch only after a model retrained from shards is bit-identical to its
  per-file original; migrating the store (and deleting its per-file copies) is the owner's decision.
- **13:15 UTC:** R4-local subsamples l0–l5 stored (`LOCAL A10 PUSHED AND FREED`, counts 5,002 each); `rundata_a10_w7_k32`
  pull at 95%. Disk 4.3 GB free but the w8 + w9 subsamples need ≈ 4.9 GB, so `research/tools/push_old_local.sh` moves the
  local-only Phase A/B/S sets `gen0`, `gen1`, `svdata` (≈ 1.7 GB) to the store (count-verified) and frees them; a queue guard
  holds the w9 pull until then.
- **12:20 UTC: all R4 data in the store; all workers archived.** Pasted from the logs: r4w7 `SV GEN rundata_a10_w7 DONE:
  30000 files`, `SUBSAMPLE ... 30000 runs, rows 3889335 -> 951436 (k=32)`, holdout file count 1000, `PUSH rundata_a10_hold DONE:
  1000 files`, `PUSH rundata_a10_w7_k32 DONE: 30001 files`, `PUSH rundata_a10_w7 DONE: 30000 files`; r4w9 `PUSH
  rundata_a10_w9_k32 DONE: 30001 files`, `PUSH rundata_a10_w9 DONE: 30000 files`. Sessions r4w7, r4w9 archived (r4w8 at
  11:47); no active workers. R4 queue (runner restarted 12:14 after its 2-hour stop) pulls the w7 rest next; R4-local
  subsamples l0–l3 stored, l4–l5 going, then the local copies are freed and the w8 / w9 pulls run.
- **11:47 UTC: r4w8 done** (pasted from its log): `SV GEN rundata_a10_w8 DONE: 30000 files`; `SUBSAMPLE ... rundata_a10_w8_k32:
  30000 runs, rows 3872106 -> 951209 (k=32)`; `PUSH rundata_a10_w8_k32 DONE: 30001 files`; `PUSH rundata_a10_w8 DONE: 30000
  files`. Session archived. `rundata_a10_w7_k32` at 24,000/30,001; R4-local l0–l2 stored, l3 going.
- **11:18 UTC:** uploads under the Hub's commit rate limit (HTTP 429, retried): `rundata_a10_w7_k32` 18,000/30,001 in the
  store; R4-local subsamples l0–l2 stored (`PUSH rundata_a10_l2_k32 DONE: 5002 files`), l3–l5 going. R4 queue waiting on
  w7. No new worker commits.
- **10:27 UTC: store freed.** `ALL DELETED 178430`, `usedStorage GB 22.6` (from 99.8). Integrity check of what was kept:
  `VERIFY 27/27 files download with matching sha256` (two random files of every data set, plus the three shortest
  `rundata_w5_k32` runs, the kind whose object was shared with a deleted full file). Worker uploads started (usage 23.5 GB at
  10:26 and rising).
- **11:15 UTC: finished worker sessions archived** (owner request): r2w1, r2w2 (their last log commits merged first), r3w4,
  r3w5, r3w6, a10ports, s2scale. Active: r4w7, r4w8, r4w9 (uploading).
- **10:30 UTC: local disk for R4.** `research/tools/push_local_a10.sh` uploads the six R4-local subsamples to the store,
  checks each set's file count there, then deletes the local copies (+2.4 GB); a guard line in the R4 queue holds the
  w8 / w9 pulls until it is done (3.7 GB free now; the three A10 subsamples need ≈ 6 GB).
- **10:10 UTC: store cleanup (owner-approved in chat, 2026-11-04 ≈ 10:00 UTC: "那就删除吧").** Deleted the six full A0 sets
  `rundata_w1..w6` from the HF store (each checked first: its `_k32` copy has 30,001 files; the full sets were used only to
  build those subsamples), then squashed the store's history. Kept: `rundata` (A0 full-row holdout), `leafdata`, all `_k32`
  sets, `rundata_a10_hold`, `rundata_a10_w7_k32` (partial). Used storage still reads 99.8 GB after the squash (a 1 MB probe
  upload succeeded); waiting for the store to account the freed space before the R4 workers re-push.
- **10:21 UTC:** store usage falling (99.8 -> 73.7 GB at 10:20) while the orphaned objects are deleted. All three R4
  workers report their A10 data intact (r4w7 subsample 30,001 files; r4w9 full 9.9 GB + subsample 2.4 GB; r4w8 intact) and
  push automatically once usage < 90 GB (subsample first, then full).
- **10:19 UTC: freeing the store for real.** Deletes + squash left used storage at 99.8 GB (the objects stay). Listing
  the store's LFS objects: 386,926 total; the six deleted full sets hold 178,659 (77.8 GB). 229 of those are byte-identical
  to files of the kept subsamples (runs with ≤ 32 states: the subsample is the whole file; same object) and are kept — found
  by matching every candidate's oid against the 208,483 oids of current files. The other 178,430 (77.7 GB) are being
  permanently deleted (`permanently_delete_lfs_files`, no history rewrite; owner approval as above). Worker r4w8 confirmed
  its data intact and polls the counter before pushing.
- **10:15 UTC: R4 queue unparked** (it waits for each complete A10 subsample, then pulls and trains ids 70–74 -> `vr_81`).
  Workers r4w7–r4w9 told (information) that space was freed; they push once the store's counter drops. Local disk: the
  three A10 subsamples still to pull need ≈ 6 GB, 3.7 GB free; plan: once the store
  accepts writes, push the R4-local subsamples `rundata_a10_l0..l5_k32` there (keeps them) and delete them locally (+2.4 GB).
- **R4-local closed:** `a10_vr4lb20` discard; `a10_vr4lb40` near miss, extension pooled 400 P=0.855 -> discard.
- **09:57 UTC:** `a10_vr4lb40` near miss (37.0% vs 34.3% val, P=0.856); pre-registered extension (dev 100, pooled 400) running.
- **09:57 UTC: R4-local fits done; `a10_vr4lb20` discard; `a10_vr4lb40` running.** A10-trained run value fits the A10 holdout
  far better than the A0 model (RMSE 0.548 vs 0.709), but at bonus 20 the card picks do not move (dscore −0.006, P=0.437).
- **09:16 UTC:** R4-local blocks 0–4 done (25,000 games; `SUBSAMPLE ... rundata_a10_l4 ... 5000 runs, rows 643127 -> 158469`),
  block 5 at 17/50 (ends ≈ 09:26), then score `vr_61` on the A10 holdout and training cells 75–79. Runner restarted 08:23
  after its 2-hour stop (resume, nothing lost). Disk 3.1 GB free. R4 workers idle; HF decision pending.
- **08:17 UTC:** R4-local blocks 0–2 done (15,000 games; `SUBSAMPLE ... rundata_a10_l2 ... 5000 runs, rows 649776 -> 158425`),
  block 3 at 21/50; generation ends ≈ 09:35, then the training cells 75–79. Disk 3.9 GB free (low point ≈ 1.7 GB during
  block 5). R4 workers idle, no new commits; HF decision pending.
- **07:18 UTC:** R4-local block 0 done (`SV GEN rundata_a10_l0 DONE: 5000 files`; `SUBSAMPLE ... 5000 runs, rows 641807 ->
  158576 (k=32)`; full rows deleted), block 1 at 24/50 chunks (~33 min per block, generation ends ≈ 09:50). Partial
  `rundata_a10_w7_k32` pulled: 15,000 runs, and its `counts.tsv` arrived (30,000 lines; the trainer weights only runs
  present), so by the pre-registered rule it joins the training dirs. R4 workers idle, unchanged; HF decision pending.
- **06:23 UTC: Phase R4-local started** (pre-registered above): 30,000 A10 games generated on this VM in blocks of 5,000,
  subsampled as they finish; pull of the partial `rundata_a10_w7_k32` alongside. R4 proper still blocked on the HF storage
  quota; workers r4w7–r4w9 idle with their data only on their VMs (merged their logs).
- **04:17 UTC:** still blocked on the Hugging Face storage quota; all three R4 workers idle with their A10 data unpushed.
- **03:20 UTC: R4 BLOCKED — Hugging Face private storage full** (workers r4w7, r4w8 report it; r4w7's A10 subsample is at
  15,000/30,001 files; r4w8's data is only on its VM). Coordinator runner stopped (resume marker intact) so it cannot train
  on a partial pull. Awaiting the user: upgrade the HF plan, or authorise deleting the full (non-subsampled) R2/R3 sets
  `rundata_w1..w6` (regenerable from seeds; no planned step uses them) and squashing history.
- **02:16 UTC:** R4 workers generating A10 games; this VM's R4 training queue waits for the A10 holdout and subsamples
  (container restarted ≈ 01:45, runner restarted, nothing lost).
- **01:16 UTC:** R4 workers generating A10 games; this VM's R4 training queue waits for the A10 holdout and subsamples.
- **00:16 UTC:** R4 workers generating A10 games; this VM's R4 training queue waits for the A10 holdout and subsamples.
- **22:18 UTC: Phase R4 started** — three workers generating 90,000 A10 games (A10 run value). Bests: A0 `vr3b40`,
  A10 `a10_vr2b20`, A20 `a20_vr2b20`, Heart `heart_vr2b20`.
- **22:07 UTC: R3 closed.** A0 best `vr3b40` (pooled 400 80.5%, P=0.900 vs `vr2b20`). Ports of the R3 model: A10 near-miss ->
  extension discard (pooled 400 P=0.870), A20 discard (P=0.604), Heart discard (P=0.148). Other tracks keep the R2 model.
  Main queue empty; all workers finished.
- **21:49 UTC: A0 best `vr3b40`** (R3 run value, bonus 40) via the near-miss extension: pooled 400 80.5% vs 78.0%
  (`vr2b20`), P=0.900 — at the bar. Ports to A10 / A20 / Heart not yet run.
- **21:17 UTC:** R3 cell 53 (h=1,024, patience 3) at 60 min, expected ≈ 21:30; then control 54, selection, leak test, card picks.
- **19:16 UTC: R3 training,** cells so far (pasted):
  `VR2 id=50 runs=170100 ridge lambda=0.001 holdout_rmse=0.5141 holdout_corr=0.341 predict_mean_rmse=0.5475 99s`
  `VR2 id=51 runs=170100 hidden=64 layers=2 params=46913 best_epoch=1 holdout_rmse=0.5019 holdout_corr=0.397 predict_mean_rmse=0.5475 184s`
  Note: predict-mean RMSE moved 0.5451 -> 0.5475 because the R3 runs come from the stronger `vr2b20` agent (higher mean
  score) while the holdout is from the original agent; holdout RMSE is now confounded by this policy shift.
- **18:16 UTC:** all R3 data done and pulled (3 × 30,000 runs; all workers finished). Training cell 50 (ridge, ≈ 189,000 runs)
  running since 17:54 (file scan of ≈ 200,000 run files first).
- **17:15 UTC:** R3 subsamples here: w5 and w6 complete (30,001 linked each); w4 finishing in the queue. Training ids 50–54 next.
- **16:15 UTC:** R3 subsamples downloading (w4 26,610 / w5 15,760 / w6 15,492 of 30,001 files; ≈ 22,000 files/h in total,
  ≈ 1.5 h left). Training ids 50–54 follows.
- **15:23 UTC:** pulling the R3 subsamples here (w4 in the queue at ≈ 7 files/s; w5 and w6 started in parallel so the queue's
  own pulls find them present). Training ids 50–54 starts once all three are linked.
- **14:20 UTC:** R3 generation finishing on the workers; first subsample uploading (`rundata_w5_k32` 3,000/30,001 files).
  This VM's queue waits for all three, then trains ids 50–54.
- **13:19 UTC:** R3 data generating on three workers; this VM's queue waits for their subsamples, then trains ids 50–54.
- **11:17 UTC:** R3 data still generating on three workers (≈ 14:20 UTC expected). This VM idle.
- **10:16 UTC:** R3 data still generating on three workers (≈ 14:20 UTC expected). This VM idle.
- **09:16 UTC:** R3 worker r3w4 at chunk 111/300 (≈ 37 chunks/h; generation done ≈ 14:20 UTC, uploads after). This VM idle.
- **08:17 UTC:** R3 workers all active (last update 08:16); data expected ≈ 14:00–15:00 UTC. This VM idle until then.
- **07:17 UTC: R3 data on three workers,** chunks 36–37/300 after one hour (≈ 8 h projected; the generating agent now
  evaluates vr_41 at card picks). Bonus refinement done: vr2b10 / vr2b30 both discard vs vr2b20 (20 stays). This VM idle.
- **06:08 UTC: the R2 run value (vr_41, bonus 20) keeps on every track:** A0 `vr2b20` (pooled 400 +4.8 pts), A10
  `a10_vr2b20` 34.3% (+5.0 pts, P=0.979; dev replication running), A20 `a20_vr2b20` pooled 500 3.6% (P=0.978), Heart
  `heart_vr2b20` pooled 500 14.8% Heart wins (12.0% before, P=0.996).
- **05:53 UTC: A0 best `vr2b20`** (card picks + 20 × the R2 run value, h=64 net on 89,100 runs): val 81.0% (+6.0 pts,
  P=0.978); dev +1.0 pt (P=0.66); pooled 400 +4.8 pts, P=0.973. vr2b50 / vr2b100 discard (peak at 20). Ports to A10 / A20 /
  Heart queued.
- **05:16 UTC: R2 training, 7 of 8 cells done** (last: h=1,024 at 89,100 runs, running). Cells (pasted):
  `VR2 id=30 runs=35100 ridge lambda=0.001 holdout_rmse=0.5135 holdout_corr=0.338 predict_mean_rmse=0.5451 31s`
  `VR2 id=31 runs=35100 hidden=64 layers=2 params=46913 best_epoch=1 holdout_rmse=0.5128 holdout_corr=0.356 predict_mean_rmse=0.5451 49s`
  `VR2 id=32 runs=35100 hidden=256 layers=2 params=236801 best_epoch=1 holdout_rmse=0.5321 holdout_corr=0.318 predict_mean_rmse=0.5451 90s`
  `VR2 id=33 runs=35100 hidden=1024 layers=2 params=1733633 best_epoch=1 holdout_rmse=0.5289 holdout_corr=0.312 predict_mean_rmse=0.5451 1152s`
  `VR2 id=34 runs=89100 ridge lambda=0.001 holdout_rmse=0.5125 holdout_corr=0.342 predict_mean_rmse=0.5451 85s`
  `VR2 id=35 runs=89100 hidden=64 layers=2 params=46913 best_epoch=1 holdout_rmse=0.5046 holdout_corr=0.387 predict_mean_rmse=0.5451 64s`
  `VR2 id=36 runs=89100 hidden=256 layers=2 params=236801 best_epoch=1 holdout_rmse=0.5108 holdout_corr=0.368 predict_mean_rmse=0.5451 265s`
  Best: h=64 at 89,100 runs (0.5046, corr 0.387); ridge nearly flat (0.5135 -> 0.5125); bigger nets need more data.
- **03:15 UTC:** coordinator full-set upload resumed after the 429 (3,000-file commits; each commit draws a transient 502
  that the re-list retry clears): `push rundata_w3: 18000/21000 files uploaded`. Then guarded delete, pull of `rundata_w1_k32`, R2 training. Workers idle.
- **02:17 UTC: all R2 data generated (3 × 30,000 runs).** Store has the r2w1 and r2w2 subsamples and the coordinator's
  `rundata_w3_k32` (30,001 files, one HTTP 502 on the last commit, retried). Coordinator now uploading its full set, then
  frees 12 GB, pulls `rundata_w1_k32`, and trains the eight R2 cells.
- **02:37 UTC incident:** the full `rundata_w3` push failed on HTTP 429 (Hub commit rate limit: three VMs pushing
  500-file commits) at 9,000/30,000 files; the guarded delete correctly did not run, and the runner was stopped before
  the next line could pull 2.6 GB into 2.4 GB of free disk. Fix: `STS_PUSH_BATCH` (3,000 files per commit here) and a
  10-minute back-off on 429; the push resumes (it skips stored files) ahead of the pull. r2w1 is still pushing its full set.
- **01:16 UTC: R2 data:** r2w2 done (store: `rundata_w2` 30,000 + `rundata_w2_k32` 30,001 incl. counts.tsv; pulled here);
  r2w1 generation done, uploading `rundata_w1_k32` (6,000 files so far, its container restarted once); coordinator
  sv chunk 286/300 (this container restarted at ≈ 01:00 UTC; runner resumed 01:13, data intact). R2 training queued behind.
- **23:16 UTC: R2 data:** r2w2 generation done, uploading (store: `rundata_w2_k32` 15,500 files so far; counts.tsv to be
  checked when complete); r2w1 chunk 220/300; coordinator chunk 204/300. No errors.
- **22:16 UTC: R2 data, no errors:** r2w1 chunk 170/300, r2w2 chunk 250/300, coordinator chunk 159/300.
- **21:16 UTC: R2 data, no errors:** r2w1 chunk 120/300; r2w2 chunk 175/300 at 20:59 (its container restarted once, generation
  resumed); coordinator chunk 114/300.
- **20:16 UTC: R2 data, no errors on any VM:** r2w1 chunk 80/300, r2w2 chunk 125/300 (worker summaries), coordinator
  chunk 70/300. Weighted re-runs at 8,100 runs: every net size now beats the mean (h=64 0.5230); ridge 0.5135 still leads.
- **19:17 UTC: R2 data on three VMs, no errors:** r2w1 gate passed, chunk 30/300 (≈ 80 s/chunk); r2w2 chunk 50/300
  (≈ 50 s/chunk, faster VM); coordinator chunk 34/300 (≈ 80 s/chunk).
- **18:20 UTC: Phase R2 started** (pre-registered above): 3 VMs × 30,000 A0 games of run states (≈ 7 h), subsampled
  and pushed to the Hugging Face store (rundata + leafdata already uploaded: 10,000 + 2,500 files). Workers assigned
  W=1 (s2scale) and W=2 (a10ports); the coordinator's share follows the last v1 info cell.
- **17:43 UTC: S2 scale-up done (worker):** sv16 −0.348 (best learned leaf; sv11 was −0.449), sv17 −0.384; line stays
  open by its pre-registered rule. Both workers idle (no pre-registered batch). Main queue: v1 info cells 8–9, then empty.
- **Heart funnel of the Heart best (`heart_bfrelic10`, val 300, from its run file):** final act 1/2/3/4 = 12/61/65/162;
  act-3 clears without keys 18; died to Shield & Spear (floor 54) 9; died to the Heart (floor 55) 113; Heart beaten 40 ->
  conversion at the Heart ≈ 26% (40/153). The Heart fight is the bottleneck. Already tried there: potion value
  (`heart_pvh0`), rollout horizon (`heart_brt8`), search reallocation (`heart_hm2` informational, `heart_hm2b` −, P=0.83),
  combo, and a Heart deck model on 3× data (`heart_bfb25_g12` pooled +0.006, P=0.633). **Next step (analysis, no new
  compute):** read the Heart fights in the interventional logs `bg_heart` + `bg_heart2` (on this VM) — player HP and deck
  at Heart entry vs outcome, turns survived, damage dealt per turn against Invincible 300 — and pre-register a Heart
  batch from what separates the 26% that win.
- **17:38 UTC: Phase R stage 2 closed without a keep.** Selection: ridge on 8,100 runs (gate passed). Card picks: vrb20
  −0.020, vrb50 +0.026 (near-miss; dev +0.078; pooled-400 P=0.884 → discard), vrb100 −0.050, vrb50_nobf −0.090. A0 best
  stays `mapd085`. Main queue: informational v1 cells 7–9 (≈ 1–2 h), then empty.
- **16:50 UTC:** Phase R v1 grid at cell 6/9 (cells 1–5 all lose to predicting the mean; amendment with the v2
  trainer, ridge cells and a gate is queued behind it). s2scale: leaf nets sv16/sv17 trained on 3.2×10⁶ states, leak
  test pass, play tests running. a10ports: idle (five ports, none kept). Data store: waiting for the Mac set-up session.
- **Parallel workers started** (section above): `s2scale` runs the S2 scale-up, `a10ports` the A10/A20 ports; the
  main queue keeps Phase R only. Phase R data at chunk 40/100 at 14:20.
- 14:25 UTC: the queue runner hit the 2-hour background limit during the Phase R data line (chunk 44/100 done,
  `research/queue.current` intact, chunks resume). It restarts at the 15:15 hourly run, so later 2-hour stops line up
  with the hourly runs.
- 14:28 UTC: **the user confirmed that the queue runner (here and in worker sessions) is restarted immediately
  after every 2-hour background stop.** Restarted at once; Phase R data resumes from chunk 45.
- **Data store ready (merged from `setup/hf-datastore`):** private dataset `latioswang/sts-ironclad-data`, verified 2026-11-02T17:58:20Z selftest OK from a claude.ai cloud session in environment 'python' (Linux), not macOS.
  Token = network secret on the cloud environment (added in transit to huggingface.co; never in a VM); this session can
  read it too (`data_sync.py ls` works here). Uploading `rundata` (10,000 files, 4.2 GB) and `leafdata` (2,500 files,
  672 MB) from this VM, so the Phase R and S2 data survive the container and workers can pull them.
- **Phase R data done** (10,000 run files), guarded rebuild bit-identical on all three bests, 3×3 grid started 15:46.
  First cell: `VR id=1 runs=1000 hidden=64 layers=2 params=46913 holdout_rmse=0.5466 holdout_corr=0.252 predict_mean_rmse=0.5454` — holdout RMSE above the predict-mean baseline (overfit at 1,000 runs).
- **Heart best `heart_bfrelic10`** (boss relics chosen by KB value + 10× simulated next-act fights; pooled 500 12.0% vs
  10.4%, P=0.986, dev replication queued) on top of `heart_ka3` (ruby key in act 3; extension keep 10.2% vs 9.8%),
  `heart_wba3` (act-3 burning elite +3; extension keep) and `heart_a4pot` (act-4 shop potions; pooled P=0.996, dev +3).
  Heart wins on the pooled 500: 8.2% → 12.0% over the night. Key-less act-3 clears on val: 37 → 18.
- **Phase S closed** (gate: large net at 500 sims −0.57). **Phase S2 done, one cell kept as the scale-up candidate**:
  leaf-trained small net `sv11` −0.45 at equal wall-clock (42.0% wins; best learned leaf), `sv12` (h=220) −0.61; root-
  trained nets transfer poorly to tree leaves (RMSE 15.1–15.7 vs 12.1 leaf-trained). S2 scale-up (7,500 more games,
  h=42 and h=96) queued after Phase R. **Phase R data running** (10,000 games of run states, ≈ 15:40), then the
  3×3 grid, selection, leak test and four A0 card-pick experiments. The user's progress page now rebuilds itself
  from the repository (`research/tools/build_progress.py`, plan text in `research/progress/plan.json`) and is
  republished hourly: https://claude.ai/artifact/51apPQYZop7TnZSpdoC6am. Heart best `heart_bfrelic10` (pooled 12.0%).
- **M5 test confirmation (500 test seeds, run 6 of the test budget): `bfbonus10_g1` 66.6% win, Wilson 95%
  [62.4%, 70.6%], floor 45.9; paired vs chest1's test run (54.6%): +0.163 score, +12.0 pts, P=1.000.** The
  val figure (73.3%) overstates it by ~7 pts — the usual winner's-curse gap of a chain of val-selected keeps
  (chest1 was 57.7% val / 54.6% test, a 3-pt gap; the chain since then added four keeps). Test is the
  honest number; val remains the loop's metric.
- Tracks: **A0 best `mapd085`: `mapd085` 75.0% win, floor 47.4, +0.018 vs base, P=0.913 → keep** (map-path discount 0.9 → 0.85: a shorter planning horizon on the
  map DP; marginal keep; dev check: `mapd085_dev` 68.0% win, floor 45.6, +0.000 vs base, P=0.431 → info vs bfbonus10_g1_dev — exactly zero on 100 fresh seeds, i.e.
  the changed discount altered no outcome there: unreplicated. The trend did not continue: `mapd080` 73.3% win, floor 47.2, -0.021 vs base, P=0.086 → discard (vs
  mapd085) — 0.85 is a local optimum on val only; the keep stands under the rule but is not claimed as
  an improvement; batch 39 otherwise neutral: wev15 −0.000, wev25 −0.040,
  wsr2 −0.004, wsr4 −0.005, resthp04 −0.018, resthp06 +0.002). Previous: **`bfbonus10_g1`: `bfbonus10_g1` 73.3% win, floor 47.3, +0.051 vs base, P=0.947 → keep** (KB value + 10 × the g1 model's predicted fight-value
  gain — graded labels: damage fraction vs the act boss and vs a next-act boss; replaces g0 in the bonus).
  Dev check (100 fresh seeds vs bfbonus10_dev 67.0%): `bfbonus10_g1_dev` 68.0% win, floor 45.6, +0.023 vs base, P=0.663 → info — direction positive, size smaller than val
  (+4.0 pts) and not significant at n=100; the keep stands under the val rule, label "weakly replicated". Previous: **`bfbonus10`: `bfbonus10` 69.3% win, floor 46.8, +0.054 vs base, P=0.952 → keep** — the first learned *strategic* component that pays:
  card value = KB tier value + deck-need bonuses + 10 × (interventional model g0's predicted fight value
  with the card − without it). The model as a replacement lost 10–16 pts; as an additive adjustment to
  the long-horizon tiers it adds +3.0 pts win (vs potsave2 66.3%; chain: wea1p1 63.7% → potsave2 → bfbonus10).
  Replicated on 100 fresh dev seeds: `bfbonus10_dev` 67.0% win, floor 44.9, +0.051 vs base, P=0.876 → info (vs potsave2_dev 63.0%); **A10 best `a10_bfbonus25_g1m`: `a10_bfbonus25_g1m` 29.3% win, floor 37.4, +0.062 vs base, P=0.950 → keep**
  (chain: rest <60% 18.7% → KB synergy bonuses 24.3% → elites only above 75% HP 27.0% → 25× bonus from
  the combined-ascension deck-value model 29.3%);
  **A20 best `a20_restboss1`: `a20_restboss1` 4.0% win, floor 28.4, +0.014 vs base, P=0.875 → keep** (always heal at the pre-boss rest site; pooled val+dev keep, P=0.912,
  dev-alone +0.012); previous **`a20_bfb25`: `a20_bfb25` 3.7% win, floor 27.8, +0.036 vs base, P=0.991 → keep** (deck-value bonus 25× instead of 10× with model 3, on top of the
  boss-potion change). Dev check of the two keeps together: `a20_bfb25_dev` 1.0% win, floor 26.2, -0.026 vs base, P=0.146 → info vs a20_bfbonus10_g2m_dev — **negative on
  fresh seeds**; both A20 keeps of batch 41 are val-only artefacts (the floor-driven A20 ladder at n=300 has
  a 1-floor noise band). They stand under the rule but the reported A20 state is "floor ~27, unreplicated
  since a20_bfbonus10_g2m". Rule for A20 from here: keep decisions on the pooled val+dev 400 (the A20 dev
  run costs 30 s), declared in advance; previous **`a20_potb2`: `a20_potb2` 1.7% win, floor 27.0, +0.013 vs base, P=0.998 → keep** (potions worth 2 HP in boss fights only — spend them where A20 runs end;
  P=0.998 on a +0.013 score change; dev check: `a20_potb2_dev` 2.0% win, floor 27.0, -0.002 vs base, P=0.321 → info vs a20_bfbonus10_g2m_dev — does not replicate
  (floor-level noise); previous **`a20_bfbonus10_g2m`: `a20_bfbonus10_g2m` 1.0% win, floor 26.6, +0.017 vs base, P=0.881 → keep** (model 3's 10× bonus; kept on the pooled val+dev rule,
  P=0.935; previous **`a20_restlo`: `a20_restlo` 1.3% win, floor 25.6, +0.009 vs base, P=0.920 → keep** (chain: elite gate 80% HP → act-1 elites avoided → rest rooms
  valued higher; replicated on 100 dev seeds, +0.084, P=0.999; seeded from `base_a20` 2.3% / floor 21.8); **Heart best `heart_bfbonus25_g_heart`: `heart_bfbonus25_g_heart` 9.3% win, floor 46.3, +0.052 vs base, P=0.989 → keep** (win = act ≥ 4 && win; model 6 = A0 fights +
  Heart-labelled act-3 states, 25× bonus; **dev check: `heart_bfbonus25_g_heart_dev` 4.0% win, floor 45.3, -0.034 vs base, P=0.197 → info vs heart_cur_dev 9.0% — does NOT replicate
  on 100 fresh seeds (−5 Heart wins); the keep stands under the val rule but is labelled "unreplicated" and
  is not claimed as a Heart improvement until a second independent sample agrees**. Second slice (dev2,
  100 seeds): baseline `heart_cur_dev2` 8.0% win, floor 43.2, → info; `heart_bfbonus25_g_heart_dev2` 9.0% win, floor 45.6, +0.053 vs base, P=0.929 → info — floors +1.9 and P=0.93 on this slice, Heart wins +1 of 100. Verdict:
  the score effect (floors in acts 3–4) replicates on dev2 but not dev; Heart wins are flat on both slices
  (−5, +1). Third slice (dev3): baseline `heart_cur_dev3` 4.0% win, floor 41.1, → info; `heart_bfbonus25_g_heart_dev3` 4.0% win, floor 43.7, +0.046 vs base, P=0.984 → info — floors +2.6 (43.7 vs 41.1), Heart wins equal (4 vs 4). Verdict
  across three independent slices: the floor effect replicates on two of three (dev2, dev3), Heart wins are
  flat on all three (−5, +1, 0). Reported as "floor-replicated, Heart wins unchanged"; 10× `heart_bfbonus10_g_heart` 8.0% win, floor 45.2, +0.020 vs base, P=0.859 → discard; chain: INFO_keys1 6.3% → heart_cur 7.3% → 9.3%).
- Batch 26 (vs priormlp_d0): `wea1p1` KEEP; `selq03` +0.003, `selq05` +0.010 (robust-child selection
  neutral), `sbd0` exactly +0.000 (removal order never differed), `ca1b2` −0.007.
- A0 keep chain: … → chests 57.7% → root MLP prior 61.3% → act-1 elite path weight 63.7% → potions
  saved for bosses 66.3% (val; 65.5% pooled val+dev) → interventional deck-value bonus on card picks 69.3%
  (model g0) → same bonus with the graded-label model g1 73.3% → map-path discount 0.85 75.0%.
- g1 as a replacement (vs bfbonus10): `bfpick_g1` 64.3% (−0.066), `bfrem_g1` 64.3% (−0.068) — still worse than
  the hand rules; as a bonus it is the best component so far.
- Batch 34 (potion value): `potv12` −0.030; `potsave` +0.026/P 0.81 (dev +0.042, pooled +0.030/P 0.866:
  fails); **`potsave2` pooled KEEP** (above).
- A20 (160/300 base runs die in act 1, 89 before the boss): `a20_rest06` −0.006, `a20_ct4` −0.007;
  **keeps: `a20_eh08` (elites only above 80% HP) floor 21.8→24.2 (+0.037, P=0.986), then `a20_noel1`
  (act-1 elites valued below a hallway fight) floor →25.3 (+0.019, P=0.902), then `a20_restlo` (rest
  rooms valued 5 below 60% HP) floor →25.6 (+0.009, P=0.920).** Discards: `a20_el04` (elites below
  hallway fights in every act) −0.016, P=0.04 — later elites are still needed; `a20_mon1` −0.007.
  The keeps are floor-driven; wins moved 7→4→3→4 of 300, which is noise at this rate. Rule for the A20 ladder: keeps
  on score are legitimate while wins are < 10%, but every A20 keep must be re-checked on win rate once
  the track reaches ~10% (floors and wins can diverge: a passive policy survives longer and wins less).
- Batch 29 (A0, vs wea1p1): `wea1p2` −0.008, `weh25` +0.001, `eh05` −0.014 — the map/elite knobs are
  saturated at A0; `a10_wea1p1` −0.022 (the A0 act-1 elite weight does not transfer to A10).
- Batch 31 (A0): `potv2` −0.007, **`potv8` (potions worth 8 HP) 65.0%, +0.022, P=0.77 — near-miss**,
  `rh06b` +0.009 (P=0.69). Follow-up batch 34: save potions for bosses/elites (`potion_v_boss` low,
  `potion_v` high), `potv12`.
- Batch 32 (kb_syn follow-ups): `a20_kbsyn` −0.006 (A20 runs rarely live long enough for deck quality
  to matter); `a10_kbsyn2` (double weights) 19.3%, −0.069, P=0.02; `a10_kbsyn_rel` (relic-card only)
  22.7%, −0.031 — the card-card synergies carry the A10 keep, and the response is sharply
  non-monotonic in the weight, so the keep is being re-checked on 100 independent dev seeds.
- Batch 33 (A10, vs a10_kbsyn 24.3%): `a10_ct4b` +0.007; **`a10_eh075` (elites only above 75% HP) 27.0%,
  floor 35.4, +0.065, P=0.981 → KEEP, new A10 best.** Elite HP gates now: A0 0.6, A10 0.75, A20 0.8.
- Batch 28 (prior-guided rollouts at calibrated equal wall-clock — 2.43× per-simulation cost → 210
  sims): `rollprior_cal` 59.3% win, floor 44.2, -0.062 vs base, P=0.033 → discard. Closed: a learned fast rollout policy does not pay for its cost against
  2.4× more uniform rollouts; the search's value comes from breadth, not rollout quality.
- **Verification on 100 fresh dev seeds (A10):** `a10_rest06` 15.0% win / floor 31.8 vs `a10_kbsyn` 15.0% /
  31.6 — dscore −0.004, P=0.478: **the +5.7-pt KB-synergy keep does not replicate** (val SE at n=300
  is ~2.5 pts, so a +5.7 reading from a true ~0 is a 2σ event — expected once in ~40 keeps/discards).
  Pre-registered handling: the val keep rule stays the loop's gate (frozen protocol), but a track best
  is reported as "unreplicated" until its chain shows dscore > 0 on dev; `a10_eh075_dev` (the whole
  chain) decides the label for A10. If the chain fails too, A10 experiments continue on the current
  best (paired deltas are unaffected by the baseline's absolute level) and the A10 headline is quoted
  with the dev figure alongside. **Result:** `a10_eh075_dev` 18.0% win, floor 34.0, +0.075 vs base, P=0.928 → info (vs a10_rest06_dev, 100 fresh seeds) — the
  chain replicates; the elite gate carries it, the KB-synergy step is ~0.
- **Verification (A20 chain, 100 fresh dev seeds):** `base_a20_dev` 1.0% win, floor 21.9, → info; `a20_restlo_dev` 3.0% win, floor 25.2, +0.084 vs base, P=0.999 → info (vs base_a20_dev) — the three
  floor-driven A20 keeps replicate strongly (floor +4.3 on independent seeds).
- Batch 35 (potion-value shape, vs potsave2): `potsave3` 66.7% win, floor 45.7, +0.006 vs base, P=0.656 → discard; `potsave4` 66.0% win, floor 45.6, -0.002 vs base, P=0.363 → discard — flat around hallway 8 / boss 4.
  **A10 transfer: `a10_potsave2` 20.3% win, floor 34.2, -0.091 vs base, P=0.000 → discard (vs a10_eh075)** — at A10 hallway fights are dangerous enough that
  hoarding potions for bosses kills; the potion policy is ascension-specific (A0 saves, A10 spends).
- **Killer encounters, A0 best `potsave2` (val, 300; computed from DIAG_deaths):** 101 losses, 68% at
  bosses: Champ 15, Collector 14, Donu & Deca 10, Automaton 8, Time Eater 6, Awakened One 6, Hexaghost 5,
  Slime Boss 4, Guardian 1; elites: Giant Head 6, Three Sentries 4, Gremlin Leader 3, Reptomancer 2,
  Nemesis 1, Slavers 1; hallway fights ≤ 2 each. Act-2 bosses alone are 37% of all losses (Champ +
  Collector + Automaton) — the deck arriving at floor 33 is the problem Phase C v3 targets. Potions left
  at death: mean 0.05 (4/101 runs died holding one) — no hoarding waste under potsave2.
- **Phase C v3 first results (model g0: 500 games, 57,522 rows, 14,757 decisions, 42,765 pairs; held-out
  boss-win BCE 0.459 vs 0.685 base, paired corr 0.113, paired sign acc ~0.58):** `bfpick_g0` 56.0% win, floor 45.3, -0.107 vs base, P=0.004 → discard. Decks only
  slightly bigger (mean 24.9 vs 23.7 cards), floors equal (45.3 vs 45.5), but act-3 boss deaths double
  (Donu & Deca 20 vs 10). Diagnosis: the labels are fights against the *current* act's boss, so an
  act-1 pick is judged by Guardian/Hexaghost value — short-horizon; the KB tiers encode what act-3
  bosses need (scaling, AoE). The absolute predictor is good, the option differences are too noisy and
  myopic to beat hand tiers at card rewards. `bfpick_g0_m02` 50.3% win, floor 43.8, -0.195 vs base, P=0.000 → discard (margin 0.02 starves the deck: mean
  20.0 cards); `bfupg_g0` 66.3% win, floor 45.8, +0.004 vs base, P=0.559 → discard (model upgrades ≈ KB upgrades); `bfrem_g0` 54.3% win, floor 44.5, -0.140 vs base, P=0.000 → discard (model removals keep
  immediate damage and drop what act 3 needs). **g0 as a replacement is closed.** Next: (a) the model as
  an additive adjustment to KB values (`bf_bonus`, batch 36); (b) model g1 with graded labels — damage
  dealt to the current boss and to a boss sampled from the *next* act's pool (continuous, far less noisy
  than win/loss, and forward-looking), so option differences carry signal.
  A10 with the A0-trained g0 (vs a10_eh075): `a10_bfrem_g0` 14.7% win, floor 32.6, -0.177 vs base, P=0.000 → discard; `a10_bfupg_g0` 21.7% win, floor 34.3, -0.075 vs base, P=0.003 → discard; `a10_bfpick_g0` 20.0% win, floor 34.9, -0.079 vs base, P=0.041 → discard; `a10_bfpick_g0_m02` 12.7% win, floor 32.1, -0.207 vs base, P=0.000 → discard — the model does not transfer
  across ascensions (enemy HP/damage differ), so any future model must be trained on each ladder's
  own fights (ascension is already a feature; datasets can be merged).
- **Heart track:** `heart_cur` 7.3% win, floor 44.6, +0.047 vs base, P=0.983 → keep (vs INFO_keys1 6.3%; the current A0 agent with keys and the deck-value bonus,
  which ran with model g1 compiled in → recorded as `bf_model=1`). 105 of 300 runs die at the Heart itself.
  Dev check (100 fresh seeds, Heart winmode): baseline `INFO_keys1_dev` 10.0% win, floor 42.1, → info; `heart_cur_dev` 9.0% win, floor 44.3, +0.030 vs base, P=0.790 → info — floors replicate (+0.030), Heart
  wins do not (−1 of 100); the Heart keep is "floor-replicated, wins unproven" — the conversion problem
  (dying at the Heart) is untouched by the A0 chain.
- **Models are runtime data now** (`research/models/bf_g<n>.txt`, chosen by `bf_model=<n>`): after g1 was
  trained, the rebuilt binary could no longer reproduce `bfbonus10` (15/16 games differed) because the A0
  best depended on the compiled-in g0 weights. The pipeline stopped at repro_check (as designed); the fix
  makes every recorded run reproducible regardless of which model was trained last. g0 = model 0, g1 = 1,
  combined A0+A10 = 2. g1 training: 58,540 rows, paired corr boss_dmg 0.139 / next_dmg 0.099 (graded labels
  are only modestly less noisy); its experiments are queued with TRAIN=0.
- **Open simulator issue:** fights vs a next-act boss started from act 1 hit `monstersAlive = −2` with the
  Collector (two INVALID torch-head slots, Collector curHp 0 at turn 0) inside search copies — 147 context
  dumps in 3 of 500 g1 games (0.08% of fights; labels of those fights are garbage). Reproduces on seed
  300071 (`--inproc`, bf_log=1). Diagnosis in progress: the run took Neow's Lament (enemies have 1 HP in
  the first 3 fights); a Collector fight initialised at floors 1–3 has two INVALID placeholder slots
  (torch heads spawn later) and the Lament sets *their* HP to 1 too, so random-target attacks (Sword
  Boomerang) hit and "kill" placeholders → `monstersAlive` −2. Impossible in real play (the Collector is
  never fought under the Lament); only the hypothetical next-act fights expose it.
  Also noted while reading `BattleContext::init(gc, enc)`: the fight's ai/hp/shuffle/cardRandom streams
  are seeded from `gc.seed + gc.floorNum`, not from the GameContext's RNG objects, so hypothetical fights
  at one decision share those streams (common random numbers across options — fine) and coincide with the
  streams the real fight on that floor would use. No decision-time leak: the model is trained offline and
  sees only observable features; cf_pick (closed) would have been affected and must not be revived as is.
  **Root cause fixed in the simulator** (`BattleContext::initRelics`, Neow's Lament): the 1-HP loop ran
  over every slot up to `monsterCount`, including reserved not-yet-spawned slots (`idx == -1`), which then
  counted as alive and died. Fix: skip reserved slots. Unreachable in real play (the Lament covers the
  first 3 fights; reserved slots exist only in act-2/3 encounters), so recorded runs stay bit-identical
  (repro_check in the queue after the rebuild). Patch file regenerated. **Verified:** rebuilt binary —
  potsave2 / bfbonus10 (model 0) / heart_cur (model 1) all bit-identical, leak test green with model 1 on,
  and seed 300071 now logs 0 context dumps (36 before).
- **Model 2 (g1m: bg1 + bg_a10):** A0 `bfbonus10_g1m` 71.0% win, floor 47.1, -0.028 vs base, P=0.200 → discard; `bfbonus25_g1m` 73.0% win, floor 47.1, -0.006 vs base, P=0.432 → discard — slightly worse than the A0-only g1 on A0 (data
  dilution). **A10: `a10_bfbonus10_g1m` 27.7% win, floor 35.7, +0.012 vs base, P=0.663 → discard; `a10_bfbonus25_g1m` 29.3% win, floor 37.4, +0.062 vs base, P=0.950 → keep → KEEP, new A10 best** — the first learned component that pays on A10,
  once the model has seen A10 fights (the A0-only g0 lost 7 pts there). Chain A10: rest<60% 18.7% → KB
  synergy 24.3% → elite gate 75% 27.0% → combined-model bonus 25× 29.3%. Dev check (100 fresh seeds vs
  a10_eh075_dev 18.0%): `a10_bfbonus25_g1m_dev` 21.0% win, floor 34.3, +0.035 vs base, P=0.738 → info — direction and size replicate (+3.0 pts), not significant at n=100.
- Batch 37 (bonus beyond card rewards, model 1, vs bfbonus10_g1): shop purchases `bfshop10` 72.0% win, floor 47.0, -0.020 vs base, P=0.073 → discard; upgrades 10×
  `bfupgb10` 73.3% win, floor 47.3, -0.000 vs base, P=0.504 → discard (identical games — never flips an upgrade); 25× `bfupgb25` 71.0% win, floor 47.0, -0.029 vs base, P=0.102 → discard. The deck-value bonus helps only at
  card rewards; shop and upgrade decisions are left to the hand rules.
- Datasets: `bg1` (A0 best, g1 labels: 500 games, 58,540 rows, 147 Collector/Lament dumps in 3 games) and
  `bg_a10` (A10 best `a10_eh075`: 500 games, 15.8% win, floor 31.8, 107 MB; 993 dumps — generated before
  the Lament fix was built; the affected fights are < 0.1% of rows and are left in; regenerate with the
  fixed simulator when the machine is idle); `bg_a20` (A20 best `a20_restlo`: 500 games, 1.6% win, floor
  23.5, 79 MB, **0 context dumps on the fixed simulator** — bulk confirmation of the Lament fix); `bg_heart`
  (keys agent `heart_cur`, act-3 "next" fight = the Heart: 500 games, 183 reached Act 4, 137 MB, 0 dumps);
  `bg1b` (current A0 best `bfbonus10_g1`, fresh train seeds 300500+: 500 games, 70.2% win, floor 46.3,
  158 MB — the generating agent's own strength on train seeds matches val/test); `bg_a10b` (current A10
  best `a10_bfbonus25_g1m`, seeds 300500+: 500 games, 26.6% win, floor 36.4, 120 MB); `bg1k3` (current A0
  best, seeds 301000+, bf_k=3 — labels averaged over three fights per option: 500 games, 67.6% win, floor
  45.7, 156 MB; ~12 min per 50 games, 3.5× the single-fight cost); `bg_a10c` (A10, fixed simulator, seeds
  300000+ like bg_a10 but generated by the *current* A10 best `a10_bfbonus25_g1m`: 500 games, 23.6% win,
  floor 35.2, 117 MB, 0 dumps — so model 11 tests clean data *and* newer-agent data together).
- **Model 3 (bg1 + bg_a10 + bg_a20: 128,594 rows from 1,500 games; held-out boss-win BCE 0.340 vs 0.607, paired
  corr 0.098):** A20 `a20_bfbonus10_g2m` 1.0% win, floor 26.6, +0.017 vs base, P=0.881 → discard (near-miss → pre-registered dev extension queued); A10 at 10× `a10_bfbonus10_g2m` 25.7% win, floor 36.0, -0.063 vs base, P=0.039 → discard (vs the 25×
  model-2 best — weight-matched 25× run queued); A0 at 10× `bfbonus10_g2m` 71.7% win, floor 46.9, -0.025 vs base, P=0.217 → discard. Pattern: pooling ascensions dilutes —
  each ladder is best served by a model that saw its own fights (g1 on A0, g2 on A10). Weight-matched
  A10 check: `a10_bfbonus25_g2m` 28.0% win, floor 36.5, -0.030 vs base, P=0.181 → discard (model 3 vs model 2, both 25×) — confirms dilution. Queued: A10-only model 4 (bonus
  25× on A10) and A20-only model 5 (10×/25× on A20). A10-only model 4 (40,112 rows; paired corr boss_dmg 0.12): `a10_bfbonus25_g_a10` 26.7% win, floor 37.0, -0.034 vs base, P=0.152 → discard — too little data
  alone; the A0+A10 model (2) stays the A10 best. So: pooled-with-the-target-ladder beats both target-only
  and everything-pooled; the next A10 model should be bg1 + bg_a10 with more A10 games. A20-only model 5
  (29,942 rows; the highest paired correlations, boss_dmg 0.18): `a20_bfbonus10_g_a20` 1.0% win, floor 26.5, -0.003 vs base, P=0.412 → discard; `a20_bfbonus25_g_a20` 1.0% win, floor 27.2, +0.011 vs base, P=0.762 → discard (vs the model-3 bonus) —
  no gain over model 3 at A20 either; the A20 bonus is saturated around +1 floor.
- Batch 38 A20 knobs (vs a20_bfbonus10_g2m): `a20_potv2` 1.0% win, floor 26.3, -0.007 vs base, P=0.308 → discard; `a20_ca1b2` 1.0% win, floor 26.7, +0.002 vs base, P=0.647 → discard; `a20_rest07` 2.0% win, floor 26.4, +0.005 vs base, P=0.651 → discard — all neutral; the A20
  ladder's remaining lever is act-1 survival (Hexaghost 35 and Slime Boss 27 of ~297 deaths are the act-1
  boss; Automaton/Champ/Collector the act-2 boss), i.e. deck strength again.
- Batch 40 (A10 map weights, vs a10_bfbonus25_g1m): `a10_wev25` 30.0% win, floor 37.8, +0.016 vs base, P=0.768 → discard (dev extension declared); `a10_resthp06` 29.0% win, floor 37.5, -0.001 vs base, P=0.492 → discard; `a10_wsr4` 28.3% win, floor 37.5, -0.007 vs base, P=0.299 → discard;
  `a10_mapd095` 30.0% win, floor 37.7, +0.013 vs base, P=0.771 → discard. The A10 map policy is at its optimum within noise.
- Batch 42 (bonus 50×): A20 `a20_bfb50` 1.0% win, floor 28.3, -0.016 vs base, P=0.185 → discard; A10 `a10_bfb50` 30.0% win, floor 37.4, +0.007 vs base, P=0.581 → discard — the bonus weight is saturated at 25× on both ladders.
- Batch 43 (Neow ranking): A20 `a20_neow1` 4.3% win, floor 27.9, +0.010 vs base, P=0.663 → discard; A10 `a10_neow1` 31.0% win, floor 36.7, +0.004 vs base, P=0.542 → discard — neutral on both (as at A0).
- Batch 44 (second replication slice, dev2) done: A0 learned-bonus chain +0.171 (P=0.996) and A10 model-bonus
  keep +0.161 (P=0.991) replicate; the A20 model-bonus keep (+0.007, P=0.61) does not. See "Second-slice
  replication" below.
- Batch 45 (pre-boss rest and card-value knobs): A20 `a20_restboss1` 4.0% win, floor 28.4, +0.014 vs base, P=0.875 → discard (near-miss → pooled extension queued);
  `a20_restboss05` 3.7% win, floor 27.1, -0.014 vs base, P=0.111 → discard; `a20_aoe3` 3.7% win, floor 28.0, +0.004 vs base, P=0.751 → discard; `a20_scal4` 2.7% win, floor 27.9, -0.007 vs base, P=0.180 → discard; `a20_upgpow3` 3.0% win, floor 27.5, -0.011 vs base, P=0.121 → discard. A10 `a10_restboss1` 26.0% win, floor 36.6, -0.048 vs base, P=0.015 → discard; `a10_aoe3` 27.0% win, floor 36.8, -0.035 vs base, P=0.031 → discard — at A10 the pre-boss upgrade beats the heal
  and AoE bonuses hurt; both ladders are at their optimum for these knobs.
- Batch 46 (third replication slice): A0 and A10 learned-bonus keeps replicate (see the replication section).
  Batch 47 (A20 healing follow-ups, vs a20_restboss1): `a20_resthp07` 3.7% win, floor 28.3, -0.004 vs base, P=0.258 → discard; `a20_wrest7` 4.0% win, floor 28.3, -0.000 vs base, P=0.470 → discard; `a20_rh07` 4.0% win, floor 27.5, -0.017 vs base, P=0.134 → discard — flat; the pre-boss heal is
  the only healing knob that moved A20.
- Queue empty at 2026-11-02 00:09 UTC. Next: the Heart and A20 tracks are evaluation-limited (a 2-point
  Heart-win effect needs n > 1,000 at the binary metric), so their keep decisions move to a pooled 500-seed
  rule (val 300 + dev2 100 + dev3 100; dev stays the replication slice) via `research/tools/exp_pooled.sh`,
  and a small Heart batch runs under it overnight (potions spent at the Heart, pre-boss heal, bonus 50×,
  power upgrades).
- Running (sequential queue, `research/queue.txt`): overnight hygiene — regenerate the A10 fight set on the
  fixed simulator (`bg_a10c`, same seeds as bg_a10) and retrain the A10 model (11) → `a10_bfbonus25_g2c`.
  Result: `a10_bfbonus25_g2c` 30.3% win, floor 37.6, +0.014 vs base, P=0.642 → discard — neutral; clean A10 data from the newer agent neither helps nor hurts (consistent with
  the expert-iteration and hygiene expectations). Model 2 stays the A10 model. **The queue is empty**;
  the next-day plan is above.
  potb2/bfb25 val-only (dev −0.002 / −0.027); Heart keep floor-replicated on 2 of 3 slices, wins flat.
- **Plan for the next day (not started at night):** the cheap knobs are exhausted on every track and the
  learned strategic layer is closed at its kept form. The remaining lever inside the fixed wall-clock rule
  is *speed*: the 4×-compute probe gave +4.5 pts, so a 2× faster simulator/search (profiling the per-
  simulation BattleContext copies and action enumeration; the earlier fixed-pile attempt was reverted for
  stack corruption and must be redone safely) is worth ~+2 pts at equal wall-clock on every ladder. Second:
  A20 act-1 survival needs a different idea than map weights (53% of A20 runs die in act 1 with a starter
  deck; Neow choice and the first two card picks are the only levers before the first elite).
  iteration of the strategic model (models 7/8 worse than 2/1), Heart keep replication (floors on one of two
  slices, Heart wins flat), A20-only model 5, A10-only model 4, batch 38 knobs, A0 threshold retune.
- Prior work is at its ceiling with 500-sim targets (v2/v3, temperature, c sweeps all neutral or worse).

#### Earlier state (07:35 UTC)
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
- **Keys/Act 4 (`INFO_keys1`, val 300, keys=1 chest_pol=1): 35.0% of runs reach Act 4 (105/300);
  19 beat the Heart = 18% conversion, 6.3% of all runs.** CORRECTION (08:45 UTC): I first reported
  "49 Heart wins / 47% conversion" — the harness `win` column also counts key-less Act-3 clears
  (30 runs). The Heart track's win is now defined in the harness as `act >= 4 && win`
  (`--winmode heart`, automatic for `TRACK=heart`); earlier Heart-track rows recomputed:
  INFO_keys1 6.3%, wb3 6.0%, krh05 7.0%, bossprep_h 5.0% (all noise-level). Act-4 deaths: ~90% at
  the Heart (floor 55), ~10% at Shield & Spear; Heart wins finish with ~28 HP → the Heart's damage
  race is the constraint, not reaching it.

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

### Chain results 08:39 UTC (gen2 distillation, batch 22, A10 seed, boss-prep batch)
- gen2 (500 self-play games, 2,000 sims, root-prior agent): **65.8% win at 4× compute** (train seeds;
  informational) — the compute lever is consistent (+4.5 pts for 4× sims).
- Distilled prior from gen2 (167k decisions; val CE 0.2481, 55% captured of a sharper target):
  `priormlp2_d0` **58.3%, −0.034 vs priormlp_d0 (P=0.15)** — worse than the 500-sim-target prior.
  Fewer decisions and sharper (more peaked) targets over-focus the root at c=1.5. Next: train on
  gen_all+gen2 combined with hidden 128, and a prior temperature (`prior_temp`, implemented).
- **A10 track seeded: `base_a10` 16.3% win, floor 30.8** (A0 best params at ascension 10).
- Boss prep: `bossprep1` −0.026 (P=0.23), `bossprep2` +0.004 (P=0.54), Heart −0.019, A10 +0.009 —
  neutral everywhere; closed in this form (per-boss card bonuses are too blunt an instrument).

### Batch 23 results (val 300)
A0 (vs priormlp_d0): `ocpot1` (out-of-combat potions) −0.022 (P=0.26); prior temperature: `pm_t07`
(sharpen) **−0.145**, `pm_t15` (flatten) −0.026 → the raw softmax prior is right, sharpening is harmful.
Heart (Heart-win metric, vs INFO_keys1 6.3%): `act4prep1` 5.0% (−0.014, P=0.12), `ocpot_h` 6.0% —
neutral; Act-4 conversion tracks overall deck strength, not Act-4-specific card bonuses.
**A10 (vs base_a10 16.3%): `a10_rest06` (heal at rest below 60%) +0.040, P=0.988 → KEEP (18.7%);**
`a10_elite075` +0.031 (P=0.84); `a10_ct4` (take more cards) +0.028 vs the new best (P=0.89, 20.3%).
Prior v3 (gen0+gen1+gen2, 465k decisions, hidden 128): val CE 0.2668 (50% captured); `priormlp3_d0` running.

`priormlp3_d0` (prior v3: 465k decisions, hidden 128) 62.7%, +0.014 (P=0.65) — neutral; more data
and a bigger model do not move the prior at 500-sim targets. v1 (gen0+gen1, hidden 64) stays canonical.
**Consistency rule added:** the compiled prior weights are part of the agent. `pfm_weights.inc` must be
the file the current best was measured with; a candidate prior is tested from a side build or
restored immediately if not kept (v3 was briefly compiled into `fair_eval` — restored from git before
any further comparison; the best's rows reproduce bit-for-bit on the restored build).

### Batch 25 (val 300) — no keeps
A10 (vs a10_rest06 18.7%): `a10_combo` +0.030 (P=0.78), `a10_eh08` +0.023 (P=0.74), `a10_wrl4`
+0.014, `a10_ct35` +0.007 — all four at the same 18.7% win, differing only in floor; noise.
A0: `pm_c12` +0.016 (P=0.70) — the PUCT c sweep (0.5–1.5) is closed, c stays 1.5.
Parameter-level tuning is exhausted on both tracks; next structural items: KB relic/card synergy
bonuses in card valuation (the KB's synergy lists are unused so far), prior-guided rollouts at
measured equal wall-clock, A20 seed.

### Phase C v3 — interventional deck value (`bf_*`), implemented 2026-11-01 10:30 UTC, queued
Why: 57% of the A0 best's losses and 30% of all A10 runs end at an act boss; the 4×-compute probe
(+4.5 pts) says combat search is not the main bottleneck — the deck is. V_run (observational) failed
because rows within a run share one label and the path confounds the deck; cf_pick (online fights)
failed to winner's curse at 3 fights. v3 moves the fights offline and learns from them:
- `bf_log=1`: at every card reward, rest upgrade and removal, the agent copies the run state once per
  option (card added / upgraded / removed) and plays, for the base state and each option, one fight
  vs the act boss and one vs a sampled hallway/elite fight of the act (`bf_sims` 100, no potions,
  fresh RNG, **same fight seeds for every option of the decision** = common random numbers). Rows:
  666 observable run features + [boss_win, boss_hp, hall_win, hall_hp, run, decision, kind, is_base].
  Logging never touches the agent's RNG, so the generating games are the best agent's own games.
- `train_bf.py`: MLP 666→128→4 (sigmoid heads), BCE on absolute labels + λ·MSE on the predicted
  option-minus-base differences vs the observed paired differences; run-grouped validation; the
  export gate needs boss-win BCE below the base rate and a positive paired correlation.
- Decisions: `bf_pick=1` (card rewards: argmax predicted gain, skip unless > `bf_margin`),
  `bf_upgrade=1`, `bf_remove=1` (argmax predicted value). Value = bf_wb·P(boss) + bf_whp·bossHP +
  bf_wh·P(hall) + bf_whhp·hallHP. Bit-identical with everything off (repro_check in the queue).
- Cost: ≈8 fights per logged decision ≈ 1 min/game → 500 games ≈ 2 h on 4 cores (queued after the
  cheap batches 28–30). Pipeline: `research/tools/bf_pipeline.sh DATA TAG` (train → rebuild →
  repro_check → leak test with the model on → `bfpick_g0`, `bfpick_g0_m02`, `bfupg_g0`, `bfrem_g0`).

### KB synergy bonuses (`kb_syn=1`) — batch 27 result: neutral/negative
`kbsyn1` 63.3% (−0.007, P=0.43), `kbsyn2` (double weights) 60.0% (−0.037, P=0.18): the KB's
synergy lists do not improve picks on top of tiers + deck-need bonuses (a bigger push hurts).
`kbsyn_rel` and `a10_kbsyn` pending. Originally:
The repo KB's synergy lists were unused until now. `research/tools/gen_kb_synergy.py` turns them into
1,422 card-card and 1,741 relic-card pairs (synergy +1, super +2, anti −1; colorless cards and
curses mapped through the CardId enum). Card valuation adds `kb_syn_card_w` × (capped sum over deck
cards) + `kb_syn_relic_w` × (capped sum over held relics). Variants: default weights, doubled,
relic-only; also on the A10 track. Bit-identical when off.

### Boss-aware deck preparation (`boss_prep=1`) — implemented, tested in batch 20 (neutral)
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
2. `pgrep -x fair_eval` — if evals are running, wait for them; never start a 2nd batch. Queue work
   with `research/tools/runq.sh` (reads `research/queue.txt`); check `research/queue.log`.
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

## Pre-registered near-miss extension (rule added 2026-11-01 11:40 UTC)
A val near-miss (dscore > 0, 0.7 ≤ P < 0.9) may be extended **once** with the 100 dev seeds, declared
before the dev run is seen; the keep decision is then made on the pooled 400 seeds with the same rule
(dscore > 0, P ≥ 0.9) **and** the dev portion alone must have dscore > 0 (the val portion is where the
candidate was selected, so pooled-only would inherit the winner's curse). `research/tools/pooled.py NAME
BASE ASC val,dev`. A near-miss that fails stays discarded; no second extension; a family whose declared
candidates failed on dev gets no further extensions. Cost: one 100-game run per candidate.

| Candidate | val (300) | dev alone (100) | pooled (400) | decision |
|---|---|---|---|---|
| `potsave` vs `wea1p1` | +0.026, P=0.807 | +0.042, P=0.750 | +0.0296, P=0.866 | discard |
| `potsave2` vs `wea1p1` | +0.035, P=0.875 | +0.074, P=0.886 | +0.0445, P=0.949 | **KEEP** |
| `bfbonus25` vs `bfbonus10` | +0.019, P=0.729 | -0.028, P=0.325 | +0.0075, P=0.609 | discard |
| `bfbonus50` vs `bfbonus10` | +0.041, P=0.876 | -0.030, P=0.340 | +0.0233, P=0.780 | discard |
| `a20_bfbonus10_g2m` vs `a20_restlo` | +0.017, P=0.881 | +0.027, P=0.863 | +0.0192, P=0.935 | **KEEP** |
| `a10_potv2` vs `a10_bfbonus25_g1m` | +0.028, P=0.865 | -0.000, P=0.497 | +0.0208, P=0.811 | discard |
| `ct45` vs `bfbonus10_g1` | +0.015, P=0.721 | +0.006, P=0.584 | +0.0129, P=0.732 | discard |
| `vrb50` vs `mapd085` | +0.026, P=0.761 | +0.0775, P=0.866 | +0.0386, P=0.884 | discard |
| `vr3b40` vs `vr2b20` | +0.023, P=0.758 | +0.0876, P=0.885 | +0.0392, P=0.900 | **KEEP** |
| `a10_vr3b40` vs `a10_vr2b20` | +0.021, P=0.716 | +0.0778, P=0.867 | +0.0348, P=0.870 | discard |

Two of seven extensions kept (potions saved for bosses on A0; the model-3 bonus on A20); the bonus-weight
family (25/50) and the threshold family (4.5) were val-only artefacts. Eighth use, declared before its dev
run: `a10_wev25` 30.0% win, floor 37.8, +0.016 vs base, P=0.768 → discard vs `a10_bfbonus25_g1m` (event rooms valued 2.5 at A10).
Outcome: `a10_wev25_dev` 21.0% win, floor 34.2, -0.002 vs base, P=0.460 → info; pooled val,dev: n=400  a10_wev25 win=0.278 floor=36.91  a10_bfbonus25_g1m win=0.273 floor=36.60  dscore=+0.0111 dwin=+0.005 P(improve)=0.743 → fails, stays discarded. Extension tally: 2 kept of 8. Ninth use (A20, declared before its dev run, under the A20 pooled rule): `a20_restboss1` 4.0% win, floor 28.4, +0.014 vs base, P=0.875 → discard vs `a20_bfb25`
(always heal at the pre-boss rest site). Outcome: `a20_restboss1_dev` 2.0% win, floor 26.3, +0.012 vs base, P=0.851 → info; pooled val,dev: n=400  a20_restboss1 win=0.035 floor=27.85  a20_bfb25 win=0.030 floor=27.40  dscore=+0.0138 dwin=+0.005 P(improve)=0.912 → **KEEP**, new A20 best (the first
A20 keep decided on the pooled rule; dev-alone positive). Extension tally: 3 kept of 9.

## Label-noise reduction (bf_k=3), result 2026-11-01 21:30 UTC
Model 9 (bg1k3: 59,737 rows, labels averaged over three fights per option): paired correlations rose to
0.19 (boss_dmg) / 0.14 (next_dmg) from g1's 0.14 / 0.10, but the agent did not improve: `bfbonus10_g1k3` 72.7% win, floor 47.2, -0.008 vs base, P=0.402 → discard.
Less noisy labels of the same kind do not help either; with "more data" (model 8) and "cleaner labels"
(model 9) both neutral, the A0 bonus is at the ceiling of this label family. Model 10 (pooled bg1 + bg1k3,
paired corr 0.20 boss_dmg): `bfbonus10_g1k3p` 74.3% win, floor 47.4, +0.011 vs base, P=0.639 → discard — neutral too. **The strategic model line is closed on A0 at g1.**

## Next block (queued 2026-11-01 17:25 UTC): expert iteration of the strategic model
The deck-value models were trained on fights from the *previous* agents' games. Queued after the Heart
block: 500 more A0 games from the current A0 best (`bg1b`, fresh train seeds 300500+) → model 8 on
bg1+bg1b → `bfbonus10_g1b`; 500 more A10 games from the current A10 best (`bg_a10b`) → model 7 on
bg1+bg_a10+bg_a10b → `a10_bfbonus25_g_a10b`. Expected: modest gains (the g0→g1 step was +4 pts from
better labels; this step changes the data distribution, not the labels). **Result (A0): `bfbonus10_g1b` 72.0% win, floor 47.0, -0.019 vs base, P=0.285 → discard** — model 8
(1,000 games, paired corr 0.17 boss_dmg) does not beat g1 (500 games); more data of the same kind is not
the lever; g1 stays. **A10: `a10_bfbonus25_g_a10b` 24.0% win, floor 37.0, -0.060 vs base, P=0.031 → discard** — model 7 (1,500 games) is clearly worse than model 2 (1,000 games) on
A10. Expert iteration of the strategic model is closed: on both ladders the model trained on the
*previous* agent's games is better than one that also saw the current agent's games. Likely reason: the
current agents' decks are already shaped by a model bonus, so their option sets carry less contrast
(the model's own picks dominate), and the regression learns less from them. Models 2 (A10) and 1 (A0) stay.

## Second-slice replication of the kept chains (batch 44, dev2 = seeds 600000+)
- A0: `potsave2_dev2` 58.0% win, floor 45.2, → info; `bfbonus10_g1_dev2` 72.0% win, floor 46.8, +0.171 vs base, P=0.996 → info — the two learned-bonus keeps (g0 → g1) replicate strongly on an independent sample
  (72.0% vs 58.0% win, +0.171 score, P=0.996).

- A0, third slice (dev3): `potsave2_dev3` 53.0% win, floor 41.4, → info; `bfbonus10_g1_dev3` 62.0% win, floor 44.4, +0.147 vs base, P=0.988 → info — 62.0% vs 53.0% win, +0.147, P=0.988: the learned bonus now
  replicates on three independent slices (dev +0.051, dev2 +0.171, dev3 +0.147).
- A10: `a10_eh075_dev2` 18.0% win, floor 32.4, → info; `a10_bfbonus25_g1m_dev2` 26.0% win, floor 36.6, +0.161 vs base, P=0.991 → info — the model-2 bonus keep replicates on the second slice (26.0% vs 18.0% win, +0.161,
  P=0.991); with the first slice (+0.035, P=0.74) the A10 keep is now replicated.

- A10, third slice (dev3): `a10_eh075_dev3` 17.0% win, floor 30.6, → info; `a10_bfbonus25_g1m_dev3` 28.0% win, floor 35.5, +0.205 vs base, P=1.000 → info — 28.0% vs 17.0% win, +0.205, P=1.000: the A10 model bonus
  replicates on three slices too (dev +0.035, dev2 +0.161, dev3 +0.205).
- A20: `a20_restlo_dev2` 2.0% win, floor 23.0, → info; `a20_bfbonus10_g2m_dev2` 1.0% win, floor 23.9, +0.007 vs base, P=0.608 → info — the model-3 bonus keep (pooled keep, dev +0.027) reads +0.007 (P=0.608) on
  the second slice: a small floor effect that does not replicate; the A20 ladder's keeps since a20_restlo
  are all within its noise band. Honest A20 state: floor ≈ 26–27, wins 1–4% (noise), chain a20_eh08 →
  a20_noel1 → a20_restlo replicated (+0.084 on dev), nothing solid after that.

## Phase S — scaling curve of the learned leaf value (approved by the user 2026-11-02 05:27 UTC, pre-registered 05:40 UTC)

**Question.** Does a learned combat value, scaled in data and parameters, overtake random rollouts at equal
wall-clock on this machine (4 CPU cores)? The month's earlier answer was "no" at one small point (Phase B leaf);
this measures the curve instead of one point.

**Protocol (frozen before any run).**
- Data: `research/tools/sv_gen.sh svdata 2500 100 310000` — 2,500 train-seed games (seeds 310000+) of the A0
  best with the policy/root-value logger (`pf_log=1`, format v2): one state per decision, target = the search's
  root mean value (the AlphaZero-style bootstrapped target of Phase B, which beat final outcomes). ≈ 10⁶ decisions.
- Models (`research/vf/train_sv.py`): DIM 204 → h → h → 1, ReLU, standardised regression, Adam, 20 epochs, best
  holdout epoch exported to `research/models/sv_<id>.txt`. Sizes h = 42 / 220 / 850 ≈ 1.0×10⁴ / 9.3×10⁴ / 9.0×10⁵
  parameters. Data scales 10⁴ / 10⁵ / 10⁶ decisions as nested subsets of one fixed shuffle; one held-out block
  (the last 10%, ≤ 50,000) shared by every model. Ids 1–9 = data scale (outer) × size (inner).
- Evaluation (`research/tools/sv_eval.sh`): the model replaces rollouts entirely (`leaf_sv=<id> rollout_turns=0`);
  per-game cost calibrated on the idle machine against the A0 best (`calibrate.sh`, 8 in-process games) and
  converted to simulations at equal wall-clock; then val 300 paired vs `mapd085`. Rows logged with status
  `scaling` — **never promoted by this protocol**. If a cell clears the ordinary keep rule it may be promoted
  only after a dev replication, like any other keep.
- Read-out: the 3×3 table of paired dscore (and win rate), the slope along data at fixed size and along size at
  fixed data. Decision rule: continue to 10⁷ decisions (25,000 games, ≈ 7 h) only if the 10⁵→10⁶ slope at the
  two larger sizes is positive; otherwise Phase S is closed with the curve as the result.
- Predictions (written before the runs): all nine cells below the base on val (LOW confidence that any cell
  clears zero); the data slope positive at the two larger sizes (MED); the size slope flat or negative at 10⁴
  decisions (overfitting) and positive at 10⁶ (MED). The decisive number is the slope, not the level.
- Cost: generation ≈ 2,500 × 3.9 s / 4 ≈ 40 min; nine trainings ≈ 45 min (the 10⁶ × 9×10⁵ cell dominates);
  nine calibrations + evaluations ≈ 60 min. Everything runs in the sequential queue after batch 53.

**Phase S results, part 1 — fit quality (holdout RMSE of the search's root value, predict-mean baseline 33.57;
pasted from the trainer, 2026-11-02 06:49 UTC):**

| decisions \ params | 1.0×10⁴ | 9.4×10⁴ | 9.0×10⁵ |
|---|---|---|---|
| 10,000 | 16.59 (sv1) | 13.83 (sv2) | 13.28 (sv3) |
| 100,000 | 11.78 (sv4) | 10.69 (sv5) | 9.92 (sv6) |
| 813,736 | 10.18 (sv7) | 7.97 (sv8) | 5.65 (sv9) |

Both slopes are positive in every row and column and the largest step is at the largest cell (8.0 → 5.65), so the
fit curve has not flattened at this scale. My pre-registered prediction that the 10⁴-decision row would overfit
with size was wrong (13.8 → 13.3 improves). The number that decides anything is still the play test: each model as
the leaf value with no rollouts, at equal wall-clock, vs `mapd085` on val 300 — running now.

**Phase S results, part 2 — play tests (equal wall-clock, val 300 vs `mapd085`, status `scaling`):**
- `sv1` (10⁴ × 10⁴): calibration candidate/best 0.400 → 1,250 simulations; win=0.067 floor=24.86 dscore=−1.1246
  P=0.000 (3.89 s/game) — collapse. Control added to the queue (declared before the remaining cells finish): the
  heuristic cutoff leaf with zero rollouts at the same conversion (`svctrl`), so every cell is read against "no
  rollouts" as well as against the rollout agent.

- `sv5` calibration: candidate/best 3.326 → 150 simulations at equal wall-clock. **The leaf network's inference cost
  dominates on this CPU**: at 9.4×10⁴ parameters a game is 3.3× slower than random rollouts, so equal wall-clock
  gives the bigger models fewer simulations — the play test confounds value quality with search depth. Added
  (07:06 UTC, declared before those cells ran): a fixed-simulation read for the diagonal cells (`sv1_s500`,
  `sv5_s500`, `sv9_s500`: 500 simulations, no rollouts, wall-clock ignored, logged as budget-scaling, informational)
  so the value-quality slope and the practical equal-compute number are reported separately.

- `sv5` (10⁵ × 9.4×10⁴, 150 simulations at equal wall-clock): win=0.267 floor=36.56 dscore=-0.6952 P=0.000
  (3.44 s/game). Still far below the rollout agent, but 20 points of win rate above `sv1` with 8× fewer simulations:
  value quality improves with scale faster than the inference-cost penalty removes search.

- `sv9` calibration: candidate/best 83.791 (1,867.7 s vs 22.3 s for eight in-process games) → equal wall-clock would
  be ≈ 6 simulations; the evaluator floors at 50, so the `sv9` row is a **~8× budget-scaling point, not equal
  wall-clock** (labelled as such below). Consequence for the plan: the fixed-500-simulation read of `sv9` costs
  ≈ 230 s per game, so it runs on `val100` (the first 100 val seeds; base rows taken from the identical val games),
  informational only. `sv3`/`sv6` (850-wide) will also sit at the 50-simulation floor.

- `sv9` (8.1×10⁵ × 9×10⁵, **50 simulations = 6.0× the base's wall-clock (23.60 s vs 3.94 s per game), budget-scaling point**): win=0.270
  floor=37.90 dscore=-0.6655 P=0.000. Same level as `sv5` at 150 simulations and equal wall-clock: the best
  value net, with a third of sv5's simulations and 6× its compute, plays like sv5. Read: at this scale the leaf
  net cannot replace rollouts on a CPU; what it buys in value quality it loses in simulations.

- `sv2` (10⁴ × 9.4×10⁴, 156 simulations at equal wall-clock): win=0.260 floor=35.76 dscore=-0.7176 P=0.000 —
  indistinguishable from `sv5` (same size, 10× the data, RMSE 13.8 vs 10.7). At equal wall-clock the simulation
  count, not the fit, sets the level in this regime.

- `sv4` (10⁵ × 10⁴, 1,037 simulations at equal wall-clock): win=0.343 floor=38.28 dscore=-0.5848 P=0.000 —
  the best cell so far: the small net, which keeps most of the simulations, beats the bigger nets at equal
  wall-clock once it has 10⁵ decisions (vs `sv1` −1.12 at 10⁴ decisions). The data slope at the small size is the
  steepest effect in the play grid.

- `sv7` (8.1×10⁵ × 10⁴, 1,133 simulations at equal wall-clock): win=0.360 floor=38.42 dscore=-0.5653 P=0.000 —
  barely above `sv4` (−0.58): the small net's data slope flattens between 10⁵ and 8×10⁵ decisions in play even
  though its fit kept improving (11.8 → 10.2). At the small size the fit, not the data, is now the limit; at the
  large sizes the inference cost is. Neither path reaches the rollout agent at equal wall-clock on this CPU.

- `sv8` (8.1×10⁵ × 9.4×10⁴, 107 simulations at equal wall-clock; calibration 4.656×): win=0.297 floor=38.13
  dscore=-0.6344 P=0.000 — worse than `sv7` (same data, 9× fewer parameters, 1,133 simulations). Equal-wall-clock
  play grid so far: 10⁴ params: −1.12 / −0.58 / −0.57 by data; 9.4×10⁴ params: −0.72 / −0.70 / −0.63; 9×10⁵ params
  (6× budget): −0.67 at full data. On this CPU the equal-compute optimum is the smallest net with the most simulations.

- **Control `svctrl`** (heuristic cutoff leaf, zero rollouts, 3,423 simulations at equal wall-clock; calibration
  0.146×): win=0.117 floor=30.97 dscore=-0.9548 P=0.000. **Read against this, every learned leaf except
  the 10⁴-decision small net is a large improvement**: the control sits at −0.95 (11.7% wins); `sv7` at −0.57
  (36.0%), `sv4` −0.58, `sv9` −0.67, `sv5`/`sv2` −0.70, `sv8` −0.63, `sv1` −1.12. So the learned value is worth
  ≈ +0.38 score (+24 points of win rate) over the heuristic leaf at equal wall-clock, but zero-rollout search of
  any kind is ≈ −0.57 below random rollouts here: rollouts carry information no leaf at this scale replaces.

**Phase S interim read-out (2026-11-02 09:02 UTC, 7 of 9 cells + control; pasted from results.tsv):**

| cell | decisions | params | sims at equal wall-clock | win | floor | dscore vs mapd085 |
|---|---|---|---|---|---|---|
| sv1 | 10⁴ | 1.0×10⁴ | 1250 | 6.7% | 24.86 | -1.1246 |
| sv2 | 10⁴ | 9.4×10⁴ | 156 | 26.0% | 35.76 | -0.7176 |
| sv3 | 10⁴ | 9.0×10⁵ | pending | | | |
| sv4 | 10⁵ | 1.0×10⁴ | 1036 | 34.3% | 38.28 | -0.5848 |
| sv5 | 10⁵ | 9.4×10⁴ | 150 | 26.7% | 36.56 | -0.6952 |
| sv6 | 10⁵ | 9.0×10⁵ | pending | | | |
| sv7 | 8.1×10⁵ | 1.0×10⁴ | 1132 | 36.0% | 38.42 | -0.5653 |
| sv8 | 8.1×10⁵ | 9.4×10⁴ | 108 | 29.7% | 38.13 | -0.6344 |
| sv9 | 8.1×10⁵ | 9.0×10⁵ | 50 (6× budget floor) | 27.0% | 37.90 | -0.6655 |
| control (heuristic leaf) | — | — | 3423 | 11.7% | 30.97 | -0.9548 |
| base `mapd085` (random rollouts) | — | — | 500 | 75.0% | 47.36 | 0 |

Reading, against the pre-registered rule:
1. **Equal wall-clock:** no cell comes near the rollout agent; the best learned leaf (`sv7`, −0.57) is 39 points of
   win rate below it. The zero-rollout control shows why the comparison is fair and still lost: a heuristic leaf
   with 3,423 simulations scores −0.95, so the learned value is worth ≈ +0.38 over the heuristic at equal compute,
   and random rollouts are worth ≈ +0.57 over the best learned leaf. On this CPU rollouts are cheap information.
2. **Slopes in play:** small net: −1.12 → −0.58 → −0.57 (flattened after 10⁵ decisions); mid net: −0.72 → −0.70 →
   −0.63 (+0.07 per decade of data); large net: budget-floored, −0.67 at full data (`sv3`/`sv6` pending). The
   continuation rule needs a positive 10⁵→8×10⁵ slope at both larger sizes; the mid size qualifies, the large size
   waits for `sv6`. Even at +0.07 per decade the mid net needs ≈ 9 more decades of data to reach the base — not a
   path on this machine.
3. **Fit vs play:** the fit grid is monotone and steep (RMSE 16.6 → 5.65), the play grid is nearly flat beyond
   10⁵ decisions. The bottleneck is not label quality or data; it is that each leaf evaluation of a 10⁵–10⁶
   parameter net costs more than a random rollout on a CPU core, so the search loses depth faster than the leaf
   gains accuracy. The fixed-simulation reads (`sv*_s500`, pending) measure value quality with that cost removed;
   if they show the large net close to the base at 500 simulations, the honest conclusion becomes "scaling works
   here but needs inference hardware (a GPU or a batched evaluator)", not "scaling fails".

- `sv6` (10⁵ × 9×10⁵, 50-simulation floor; calibration 37.8×): win=0.247 floor=36.84 dscore=-0.7097 P=0.000.
  Large-net data slope 10⁵ → 8×10⁵ at the floor: −0.71 → −0.67 (+0.04). **Continuation rule: both larger sizes show a
  positive 10⁵→8×10⁵ slope (+0.07 mid, +0.04 large), so the pre-registered condition for 10⁷ decisions is
  technically met — but the slopes are far too small to matter at equal wall-clock (≈ 9–14 decades to the base),
  and the large net is budget-floored, so the condition is met only in form. Decision deferred to the
  fixed-simulation reads: continue to 10⁷ only if `sv9_s500` shows the large net within reach of the base at 500
  simulations (value quality), in which case the limiting factor is inference hardware, not data.**

**Phase S play grid complete (9 of 9 + control; measured wall-clock ratios pasted from the run files):**

| cell | decisions | params | sims | wall-clock vs base | win | floor | dscore |
|---|---|---|---|---|---|---|---|
| sv1 | 10⁴ | 1.0×10⁴ | 1250 | 0.99× | 6.7% | 24.86 | -1.1246 |
| sv2 | 10⁴ | 9.4×10⁴ | 156 | 0.75× | 26.0% | 35.76 | -0.7176 |
| sv3 | 10⁴ | 9.0×10⁵ | 50 | 2.46× | 26.0% | 34.83 | -0.7358 |
| sv4 | 10⁵ | 1.0×10⁴ | 1036 | 1.31× | 34.3% | 38.28 | -0.5848 |
| sv5 | 10⁵ | 9.4×10⁴ | 150 | 0.87× | 26.7% | 36.56 | -0.6952 |
| sv6 | 10⁵ | 9.0×10⁵ | 50 | 3.03× | 24.7% | 36.84 | -0.7097 |
| sv7 | 8.1×10⁵ | 1.0×10⁴ | 1132 | 1.19× | 36.0% | 38.42 | -0.5653 |
| sv8 | 8.1×10⁵ | 9.4×10⁴ | 108 | 1.06× | 29.7% | 38.13 | -0.6344 |
| sv9 | 8.1×10⁵ | 9.0×10⁵ | 50 | 5.98× | 27.0% | 37.90 | -0.6655 |
| control (heuristic leaf) | — | — | 3423 | 0.72× | 11.7% | 30.97 | -0.9548 |
| base `mapd085` | — | — | 500 | 1.00× | 75.0% | 47.36 | 0 |

- `sv3` (10⁴ × 9×10⁵, 50-sim floor): −0.74; the large net's data slope at the floor is 10⁴ → 10⁵ → 8×10⁵: −0.74 → −0.71
  → −0.67. All three sizes improve with data; none approaches the base at equal wall-clock. Pending: the three
  fixed-500-simulation reads (the gate for continuing to 10⁷ decisions).

- Fixed-simulation read `sv1_s500` (10⁴ × 10⁴, 500 sims, cost ignored): win=0.047 floor=24.63 dscore=-1.1490 — the same
  collapse as at 1,250 sims: that cell is simply a bad value function. `sv5_s500` and `sv9_s500` (val100) pending.
  Scaling-law panel (fit and play vs training FLOPs, two cost regimes) added to the progress page (v4).

- **Fixed-simulation read `sv5_s500` (10⁵ × 9.4×10⁴, 500 sims, cost ignored): win=0.283 floor=34.68 dscore=-0.7153 —
  no better than the same net at 150 sims and equal wall-clock (`sv5`: −0.6952, 26.7%).** 3.3× more simulations buy
  nothing for the mid net: its play is limited by the value's error, not by search depth. **This contradicts the
  "inference is the wall" reading I gave the user for the mid net**; the hypothesis survives only if the large net
  (`sv9_s500`, val100, pending) behaves differently. Plausible cause [INFERRED]: holdout RMSE 10.7 is ≈ 30% of the win
  bonus (35), a leaf error large enough that averaging more simulations converges on the wrong values.

## Phase R — one learned run value against the final score (plan approved by the user 2026-11-02 ~11:00 UTC)

Principle (the user's): the only fixed number is the objective, score = min(floor, 51)/51 + win. Every other
constant — the 8 deck-head weights, bf_bonus, card thresholds, the fight terminal value (win bonus 35, 1 per HP,
potion values) — should be derived from a learned V(run state) ≈ E[final score]. Stages, each through the keep rule
with the hand values as the baseline until beaten:
1. Data: `rv_log=1` (existing logger: 666 run features at every map step + [win, floor, act, run id]); 10,000 A0
   games queued (seeds 330000–339999, ≈ 2.7 h on 4 cores). A scaling curve over 10³ / 10⁴ (/ 10⁵ if the slope
   warrants ≈ 27 h) games × 3 sizes, metric: run-grouped holdout error and correlation with the final score.
2. Card / shop / relic choice by the V difference (with − without), first additive on the KB value (the shape that
   worked), then as a replacement.
3. Fight terminal value = V(state after the fight) in place of win bonus + HP + potion constants (the trade-offs
   become derivatives of V).
Known prior failure: Phase C's V_run (P(win) only, ~2,000 runs) lost as a card-pick replacement (`rvpick1/2`).
Differences now: 5× more runs, the full score as target, additive use first, a scaling curve before integration.
Unlike the leaf value (Phase S) V is called ≈ 50 times per run, so model size has no inference wall here.

**Phase R protocol (pre-registered 2026-11-02 11:40 UTC, before any Phase R data exists).**
- Code (default off; games identical with `vr_bonus=0`, verified on 2 games): `research/vf/vr_mlp.h` (runtime model
  `research/models/vr_<id>.txt` over the 666 run features), card-reward hook `v += vr_bonus × (V(deck + card) − V(deck))`
  next to the deck-model bonus, preload before the fork; trainer `research/vf/train_vr.py` (target = min(final floor,
  51)/51 + win per logged run state; split by run; nested run subsets of one fixed shuffle; 1,000 holdout runs).
  Smoke test on 500 old runs (gen0, 300 train runs, h=32): holdout RMSE 0.610 vs 0.660 predicting the mean, corr 0.388.
- Grid: 1,000 / 3,000 / 9,000 training runs × h = 64 / 256 / 1,024 (two hidden layers), ids 1–9; read-out: holdout
  RMSE and correlation (the fit curve). Selection rule: the lowest holdout RMSE becomes `vr_10`.
- Integration (val-300 rule vs `mapd085`, which keeps bf_bonus=10): `vrb20`, `vrb50`, `vrb100` (additive), and
  `vrb50_nobf` (V replaces the deck-model bonus). Keeps go to dev for replication as usual.
- Predictions: fit improves with runs at every size (MED); the largest model is best only at 9,000 runs (LOW–MED);
  an additive integration keeps (LOW–MED; the deck-model bonus already captures part of the signal); the replacement
  `vrb50_nobf` does not beat the current agent (MED).

**Phase R amendment (pre-registered 2026-11-02 16:07 UTC, after v1 cells 1–4 and before cells 5–9 finished; a logged
deviation from the 11:40 protocol).** The first four v1 cells all lose to predicting the mean (pasted):

    VR id=1 runs=1000 hidden=64 layers=2 params=46913 holdout_rmse=0.5466 holdout_corr=0.252 predict_mean_rmse=0.5454
    VR id=2 runs=1000 hidden=256 layers=2 params=236801 holdout_rmse=0.5704 holdout_corr=0.176 predict_mean_rmse=0.5454
    VR id=3 runs=1000 hidden=1024 layers=2 params=1733633 holdout_rmse=0.5700 holdout_corr=0.173 predict_mean_rmse=0.5454
    VR id=4 runs=3000 hidden=64 layers=2 params=46913 holdout_rmse=0.5504 holdout_corr=0.256 predict_mean_rmse=0.5454

Even v1's best epoch (chosen on the holdout itself, which flatters it) is worse than a constant, yet correlation is
≈ 0.25 — a calibrated predictor with that correlation would score ≈ 0.527. Diagnosis: every run contributes ≈ 160
logged states that share one label, inputs are raw (unstandardised), so one epoch shows each run's answer ≈ 160 times and
a net can memorise deck -> score from epoch 1. Check: ridge regression on standardised features, same split, streaming
(`research/analysis/ridge_vr.py`, output `research/analysis/ridge_vr.txt`; per-row lines are the comparable ones):

    RIDGE runs=1000 weighting=per-row lambda=10 holdout_rmse=0.5293 holdout_corr=0.271 predict_mean_rmse=0.5454
    RIDGE runs=1000 weighting=per-run lambda=1 holdout_rmse=0.5339 holdout_corr=0.284 predict_mean_rmse=0.5557
    RIDGE runs=3000 weighting=per-row lambda=1 holdout_rmse=0.5189 holdout_corr=0.311 predict_mean_rmse=0.5454
    RIDGE runs=3000 weighting=per-run lambda=1 holdout_rmse=0.5294 holdout_corr=0.296 predict_mean_rmse=0.5544

The signal is real and grows with data; the v1 training is what fails. Changes (the v1 cells keep running unchanged):
- `research/vf/train_vr2.py`: same run split and holdout; 10% of the training pool (≥ 200 runs) is a validation split
  for early stopping and the ridge lambda (the holdout is scored once); inputs standardised with training moments,
  folded into the exported first layer (runtime `vr_mlp.h` unchanged; export verified: the file reproduces the
  trainer's holdout RMSE exactly on a 300-run smoke set); 16 sampled rows per run per epoch; patience 6, ≤ 80 epochs;
  `--ridge` exports one linear layer.
- Grid v2: ids 11–19 = 1,000 / 3,000 / 9,000 runs × h = 64 / 256 / 1,024; ridge ids 20 / 21 / 22 at 1,000 / 3,000 / 9,000.
- Selection: lowest holdout RMSE over ids 1–9 and 11–22 becomes `vr_10` (`research/tools/vr_select2.sh`). **Gate (new):**
  `vr_10` must beat the predict-mean RMSE on the same holdout, else the leak test and the four integration runs are
  stashed (`research/queue.gated.txt`) and not played. Integration runs and their predictions are unchanged.
- Predictions: v2 nets beat the mean at 3,000 and 9,000 runs (MED); ridge beats every v2 net at 1,000 runs (MED-HIGH);
  the best model at 9,000 runs is a v2 net rather than ridge (LOW-MED); the gate passes (HIGH — ridge at 3,000 runs
  already beats the mean in the diagnostic).
- v1 cells 5–6 (pasted): `VR id=5 runs=3000 hidden=256 layers=2 params=236801 holdout_rmse=0.5696 holdout_corr=0.213 predict_mean_rmse=0.5454`; `VR id=6 runs=3000 hidden=1024 layers=2 params=1733633 holdout_rmse=0.5683 holdout_corr=0.200 predict_mean_rmse=0.5454`.
- **Incident (17:03–17:07 UTC): v1 cells 7–9 (9,000 runs) were OOM-killed** right after loading (kernel: "Memory cgroup out
  of memory: Killed process ... anon-rss:13909144kB"); `| tail -1` masked the exit code (rc=0, no model written). Cause:
  `torch.tensor(X[tr])` copied the 1.42×10⁶ × 666 training rows twice next to the 4.3 GB raw array. Fix: `torch.from_numpy`
  (identical values). The same failure was waiting in train_vr2 at 9,000 runs (`X[it].std(dtype=float64)` temporary);
  fixed before any 9,000-run v2 cell started with chunked float64 moments (max difference from the original formula
  5e-14 on a 9,790-row check; cells 11–13 used the original formula). v1 cells 7–9 re-queued at the end of the queue as
  information only (not part of the gated selection, which runs before them).
- Side check (informational, no trainer change): v2 nets still lose to the mean at 1,000 runs although early stopping
  picks epochs 2–5. Hypothesis "rare features blow up after standardisation": outliers exist (300 smoke train runs:
  7 features sd<0.01, 31 sd<0.05; holdout max |z| 125; 20.0% of holdout rows have some |z|>10) but flooring sd at 0.1
  barely moves the smoke fit (h=64: 0.5378 -> 0.5370 vs mean 0.5460). Not the cause; v2 stays as pre-registered.
  Next v2 cell: `VR2 id=14 runs=2700 hidden=64 layers=2 params=46913 best_epoch=2 holdout_rmse=0.5479 holdout_corr=0.262 predict_mean_rmse=0.5453 7s`.
- **v2 grid + ridge complete (pasted):**
  `VR2 id=11 runs=800 hidden=64 layers=2 params=46913 best_epoch=2 holdout_rmse=0.5551 holdout_corr=0.269 predict_mean_rmse=0.5452 6s`
  `VR2 id=12 runs=800 hidden=256 layers=2 params=236801 best_epoch=5 holdout_rmse=0.5654 holdout_corr=0.165 predict_mean_rmse=0.5452 7s`
  `VR2 id=13 runs=800 hidden=1024 layers=2 params=1733633 best_epoch=4 holdout_rmse=0.5644 holdout_corr=0.168 predict_mean_rmse=0.5452 18s`
  `VR2 id=14 runs=2700 hidden=64 layers=2 params=46913 best_epoch=2 holdout_rmse=0.5479 holdout_corr=0.262 predict_mean_rmse=0.5453 7s`
  `VR2 id=15 runs=2700 hidden=256 layers=2 params=236801 best_epoch=2 holdout_rmse=0.5587 holdout_corr=0.231 predict_mean_rmse=0.5453 37s`
  `VR2 id=16 runs=2700 hidden=1024 layers=2 params=1733633 best_epoch=2 holdout_rmse=0.5509 holdout_corr=0.246 predict_mean_rmse=0.5453 23s`
  `VR2 id=17 runs=8100 hidden=64 layers=2 params=46913 best_epoch=1 holdout_rmse=0.5309 holdout_corr=0.329 predict_mean_rmse=0.5451 10s`
  `VR2 id=18 runs=8100 hidden=256 layers=2 params=236801 best_epoch=1 holdout_rmse=0.5526 holdout_corr=0.315 predict_mean_rmse=0.5451 16s`
  `VR2 id=19 runs=8100 hidden=1024 layers=2 params=1733633 best_epoch=1 holdout_rmse=0.5477 holdout_corr=0.310 predict_mean_rmse=0.5451 72s`
  `VR2 id=20 runs=800 ridge lambda=1 holdout_rmse=0.5221 holdout_corr=0.289 predict_mean_rmse=0.5452 14s`
  `VR2 id=21 runs=2700 ridge lambda=1 holdout_rmse=0.5199 holdout_corr=0.304 predict_mean_rmse=0.5453 14s`
  `VR2 id=22 runs=8100 ridge lambda=0.001 holdout_rmse=0.5135 holdout_corr=0.339 predict_mean_rmse=0.5451 31s`
  `VR SELECT: vr_22 -> vr_10 (0.513487 0.339302 8100 0 0 667 0.545085); predict-mean holdout RMSE 0.545186`
  `VR GATE OK: 0.513487 < 0.545186`
  Verdict on the amendment predictions: v2 nets beat the mean at 3,000 and 9,000 runs (MED) — mostly failed: only
  h=64 at 8,100 runs does; ridge beats every v2 net at 1,000 runs (MED-HIGH) — held (and at every size); best model at
  9,000 runs is a v2 net (LOW-MED) — failed, ridge wins; gate passes (HIGH) — held. The nets rank runs (corr up to
  0.33) but overshoot in scale; ridge gains little from data (0.5221 -> 0.5199 -> 0.5135). `vr_10` = ridge on 8,100
  runs (one linear layer, 667 parameters). Integration (leak test + vrb20/50/100/50_nobf) runs as pre-registered.
  Leak test `LEAKTEST checked=87 mismatches=0 PASS`. Integration: `vrb20` 73.0% win, floor 47.4, -0.020 vs base, P=0.262 → discard (non-zero: the model is live).; `vrb50` 77.7% win, floor 47.3, +0.026 vs base, P=0.761 → discard; `vrb100` 72.7% win, floor 46.0, -0.050 vs base, P=0.107 → discard; `vrb50_nobf` 69.7% win, floor 45.5, -0.090 vs base, P=0.011 → discard (pre-registered "replacement does not beat the current agent" (MED): held).
  **Near-miss extension declared 17:27 UTC (before any dev seed):** `vrb50` (+0.026, P=0.761) qualifies under the rule of
  2026-11-01 11:40 — once, 100 dev seeds vs `mapd085_dev`; keep needs pooled-400 P >= 0.9 and dev-alone dscore > 0.
  Result: dev alone `vs mapd085_dev: n=100 dscore=+0.0775 dwin=+0.060 P(improve)=0.866`; `pooled val,dev [act3]: n=400  vrb50 win=0.767 floor=47.10  mapd085 win=0.733 floor=46.92  dscore=+0.0386 dwin=+0.035 P(improve)=0.884` -> **discard**
  (pooled P below 0.9; no second extension). Phase R stage 2 verdict: the learned run value (ridge, 8,100 runs, corr 0.34)
  adds at most a small effect next to the deck model and is far worse as its replacement (-0.090). Stage 3 (fight
  terminal value) was conditional on a stage-2 keep and does not start. The one lever left is data: ridge 0.5221 ->
  0.5199 -> 0.5135 and the small net crossing the mean only at 8,100 runs point to 10^5 runs (the protocol's optional
  step, ≈ 27 h on one VM, ≈ 7 h on four) — practical only once workers can share data (Hugging Face store pending).
- v2 first cells (pasted): `VR2 id=11 runs=800 hidden=64 layers=2 params=46913 best_epoch=2 holdout_rmse=0.5551 holdout_corr=0.269 predict_mean_rmse=0.5452 6s`; `VR2 id=12 runs=800 hidden=256 layers=2 params=236801 best_epoch=5 holdout_rmse=0.5654 holdout_corr=0.165 predict_mean_rmse=0.5452 7s`; `VR2 id=13 runs=800 hidden=1024 layers=2 params=1733633 best_epoch=4 holdout_rmse=0.5644 holdout_corr=0.168 predict_mean_rmse=0.5452 18s`.

**Phase R2 — 10^5 runs (pre-registered 2026-11-02 18:19 UTC; the protocol's optional step, unblocked by the data store).**
Why: the v2 nets are data-limited (h=64: 0.5551 / 0.5479 / 0.5309 at 800 / 2,700 / 8,100 runs, crossing the mean only
at the largest size) and ridge flattens (0.5221 / 0.5199 / 0.5135). Data: 90,000 more A0 games with the A0 best and
`rv_log=1`, seeds 350000–439999 (unused by every split and every earlier data set), three VMs × 30,000 (s2scale
350000+, a10ports 380000+, coordinator 410000+; `research/workers/r2data.md`), 32 random states per run kept for
training (`research/vf/subsample_runs.py`; full data at 10^5 runs ≈ 43 GB does not fit in RAM), full and subsampled
sets pushed to the store. Holdout unchanged: the same 1,000 runs of `rundata` (full rows), so every number stays
comparable with the grid above. Training (trainer extension to multiple data dirs written while data generates):
ridge and v2 nets h = 64 / 256 / 1,024 at ≈ 39,000 and ≈ 99,000 training runs; selection by holdout RMSE, gate vs the
mean; then a fresh card-pick test of the selected model at bonus 20 / 50 / 100 (val-300 rule vs the A0 best; a new
model, so a new candidate family — not a second extension of `vrb50`). Predictions: the best R2 model's holdout RMSE
< 0.505 (LOW–MED); a net beats ridge at ≈ 99,000 runs (MED); one card-pick run keeps (LOW).
**R2 amendment (19:22 UTC, before any R2 model is trained): per-state weights.** Writing the multi-dir trainer exposed a
weighting mismatch: winning runs log more states (3,000 runs of `rundata`: `per-run mean score 1.5645, per-state mean
1.6892, mean states/run 158.0 (wins 176.4, losses 120.8)`), the holdout is scored per state, but v2 nets sample 16
states per run (run-equal) — a ≈ 0.125 offset that alone lifts an RMSE of 0.518 to ≈ 0.533, i.e. a large part of why
v2 nets "lost to the mean". Fix (`train_vr2.py --state-weights`): each kept state of a run weighs n_original/n_kept
(original counts from `counts.tsv`, now written by `subsample_runs.py`); weighted moments, loss, ridge and validation.
Holdout runs are never read from the training dirs. Smoke set (300 train runs, h=64 / ridge; pasted):
  [--ridge] holdout_rmse=0.5292 predict_mean_rmse=0.5460 | [--hidden 64] 0.5378 / 0.5460 (both = the v2 code, unchanged)
  [--hidden 64 --state-weights] 0.5314 / 0.5460
  [--ridge --train-dirs sub --state-weights] 0.5289 / 0.5460 | [--hidden 64 --train-dirs sub --state-weights] 0.5306 / 0.5460
  [--ridge --train-dirs sub] (unweighted) 0.5414 / 0.5633
All R2 cells use `--state-weights`; R2 adds weighted re-runs at 8,100 runs (h=64/256/1,024 + ridge, full rows) so the
data-size effect is read separately from the weighting fix. Prediction: weighted v2 nets beat the mean at 8,100 runs at
every size (MED).
**R2 training plan (fixed 00:19 UTC, before any R2 data is trained on):** holdout = the same 1,000 `rundata` runs (full rows);
training dirs = `rundata_k32` (the original 9,000 training runs, 32 states each) + `rundata_w1/w2/w3_k32`; pool order =
the original 9,000 first, then the 90,000 new runs in a fixed shuffle. Cells: ridge / h=64 / 256 / 1,024 at 39,000
(ids 30–33) and 99,000 training runs (34–37), all `--state-weights`. Selection: lowest holdout RMSE among 22–25 and
30–37 -> `vr_41` (`research/tools/vr_select3.sh`), gate vs its predict-mean RMSE; leak test; `vr2b20/50/100` (val-300 vs
the A0 best). Memory: the loader preallocates (a list concatenate peaks at 2×, ≈ 17 GB at 10^5 runs).
Weighted re-runs at 8,100 runs (full rows; pasted):
  `VR2 id=23 runs=8100 hidden=64 layers=2 params=46913 best_epoch=1 holdout_rmse=0.5230 holdout_corr=0.317 predict_mean_rmse=0.5451 238s`
  `VR2 id=24 runs=8100 hidden=256 layers=2 params=236801 best_epoch=1 holdout_rmse=0.5318 holdout_corr=0.320 predict_mean_rmse=0.5451 352s`
  `VR2 id=25 runs=8100 hidden=1024 layers=2 params=1733633 best_epoch=1 holdout_rmse=0.5359 holdout_corr=0.302 predict_mean_rmse=0.5451 475s`
**R2 result:** selection `VR SELECT: vr_35 -> vr_41 (0.504600 0.386810 89100 64 2 46913 0.545105)`, `VR GATE OK`, leak test
`LEAKTEST checked=87 mismatches=0 PASS`. Card picks: `vr2b20` 81.0% win, floor 47.6, +0.065 vs base, P=0.978 → keep (pre-registered "one card-pick run keeps" (LOW): held so far).
The first keep driven by a learned value against the final score; dev replication `rep_dev.sh vr2b20 mapd085` queued. Next: `vr2b50` 79.0% win, floor 47.6, -0.021 vs base, P=0.238 → discard (vs vr2b20). Then `vr2b100` 72.7% win, floor 46.5, -0.105 vs base, P=0.002 → discard (vs vr2b20). Dev replication: `vr2b20_dev` 69.0% win, floor 46.3, +0.025 vs base, P=0.660 → info; pooled val+dev (informational): `pooled val,dev [act3]: n=400  vr2b20 win=0.780 floor=47.29  mapd085 win=0.733 floor=46.92  dscore=+0.0548 dwin=+0.048 P(improve)=0.973`. The keep holds on 400 seeds
(+4.8 win pts); the dev slice alone is weak (+1.0 pt, n=100), so the val figure (+6.0) carries some winner's curse.
**Ports of `vr2b20` (pre-registered 05:53 UTC):** the run value is trained on A0 runs only, so transfer is uncertain: `a10_vr2b20`
(A10, val-300 rule), `a20_vr2b20` (A20, pooled-500), `heart_vr2b20` (Heart, pooled-500). Predictions: A10 keeps (LOW–MED);
A20 keeps (LOW); Heart keeps (LOW). A10: `a10_vr2b20` 34.3% win, floor 38.4, +0.071 vs base, P=0.979 → keep (prediction LOW–MED: held); dev replication queued. A20: `pooled val,dev2,dev3 [act3]: n=500  a20_vr2b20 win=0.036 floor=28.60  a20_restboss1 win=0.028 floor=27.41  dscore=+0.0312 dwin=+0.008 P(improve)=0.978` -> **KEEP** (prediction LOW: beaten; every slice positive:
val +0.022, dev2 +0.041, dev3 +0.049; the gain is mostly floor, +1.2, with wins 2.8% -> 3.6%). Heart: `pooled val,dev2,dev3 [heart]: n=500  heart_vr2b20 win=0.148 floor=47.56  heart_bfrelic10 win=0.120 floor=46.05  dscore=+0.0513 dwin=+0.028 P(improve)=0.996` -> **KEEP** (prediction LOW: beaten;
slices val +0.050, dev2 +0.040, dev3 +0.067). **All four tracks keep the same A0-trained run value at bonus 20.** A10 dev replication: `a10_vr2b20_dev` 27.0% win, floor 35.6, +0.085 vs base, P=0.918 → info.
Test split: not used — the amended cap allows one test run per confirmed milestone (M-A10 / M-A20 / Heart), and none
is reached (A10 34.3% val vs 35%).

**Phase R3 — 1.9×10^5 runs (pre-registered 2026-11-03 06:10 UTC).** Why: at 89,100 runs every net is still improving with data
(h=64 0.5230 / 0.5128 / 0.5046 at 8,100 / 35,100 / 89,100; h=256 and h=1,024 only start to pay off at the largest size) while
ridge is flat, and the 89,100-run h=64 net produced keeps on all four tracks. Data: 90,000 more A0 games with the current
A0 best (`vr2b20` — note: the data-generating agent changes from R2, a small distribution shift, accepted) and `rv_log=1`,
seeds 440000–529999 (unused), three fresh workers × 30,000 (`research/workers/r3data.md`; 3,000-file commits). Training:
the R2 trainer at ≈ 1.9×10^5 training runs with 16 kept states per run (memory), ridge + h = 64 / 256 / 1,024, same
holdout; selection among all R2 + R3 cells -> new vr id, gate; then the card-pick test (bonus 10 / 20 / 40) on A0 vs the
then-best, and ports on success. Predictions: best holdout RMSE < 0.500 (LOW–MED); a net larger than h=64 is best (MED);
an A0 card-pick keep over `vr2b20` (LOW).
R3 training plan (fixed 12:17 UTC, before any R3 data exists here): ids 50–53 = ridge / h=64 / 256 / 1,024 on all ≈ 189,000
training runs, 16 kept states per run, `--state-weights`; id 54 = h=64 at 99,000 runs with 16 kept states (control for the
cap); selection over 22–25, 30–37, 50–54 -> `vr_61`, gate; leak test; `vr3b10/20/40` vs the A0 best (val-300 rule).
Trainer speed-up (no change in results): a cached file -> run-id map per data dir (`ids_cache.tsv`); the scan of
≈ 2×10^5 run files took > 20 min per cell and the first R3 cell was killed by the 2-hour limit mid-scan. Smoke set: identical
output with and without the cache (`holdout_rmse=0.5289 predict_mean_rmse=0.5460` both).
Loader bug (fixed 18:58 UTC, no change in results): the `--rows-per-run` preallocation rebuilt `set(tfiles)` once per file
(O(n²): a 40-minute CPU hang at ≈ 2×10^5 files; invisible on the 800-file smoke set). Smoke output unchanged
(`holdout_rmse=0.5294 predict_mean_rmse=0.5460`, 16 kept). Cell 50 re-run.
**R3 amendment (20:16 UTC, before cell 53 finished): `--patience 3` for the h=1,024 cell only.** At ≈ 170,000 runs it needs
≈ 7 epochs × ≈ 16 min plus loading, past the runner's 2-hour window, so it would be killed and restarted forever. Every net
trained on >= 2,700 runs so far peaked at epoch 1 or 2 (R2 ids 14–19, 31–37; R3 ids 51–52), so patience 3 is not expected to
change the selected epoch. The first attempt (patience 6) was stopped at 33 min; the cell restarts with patience 3.
R3 data (merged from the workers, pasted):
  `SUBSAMPLE research/vf/data/rundata_w4/all -> research/vf/data/rundata_w4_k32/all: 30000 runs, rows 4904478 -> 957344 (k=32)`
  `PUSH rundata_w4_k32 DONE: 30001 files in latioswang/sts-ironclad-data`
  `PUSH rundata_w4 DONE: 30000 files in latioswang/sts-ironclad-data`
  `SUBSAMPLE research/vf/data/rundata_w5/all -> research/vf/data/rundata_w5_k32/all: 30000 runs, rows 4906995 -> 957186 (k=32)`
  `PUSH rundata_w5 DONE: 30000 files in latioswang/sts-ironclad-data`
  `PUSH rundata_w5_k32 DONE: 30001 files in latioswang/sts-ironclad-data`
  `SUBSAMPLE research/vf/data/rundata_w6/all -> research/vf/data/rundata_w6_k32/all: 30000 runs, rows 4904860 -> 957471 (k=32)`
  `PUSH rundata_w6_k32 DONE: 30001 files in latioswang/sts-ironclad-data`
  `PUSH rundata_w6 DONE: 30000 files in latioswang/sts-ironclad-data`
Workers (fresh sessions, batch as their initial task): `r3w4` session_01USafwjCmHfU33M4nKBEaNm, `r3w5`
session_01USafwfxVxqFj3VUoacBkds, `r3w6` session_01USafwiFvm2TuvHQBbsV8tv (started 06:11 UTC).
Trainer option for R3 memory: `train_vr2.py --rows-per-run 16` (deterministic per run; weights unchanged since they use
the original state counts). Smoke set, ridge, 300 train runs (pasted): 32 kept `holdout_rmse=0.5289 predict_mean_rmse=0.5460`;
16 kept `holdout_rmse=0.5294 predict_mean_rmse=0.5460`.
**Bonus refinement (pre-registered 06:16 UTC, this VM while R3 data generates):** `vr2b10`, `vr2b30` vs `vr2b20` (val-300 rule).
Prediction: neither keeps (MED; the 20/50/100 curve peaks at 20 and the noise is ±0.03).
  Result: `vr2b10` 75.7% win, floor 47.9, -0.048 vs base, P=0.052 → discard. `vr2b30` 77.7% win, floor 47.1, -0.044 vs base, P=0.031 → discard. Prediction "neither keeps" (MED): held; 20 stays.
**R3 grid result (pasted):**
  `VR2 id=50 runs=170100 ridge lambda=0.001 holdout_rmse=0.5141 holdout_corr=0.341 predict_mean_rmse=0.5475 99s`
  `VR2 id=51 runs=170100 hidden=64 layers=2 params=46913 best_epoch=1 holdout_rmse=0.5019 holdout_corr=0.397 predict_mean_rmse=0.5475 184s`
  `VR2 id=52 runs=170100 hidden=256 layers=2 params=236801 best_epoch=1 holdout_rmse=0.5042 holdout_corr=0.394 predict_mean_rmse=0.5475 609s`
  `VR2 id=53 runs=170100 hidden=1024 layers=2 params=1733633 best_epoch=1 holdout_rmse=0.5074 holdout_corr=0.383 predict_mean_rmse=0.5475 4120s`
  `VR2 id=54 runs=89100 hidden=64 layers=2 params=46913 best_epoch=1 holdout_rmse=0.5079 holdout_corr=0.380 predict_mean_rmse=0.5474 75s`
  `VR SELECT: vr_51 -> vr_61 (0.501870 0.397442 170100 64 2 46913 0.547537)`
  `VR GATE OK: 0.501870 < 0.547537`
Predictions: best holdout RMSE < 0.500 (LOW–MED) — failed (0.5019); a net larger than h=64 is best (MED) — failed (h=256
0.5042, h=1,024 0.5074; the gaps to h=64 shrink with data); patience 3 for cell 53 changed nothing (best epoch 1). The
control (h=64, 89,100 runs, 16 states, mixed-policy pool) 0.5079 vs R2 vr_35 0.5046: the 16-state cap and the policy
shift cost ≈ 0.003 on the old-policy holdout; 1.9× the data more than pays for it (0.5019). `vr_61` = vr_51.
Leak test `LEAKTEST checked=87 mismatches=0 PASS`. Card picks vs `vr2b20`: `vr3b10` 77.3% win, floor 47.8, -0.033 vs base, P=0.153 → discard; `vr3b20` 78.7% win, floor 47.8, -0.019 vs base, P=0.268 → discard; `vr3b40` 82.3% win, floor 48.1, +0.023 vs base, P=0.758 → discard.
**Near-miss extension declared 21:47 UTC (before any dev seed):** `vr3b40` (+0.023, P=0.758) — once, 100 dev seeds vs `vr2b20_dev`;
keep needs pooled-400 P >= 0.9 and dev-alone dscore > 0.
  Result: dev alone `vr3b40_dev` 75.0% win, floor 47.7, +0.088 vs base, P=0.885 → info; `pooled val,dev [act3]: n=400  vr3b40 win=0.805 floor=48.02  vr2b20 win=0.780 floor=47.29  dscore=+0.0392 dwin=+0.025 P(improve)=0.900` -> **KEEP** (P exactly at the bar: the rule compares the unrounded P, and the script
  printed KEEP). New A0 best `vr3b40` (R3 run value vr_61, bonus 40). Pooled 400: 80.5% vs 78.0% for `vr2b20`.
**Ports of `vr3b40` (pre-registered 21:50 UTC):** A10 `a10_vr3b40` (val-300 vs `a10_vr2b20`), A20 `a20_vr3b40` and Heart
`heart_vr3b40` (pooled-500 vs their `*_vr2b20` bests). Predictions: each keeps (LOW–MED; the A0 gain over the R2 model is
at the edge of significance). A10: `a10_vr3b40` 35.3% win, floor 39.0, +0.021 vs base, P=0.716 → discard; near-miss extension declared 21:53 UTC (dev 100 vs `a10_vr2b20_dev`, pooled 400). A20: `pooled val,dev2,dev3 [act3]: n=500  a20_vr3b40 win=0.044 floor=28.39  a20_vr2b20 win=0.036 floor=28.60  dscore=+0.0037 dwin=+0.008 P(improve)=0.604` -> no keep. Heart: `pooled val,dev2,dev3 [heart]: n=500  heart_vr3b40 win=0.118 floor=48.05  heart_vr2b20 win=0.148 floor=47.56  dscore=-0.0210 dwin=-0.030 P(improve)=0.148` -> no keep
(Heart wins 14.8% -> 11.8%; the Heart track prefers the R2 model at bonus 20).
**R3 model at bonus 20 on the other tracks (pre-registered 05:16 UTC, while R4 waits on storage):** `a10_vr3b20` (val-300 vs
`a10_vr2b20`), `a20_vr3b20`, `heart_vr3b20` (pooled 500 vs their bests). Question: was bonus 40, not the model, what hurt
off A0? Predictions: none keeps (MED); Heart no longer loses (MED).
  Results: `a10_vr3b20` 35.3% win, floor 38.8, +0.017 vs base, P=0.684 → discard; A20 `pooled val,dev2,dev3 [act3]: n=500  a20_vr3b20 win=0.026 floor=27.81  a20_vr2b20 win=0.036 floor=28.60  dscore=-0.0253 dwin=-0.010 P(improve)=0.038` -> no; Heart `pooled val,dev2,dev3 [heart]: n=500  heart_vr3b20 win=0.116 floor=47.42  heart_vr2b20 win=0.148 floor=47.56  dscore=-0.0347 dwin=-0.032 P(improve)=0.022` -> no. Verdict: "none keeps" held; "Heart no longer
  loses" failed — the R3 model loses on A20 and the Heart at both bonus 20 and 40, so the model (trained on runs of the
  stronger A0 agent, A0 only) is the problem off A0, not only the weight. Supports R4 (track-specific data). R4 lines parked
  in `research/queue.r4parked.txt` until the storage quota is resolved. A10 extension: dev `a10_vr3b40_dev` 30.0% win, floor 38.0, +0.078 vs base, P=0.867 → info; `pooled val,dev [act3]: n=400  a10_vr3b40 win=0.340 floor=38.72  a10_vr2b20 win=0.325 floor=37.71  dscore=+0.0348 dwin=+0.015 P(improve)=0.870` -> discard
(pooled P below 0.9; no second extension). Bests stay: A10 `a10_vr2b20`, A20 `a20_vr2b20`, Heart `heart_vr2b20`; A0 `vr3b40`.



**Phase R4 — an A10 run value (pre-registered 2026-11-03 22:17 UTC).** Why: the A0-trained run value kept on every track at
bonus 20 (R2), but the sharper R3 model at bonus 40 kept only on A0 (A10 pooled-400 P=0.870, A20 P=0.604, Heart P=0.148):
the model knows A0 runs, and heavier weight exposes the mismatch. A10 is the closest track to its target (pooled 400 34.0%
vs 35%). Data: 90,000 A10 games with the A10 best (`a10_vr2b20`) and `rv_log=1`, seeds 800000–889999 (unused), three fresh
workers × 30,000 (`research/workers/r4data.md`; `sv_gen.sh` now takes ASC / PARF, defaults unchanged); holdout = the first
1,000 A10 games of W=7 (seeds 800000–800999, full rows, `rundata_a10_hold`). Training (R2/R3 trainer, `--state-weights`,
16 kept states): (a) A10 only, h=64 / 256 + ridge; (b) A10 + all A0 data, h=64 / 256; all scored on the A10 holdout.
Selection -> new vr id, gate vs its predict-mean RMSE; then A10 card picks at bonus 20 / 40 vs `a10_vr2b20` (val-300 rule).
Predictions: an A10-only net beats the A0-trained `vr_61` on the A10 holdout (MED); A0 + A10 beats A10-only (LOW–MED);
an A10 card-pick keep (LOW–MED); A10 reaches 35% on val (LOW–MED).
Workers (fresh sessions, started 22:18 UTC): `r4w7` session_01USafwfhhUVVsE85jmwM53y (also the A10 holdout),
`r4w8` session_01USafwiw7hG5CMTDQ8CrUA7, `r4w9` session_01USafwhXKwYrTj1fmyx4u2d.
Disk for R4 (coordinator): deleted the local full `rundata` (in the store: `rundata 10000 files`; R4 does not use it, its
32-state subsample `rundata_k32` stays) and the Hugging Face Xet cache; `rundata` must be re-pulled before any A0-holdout
training.
R4 training plan (fixed 23:19 UTC, before any A10 data exists here): holdout dir `rundata_a10_hold` (1,000 A10 runs, full rows);
reference `score_vr.py vr_61` on it; ids 70 / 71 / 72 = ridge / h=64 / h=256 on the A10 subsamples only (16 kept states);
ids 73 / 74 = h=64 / h=256 on A10 + all A0 subsamples (8 kept states, memory); selection among 70–74 -> `vr_81`, gate; leak
test; `a10_vr4b20`, `a10_vr4b40` vs the A10 best (val-300 rule). The A0 model `vr_61` is not in the selection (scored only).

**Phase R5 — the run value on more decisions (pre-registered 2026-11-04 17:18 UTC, before any of these games).** Why: the
learned run value V (666 features incl. relic one-hots, gold, upgrades, deck) kept on A0 card picks (`vr3b40`: card value +
40 x ΔV) but so far scores only card rewards. Three default-off hooks in `FairAgent.cpp` (build: `repro_check vr3b40 16` ->
0 of 16 games differ; leak tests with each flag on: 98 checked, 0 mismatches):
`vr_relic_w` W: boss relic = KB relic value + W x (V with the relic − V); `vr_shop` 1: shop card = KB value + vr_bonus x
(V after buying, price paid in gold − V); `vr_upg_w` W: upgrade choice = KB upgrade value + W x (V upgraded − V).
Cells (A0, each val-300 vs `vr3b40`, keep rule + near-miss extension rule as usual): `vr5relic20` (W=20), `vr5relic40`
(W=40), `vr5shop` (vr_shop=1), `vr5upg40` (W=40). Model: `vr_61` (the A0 model). Ports to other tracks only after an A0 keep.
Predictions: each cell a keep LOW (boss relics: one decision per act, large stakes, the KB value already decent; shop
cards: rarer than card rewards; upgrades: KB value is a weak prior, so the most room — LOW–MED).

**Phase P1 — cheaper search: a move prior that keeps strength at 50 simulations (owner-approved: "lets do all three",
"approve, and allow worker budget" — up to 10 parallel workers; pre-registered 2026-11-05 09:47 UTC).** Why: the search is
≈ 98% of game cost (A0, 100 games: 500 sims 110 s / 83 wins; 50 sims 10.5 s / 62; 5 sims 1.8 s / 33). Diagnostic on 200
train games (seeds 400000+): 50 sims + the current prior 143 wins (71.5%), 50 sims without it 109 (54.5%), 500 sims without
it 166 (83%) — the prior is worth +17 points at 50 sims, and an 11.5-point gap to 500 sims remains at 8.8x less time. The
prior (v1: 298k decisions, 500-sim targets, root only) plateaued against 500-sim targets (v3 neutral). Two levers: how the
prior is used (depth, exploration constant) and what it learns from (stronger, 2,000-sim targets). **Runs on the fidelity
simulator** (fork `dfda16c`, built at `/home/user/sts_lightspeed_fid`; leak test 94 checked, 0 mismatches; 100 A0 games old
sim 84 wins / floor 48.3 / 99 s vs fork 81 / 47.3 / 104 s, 0 aborted) — every P1 comparison has both arms on the fork.
Stage 1 (this VM, now): at sims=50 on the fork, prior use `prior_mlp_depth` ∈ {0, 1, 2} x `prior_c` ∈ {1.0, 1.5, 3.0},
200 train games each (seeds 400000–400199; tuning only, never a keep), plus the 500-sim best as the reference on the same
seeds. Stage 2 (seven workers p1w16–p1w22, `research/workers/p1data.md`): 28,000 A0 games of the best on the fork with
`pf_log=1 sims=2000`, seeds 3,000,000–3,603,999 (unused). Training: prior v4 (`train_policy_mlp.py`, hidden 64 / 128 / 256)
on the 2,000-sim targets; each candidate compiled into a side build (consistency rule). Evaluation (fresh-slice rule, fork,
both arms re-run): the 50-sim agent with the best stage-1 usage and prior v4 vs the same with prior v1, and vs the 500-sim
best (the gap). Goal: the 50-sim agent within 3 points of the 500-sim best. Predictions: stage 1 closes ≥ 3 of the 11.5
points (MED); prior v4 at 50 sims beats v1 at 50 sims (MED); within 3 points of 500 sims (LOW).

**Phase X1 — exploration data for the run value (owner-approved direction "do that", 2026-11-05; pre-registered 09:15 UTC,
before any of these games).** Why: V is trained on states the KB-guided agent reaches, so it has rarely seen the outcome of a
choice the KB avoids — and the decisions use exactly those counterfactual differences. Every pure-V replacement so far lost
(Phase C `rvpick`, deck model as replacement −10 to −16, `vrb50_nobf` −0.090, R6 rest/removal). Hooks (default off; build:
`repro_check` vr5relic20 / a10_r81 / a20_relic20: 0 of 16 differ each; leak tests with each on: 98 checked, 0 mismatches;
`explore_eps=0.2` changes 18 of 20 games): `explore_eps` p (card rewards and boss relics picked uniformly at random with
probability p; own seeded RNG; data generation only, never in an evaluated config), `vr_pure` 1 (card picks by V alone: the
card with the largest V gain if > `vr_pure_margin`, default 0, else skip), `vr_pure_relic` 1 (boss relic = argmax V).
Data: 90,000 A0 games of `vr5relic20` + explore_eps=0.2, seeds 2,000,000–2,089,999 (unused; the fresh-slice pool stays below
2,000,000), three workers (`research/workers/x1data.md`), shards; exploration holdout = first 1,000 games of W=13.
Training (same trainer, `--state-weights`): ids 140 / 141 = h=64 / h=256 on the exploration data only (16 states);
142 / 143 = h=64 / h=256 on exploration + all A0 + A10 + A20 subsamples (6 states); scored on the exploration holdout (the
selection target: counterfactual states) and on the fresh A0 holdout (on-policy); selection 140–143 -> `vr_151` by the
exploration-holdout RMSE, gate. Evaluation (fresh-slice rule, A0, vs the then A0 best): `x1add` (vr_model=151, inherited
weights), `x1pure` (vr_model=151, vr_pure=1, vr_pure_relic=1: no KB on cards and boss relics), control `x1pure61` (same
with `vr_61`). Predictions: `x1add` keep LOW–MED; `x1pure` keep LOW; `x1pure` beats `x1pure61` (MED — the point of the
exploration data).

**Honest re-measurement of the current bests (pre-registered 2026-11-05 08:16 UTC; report only, no decision).** Slices
allocated now: A0 `vr5relic20` on fs1203000n500; A10 `a10_r81` on fs1203500n1000; Heart `heart_relic20` on
fs1204500n1000 (Heart win = Heart beaten). (A20 `a20_relic20`: 4.4% on the 1,000 `fresh` seeds, already measured.)
Predictions: each below its selection-slice number — A0 < 84.7% (MED), A10 < 44.0% (HIGH), Heart < 15.0% (MED).

**PROTOCOL AMENDMENT — fresh-slice keep rule (adopted 2026-11-05 07:50 UTC; applies to every keep decision from now on).**
Problem (shown above): the bests were selected on the val/dev slices that every later challenger is compared on, so the
base's score there is the maximum of many noisy tries — challengers lose by the selection gain (A20: ≈ 1–3 win points;
`a20_bm2x` "−3.0, P=0.005" was a dead heat on fresh seeds). New rule (`research/tools/exp_fresh.sh`): each experiment
allocates a never-used seed slice from `research/eval/fresh_next.txt` (starts at 1,201,000; 500 seeds on A0/A10, 1,000 on
A20/Heart) before any game, re-runs the current best on that slice, and keeps the challenger iff the paired dscore > 0 and
P >= 0.9 on that slice alone (Heart: win = Heart beaten, as before). The slice is never reused, so no best is ever compared on
seeds that selected it. Old val/dev/dev2/dev3 results stay as history; near-miss extensions use a second fresh slice. Cost:
2x games per decision. The test split rule is unchanged. `run.py` accepts split names `fs<START>n<COUNT>`.
Re-tests of today's A20 discards that were judged against the inflated base (compute-neutral ones only): `a20_bm15f`
(sims 405, boss_mult 1.5) and `a20_r121s32f` (vr_model 121, bonus 32, relic 32), each on its own fresh 1,000-seed slice vs
`a20_relic20`. Predictions: each keep LOW.

**Selection-bias diagnostic on A20 (pre-registered 2026-11-05 07:23 UTC, before any of these games; report only).** All
five A20 search changes today lost 1.8–3.2 points vs `a20_relic20` on the pooled 500 (bm15 3.8%, bm2 3.0%, bm2x 2.6%,
bm05 2.4%, s400 3.8% vs 5.6%), in both directions of compute — the pattern expected if `a20_relic20`'s 5.6% is inflated by
having been selected on those same 500 seeds [INFERRED, MED]. New split `fresh` (seeds 1,200,000–1,200,999, unused; added
to `research/eval/run.py`; diagnostic only, never used for a keep or discard). Runs at A20: `a20_relic20`, `a20_vr2b20` (the
previous best), `a20_bm2x`. Predictions: `a20_relic20` on fresh < 5.6% (MED); the relic gain vs `a20_vr2b20` on fresh smaller
than the pooled +2.0 points (MED); `a20_bm2x` within 1 point of `a20_relic20` on fresh (LOW–MED).

**Phase B20b — less search on A20 (pre-registered 2026-11-05 07:17 UTC, before any of these games).** Why: B20's
+compute cell (bosses x2 at no cost elsewhere) lost 3 points (P=0.005), so either the search misbehaves at higher
simulation counts or 500 is past the useful point at A20. Cells (pooled-500 vs `a20_relic20`; both use less compute, so
both are keep candidates): `a20_bm05` (boss_mult=0.5: bosses at 250) and `a20_s400` (sims=400 everywhere). Predictions:
`a20_bm05` keep LOW, `a20_s400` keep LOW; if both lose, 500 is near a local optimum and the B20 +compute loss stays
unexplained (and worth a mechanism check: potion use / rollout horizon at high sims).

**Phase B20 — A20 search budget moved to boss fights (pre-registered 2026-11-05 06:18 UTC, before any of these games).**
Why: the A20 best (`a20_relic20`, pooled 500: 28 wins) dies at a boss in 186 of 472 losses (floor 16: 84, floor 33: 102;
computed from its val/dev2/dev3 rows). Calibration (train seeds 300000–300099, used only for timing, never for keeps):
100 games at boss_mult=1 -> 156.8 s summed, at boss_mult=2 -> 230.3 s, so boss fights take ≈ 0.469 of A20 search compute.
Compute-neutral cells: non-boss sims x = 500 / (0.531 + 0.469 m): `a20_bm15` (sims=405, boss_mult=1.5; boss 608) and
`a20_bm2` (sims=340, boss_mult=2; boss 680), pooled-500 rule vs `a20_relic20`; each cell's summed secs reported against the
base (neutral within ±5% or flagged). Informational, +47% compute (not a keep candidate under the equal-compute rule):
`a20_bm2x` (sims=500, boss_mult=2). History: A0 `realloc` (400 sims, bosses x2.25) lost (−0.042, P=0.067); Heart `heart_hm2b`
(compute-neutral, 2x at the Heart) +0.022, P=0.83. Predictions: `a20_bm15` keep LOW–MED, `a20_bm2` keep LOW (the other
fights lose 32% of their search), `a20_bm2x` positive MED.

**Phase R10 — the mixed-data run value on A0 (pre-registered 2026-11-05 05:28 UTC, before any of these games).** Fit on a
fresh A0 holdout (1,000 games of the current A0 best, seeds 1,100,000–1,100,999, unused, in no model's training data;
`rundata` was in-sample for `vr_81`/`vr_121`):

    SCORE vr_41  on rundata_a0_fresh_z: runs=1000 states=166753 rmse=0.4277 corr=0.283 state_mean=1.845
    SCORE vr_61  on rundata_a0_fresh_z: runs=1000 states=166753 rmse=0.4769 corr=0.146
    SCORE vr_81  on rundata_a0_fresh_z: runs=1000 states=166753 rmse=1.6605 corr=0.041
    SCORE vr_121 on rundata_a0_fresh_z: runs=1000 states=166753 rmse=0.4012 corr=0.328
    prediction SD on these states: vr_41 0.2484, vr_61 0.2985, vr_121 0.1855

The A0 best's model `vr_61` is the second-worst fit on the current agent's games (policy shift: it was trained on a weaker
agent's runs); `vr_121` (A0 + A10 + A20 data) fits best. Cells (A0 val-300 vs `vr5relic20`, usual rules; leak test A0 params +
vr_model=121: `LEAKTEST checked=98 mismatches=0 PASS`): `a0_r121` = A0 best + vr_model=121 (inherited weights: cards 40, relics 20); `a0_r121s` = + vr_bonus=64,
vr_relic_w=32 (scale-matched: x 0.2985/0.1855 = 1.61, set from prediction spread only). Predictions: `a0_r121` keep LOW–MED;
`a0_r121s` better than `a0_r121` LOW (R8b: scale matching did not rescue A20).

**Phase R9 — the run value on shop relics (pre-registered 2026-11-05 01:17 UTC, before any of these games).** Why: the run
value kept wherever it chose between *items* (boss relics on all four tracks, card picks) and lost where it replaced HP /
removal rules (R6). Shop relics are an item choice made today by slot order (the first affordable relic is bought). Hook
`vr_shop_relic` (default off; build: `repro_check vr5relic20 16` -> 0 of 16 differ; leak tests: 98 checked, 0 mismatches
for both modes): among affordable shop relics, the one with the largest V gain with its price paid in gold; mode 1 always
buys it (only the choice among relics changes), mode 2 buys only if the gain > 0 (otherwise the shop goes on to potions and
cards). Cells (A0 val-300 vs `vr5relic20`, usual rules): `vr9srel` (mode 1), `vr9srelg` (mode 2). Scale note (no outcome
used): prediction SD on the A10 holdout `vr_41` 0.3705, `vr_81` 0.3582 — same scale, so `a10_r81` had no scale mismatch; with
R8b failing, the scale explanation is not pursued further. Predictions: `vr9srel` keep LOW–MED; `vr9srelg` keep LOW.

**Phase R8 results (pasted 2026-11-05 00:42 UTC) and R8b (pre-registered 00:42 UTC, before any R8b game).**

    VR2 id=102 runs=80100 hidden=256 best_epoch=1 holdout_rmse=0.3597 holdout_corr=0.537 predict_mean_rmse=0.4233
    VR2 id=103 runs=332100 hidden=64 best_epoch=2 holdout_rmse=0.3384 holdout_corr=0.603 predict_mean_rmse=0.8362
    VR2 id=104 runs=332100 hidden=256 best_epoch=1 holdout_rmse=0.3439 holdout_corr=0.587 predict_mean_rmse=0.8362
    VR SELECT: vr_103 -> vr_121; VR GATE OK: 0.338425 < 0.836155 (also < the A20-only predict-mean 0.4233)
    LEAKTEST checked=107 mismatches=0 PASS (A20 params + vr_model=121)
    a20_r121 pooled val,dev2,dev3: n=500 win=0.036 floor=28.07 vs a20_relic20 win=0.056 floor=28.93
             dscore=-0.0366 dwin=-0.020 P=0.009 -> discard (slices -0.037 / -0.041 / -0.033)

Predictions: A20-only beats `vr_61`/`vr_81` on the A20 holdout — yes (0.345 vs 0.880 / 0.421); mixed beats A20-only — yes
(0.338 vs 0.345); A20 keep (LOW–MED) — failed. The A20 win rate fell back to exactly the pre-relic best's 3.6%.
Scale check (computed after the result, no outcome used): prediction SD over the 93,746 A20 holdout states — `vr_41` 0.4371
(the model the A20 best was tuned with), `vr_61` 0.2667, `vr_81` 0.3399, `vr_121` 0.2740. With the same bonus 20, `vr_121`
pulls ≈ 0.63x as hard as `vr_41`, i.e. both hooks were weakened. [INFERRED, post-hoc: it accommodates the result; the
cell below tests whether it predicts.] **R8b:** `a20_r121s32` = A20 best + vr_model=121, vr_bonus=32, vr_relic_w=32
(20 x 0.4371/0.2740 = 31.9; set from prediction spread only), pooled-500 rule vs `a20_relic20`. Prediction: keep LOW.

**Phase R8 — an A20 run value (pre-registered 2026-11-04 20:16 UTC, before any A20 data exists).** Why: the A10-trained
`vr_81` kept on A10 (R7, +5.3 val, +7.0 dev) but lost on A20 (pooled −1.6 win, every slice negative): the run value must
be trained on the track's own games. A20 is the weakest track (5.6% vs the 25% target). Data: 90,000 A20 games with the A20
best (`a20_relic20`) and `rv_log=1`, seeds 1,000,000–1,089,999 (unused; clear of test 900000–900499), three fresh workers
× 30,000 (`research/workers/r8data.md`), shipped as compressed shards (`<name>_z`); holdout = the first 1,000 games of W=10
(seeds 1,000,000–1,000,999, full rows, `rundata_a20_hold_z`). Training (same trainer, `--state-weights`): ids 100 / 101 / 102
= ridge / h=64 / h=256 on the A20 subsamples only (16 kept states); 103 / 104 = h=64 / h=256 on A20 + A10 + A0 subsamples
(6 kept states, memory); all scored on the A20 holdout; `vr_61` and `vr_81` scored for reference. Selection 100–104 -> `vr_121`
(`vr_select3.sh`, gate), leak test, then `a20_r121` (A20 best + vr_model=121, cards and relics) vs `a20_relic20` (pooled-500
rule). Predictions: an A20-only net beats `vr_61`/`vr_81` on the A20 holdout (MED); the mixed-data net beats A20-only (MED);
an A20 keep (LOW–MED).

**Phase R7 — the A10-trained run value under the relic keep (pre-registered 2026-11-04 19:16 UTC, before any of these games).**
Why: A10/A20 bests use the A0-trained `vr_41` for card picks (bonus 20) and boss relics (w 20); R4's `vr_81` (A10 + A0 data)
fits A10 states far better (A10 holdout RMSE 0.544 vs 0.709 for `vr_61`), and with card picks alone it was a near miss
(pooled 400 P=0.802). Cells: `a10_r81` (A10 best + vr_model=81: both hooks use `vr_81`; val-300 rule), `a20_r81` (A20 best +
vr_model=81; pooled-500 rule). Leak test A10 params + vr_model=81: `LEAKTEST checked=105 mismatches=0 PASS`. Predictions: A10 keep LOW–MED,
A20 keep LOW. No A10 test run follows (the M-A10 run is spent).

**Phase R6 — the run value on rest sites and card removal (pre-registered 2026-11-04 18:17 UTC, before any of these games).**
Why: boss relics (one decision with a clear hypothetical state) kept on all four tracks; rest sites and removals are the
same kind of decision. Hooks (default off; build: `repro_check vr5relic20 16` -> 0 of 16 differ; leak tests with each on: 98
checked, 0 mismatches): `vr_rest` 1: when both are possible, rest if V(healed by 30% max HP, +15 Regal Pillow) >= max over
upgradeable cards of V(upgraded), else smith (replaces the HP-threshold rule; the ruby-key rule still comes first);
`vr_remove` 1: on removal screens a curse first, else the card whose removal gives the highest V (replaces the fixed
curse > Strike > Defend > lowest-value order). Cells (A0 val-300 vs `vr5relic20`, usual rules): `vr6rest`, `vr6rm`,
plus the relic-weight refinement `vr6relic10` (vr_relic_w=10; 40 already lost). Predictions: `vr6rest` keep LOW–MED
(the threshold rule is crude), `vr6rm` keep LOW (the fixed order is near-optimal early), `vr6relic10` keep LOW.

**Phase R5 results (pasted 17:32 UTC) and ports (pre-registered 17:32 UTC, before any port game).**

    vr5relic20 val-300: win 0.847 floor 48.38 vs vr3b40: dscore +0.0286 dwin +0.023 P=0.951 -> KEEP (new A0 best)
    vr5relic20_dev:     win 0.810 vs vr3b40_dev: dscore +0.0696 dwin +0.060 P=0.983 (info; replicates)
    vr5relic40 vs vr5relic20: win 0.840, dscore -0.0080, P=0.000 -> discard
    vr5shop    vs vr5relic20: win 0.847, dscore -0.0035, P=0.417 -> discard
    vr5upg40   vs vr5relic20: win 0.847, dscore +0.0017, P=0.540 -> discard

Ports of `vr5relic20` (boss relics + 20 x run-value gain, each track's own best `vr_model`, i.e. `vr_41`):
`a10_relic20` (A10, val-300 rule), `a20_relic20` and `heart_relic20` (pooled-500 rule, `exp_pooled.sh`). The Heart track
picks boss relics with simulated fights (`bf_relic_w=10`), which returned before the new term; the term now adds on that
path too (default off; after the rebuild `repro_check` vr5relic20 / heart_vr2b20 / a10_vr2b20: 0 of 16 differ each; leak
test Heart params + vr_relic_w=20: 92 checked, 0 mismatches). Predictions: A10 keep MED-LOW, A20 keep LOW (few runs reach a
boss relic decision that matters), Heart keep LOW–MED.

**Phase R4-local — A10 data generated on this VM while the store is full (pre-registered 2026-11-04 06:23 UTC, before any
of these games exist).** Why: the 90,000 worker games cannot reach this VM (HF private storage full; user decision pending)
and this VM's 4 vCPUs are idle. Data: 30,000 A10 games, same agent/params/logger as R4 (`a10_vr2b20`, `rv_log=1`), seeds
920000–949999 (unused; clear of test 900000–900499), six blocks of 5,000 (`rundata_a10_l0..l5`); each block is subsampled
to 32 states/run (`rundata_a10_lB_k32`) and its full rows deleted (disk: 7.3 GB free). Also pulled: the 15,000 files of
`rundata_a10_w7_k32` already in the store; rule fixed now: that partial set joins the training dirs iff its `counts.tsv`
arrived (else it is deleted unused). Holdout: the R4 holdout `rundata_a10_hold` (already here, seeds 800000–800999; the
trainer drops holdout ids from train dirs). Cells (same trainer and options as R4): ids 75 / 76 / 77 = ridge / h=64 / h=256 on
A10 only (16 kept states); 78 / 79 = h=64 / h=256 on A10 + all A0 subsamples (8 kept states); selection among 75–79 ->
`vr_82` (`vr_select3.sh`, gate vs predict-mean); leak test; `a10_vr4lb20`, `a10_vr4lb40` vs the A10 best (val-300 rule,
near-miss extension rule as usual). `vr_61` scored on the holdout for reference. If storage is fixed later, the original
R4 (ids 70–74 -> `vr_81`, 90,000 games) still runs as pre-registered; R4-local does not replace it.
Predictions: an A10-only net beats `vr_61` on the A10 holdout (MED); A10 card-pick keep at bonus 20 or 40 (LOW).

**R4-local results (pasted 09:57 UTC).** Data: 30,000 local A10 games + the 15,000-run partial `rundata_a10_w7_k32` (its
`counts.tsv` arrived, so it joined by the rule); 40,050 A10 training runs after the holdout and validation split.

    SCORE vr_61.txt on rundata_a10_hold: runs=1000 states=129910 rmse=0.7093 corr=0.472
    VR2 id=75 runs=40050 ridge lambda=0.001 holdout_rmse=0.5573 holdout_corr=0.507 predict_mean_rmse=0.6466
    VR2 id=76 runs=40050 hidden=64  best_epoch=1 holdout_rmse=0.5615 holdout_corr=0.499 predict_mean_rmse=0.6466
    VR2 id=77 runs=40050 hidden=256 best_epoch=1 holdout_rmse=0.5730 holdout_corr=0.475 predict_mean_rmse=0.6466
    VR2 id=78 runs=211050 hidden=64  best_epoch=1 holdout_rmse=0.5505 holdout_corr=0.534 predict_mean_rmse=0.7815
    VR2 id=79 runs=211050 hidden=256 best_epoch=1 holdout_rmse=0.5481 holdout_corr=0.531 predict_mean_rmse=0.7815
    VR SELECT: vr_79 -> vr_82; VR GATE OK: 0.548130 < 0.781471
    LEAKTEST checked=87 mismatches=0 PASS (vr_model=82 vr_bonus=20)
    a10_vr4lb20 val-300: win 0.337 (101/300) vs a10_vr2b20 0.343 (103/300); dscore -0.0059, P=0.437 -> discard

Reading: prediction 1 (A10-only beats `vr_61` on the A10 holdout) holds by a wide margin (0.557 vs 0.709) [the A0 model
mis-levels A10: its state mean is off]. A0 + A10 beats A10-only slightly (0.548 vs 0.557; prediction 2 holds, small).
Caveat: the gate's predict-mean baseline for 78/79 (0.7815) is the train mean of mixed A0 + A10 data, a weaker reference
than the A10-only 0.6466; vr_82 also clears the stricter one. A better-fitting model did not change A10 card picks at
bonus 20. At bonus 40: `a10_vr4lb40` val-300 win 0.370 (111/300) vs 0.343, floor 38.97, dscore +0.0374, P=0.856 ->
discard by the val rule, a near miss (0.7 <= P < 0.9): **extension declared 09:57 UTC, before any dev seed** (rule of
2026-11-01 11:40): dev 100 vs `a10_vr2b20_dev`, pooled 400; keep needs pooled-400 P >= 0.9 (unrounded) and dev-alone dscore > 0.
Extension result: dev `a10_vr4lb40_dev` 26.0% win, floor 37.11, +0.0204 vs base, P=0.614 (info);
`pooled val,dev [act3]: n=400  a10_vr4lb40 win=0.343 floor=38.51  a10_vr2b20 win=0.325 floor=37.71  dscore=+0.0331 dwin=+0.018
P(improve)=0.855` -> **discard** (P < 0.9). Second A10 near miss at bonus 40 in a row (R3 model: pooled P=0.870): the sign is
positive both times, the size is below what 400 games resolve. R4-local closed; R4 proper (90k games) still the planned test.

**Phase S2 first result — the distribution mismatch is real (pasted, 2026-11-02 12:55 UTC).** Leaf data: 2,500
games, 815,531 tree-leaf states (labels = mean of 8 rollouts). `sv11` (h=42, leaf-trained): holdout RMSE 12.060 vs
34.865 predicting the mean. Root-trained Phase S nets scored on the same leaf holdout (`research/analysis/cross_eval_sv.py`):

    leaf holdout: 50000 states; predict-mean RMSE 34.86
    sv_7: RMSE on leaf states 15.13  bias +2.81  corr 0.905
    sv_8: RMSE on leaf states 15.63  bias +2.78  corr 0.899
    sv_9: RMSE on leaf states 15.72  bias +3.05  corr 0.898
    sv_11: RMSE on leaf states 12.06  bias +0.06  corr 0.938

On the states the search actually asks about, the three root-trained nets are equally bad (RMSE 15.1–15.7, bias ≈ +3,
i.e. they overrate leaves) although their root-holdout RMSE was 10.2 / 8.0 / 5.65: the larger nets fit the root
distribution more tightly and transfer no better. That explains both Phase S puzzles — why more simulations did not
help (a biased leaf) and why the cheapest net played best (equal leaf error, more simulations). The leaf-trained
small net cuts the leaf error by ≈ 20% and removes the bias. Pre-registered "far below" (MED): partly — 12.1 vs
15.1 is a clear but not a large reduction. Play tests decide.

- **`sv11` play test (leaf-trained, h=42, 685 sims at equal wall-clock; calibration 0.730×): win=0.420 floor=41.30
  dscore=-0.4489 P=0.000** — the best learned leaf so far, +0.12 over the best root-trained cell (`sv7` −0.5653, 36.0%)
  with fewer simulations (685 vs 1,132). Pre-registered prediction "sv11 better than sv7" (MED): held. Still 0.45 below
  random rollouts; "any cell ≥ 0" (LOW): not met. Per the pre-registration, S2 is worth scaling (more leaf data, the
  cost-aware size) — `sv12` (h=220) play test next.

- `sv12` play test (leaf-trained, h=220, 87 sims at equal wall-clock; calibration 5.734×): win=0.300 floor=39.02
  dscore=-0.6135 P=0.000 — worse than `sv11` despite the lower leaf RMSE (10.56 vs 12.06): at equal wall-clock the
  cost-aware size on this CPU is the small net. Leaf-data slope for h=42 (holdout RMSE, pasted): 10⁴ rows 17.887 →
  10⁵ 13.363 → 7.7×10⁵ 12.060 (−10% for the last 7.7×; flattening but positive). h=96 on all rows (pasted): SV id=15 rows=765531 hidden=96 layers=2 params=29089 holdout_rmse=11.295 predict_mean_rmse=34.865.
- **S2 scale-up (pre-registered 13:15 UTC, queued after Phase R):** 7,500 more leaf-logged games (seeds 340000+,
  ≈ 3.3×10⁶ leaf rows in total), models `sv16` (h=42) and `sv17` (h=96) on all rows, equal-wall-clock play tests.
  Predictions: leaf RMSE for h=42 drops ≈ 5% (MED); `sv16` beats `sv11` (−0.45) by a small margin (LOW–MED); no cell
  reaches 0 (HIGH). If `sv16` ≤ `sv11`, the leaf-value line is closed on this CPU.
  **Result (worker s2scale, merged 17:43 UTC; equal wall-clock vs `mapd085`):** `sv16` 48.7% win, floor 43.0, -0.348 vs base, P=0.000 → scaling; `sv17` 45.3% win, floor 42.9, -0.384 vs base, P=0.000 → scaling.
  Predictions: h=42 leaf RMSE −5% (MED) — partly (12.060 -> 11.736, −2.7%, different holdout); `sv16` beats `sv11` by a
  small margin (LOW–MED) — held, by more than small (−0.348 vs −0.449); no cell reaches 0 (HIGH) — held. The close
  condition (`sv16` ≤ `sv11`) is not met, so the line stays open: 4× leaf data bought +0.10 at equal wall-clock. sv17
  fits better but pays its 1.415× cost (354 sims) and plays worse — on CPU the small net remains the cost-aware size.
  Next S2 step needs more leaf data again (the slope is the open question), which needs workers to share data.

## Magic-number audit (2026-11-02 11:05 UTC, prompted by the user: "only the final score is fixed; every other number should be learned")

Inventory [computed from the code and results.tsv]: 161 named parameters; 97 have been tested at least once by the
keep loop (coordinate search against the real objective on val seeds); 51 never (excluding on/off switches),
including all 8 deck-model head weights; plus tables taken from the knowledge base (73 card values, 21 boss-relic
values, synergy pairs, event rules) and hard-coded constants (encounter weights in the hypothetical-fight sampler).

First test — learn the deck model's 8 head weights against the real run score (`research/analysis/fit_bf_heads.py`,
model 1, 1,000 held-out A0 runs bg1b + bg1k3, 30,956 base decisions, run-grouped 5-fold CV; pasted):
    model bf_g1: 30956 base decisions from 1000 runs; run score mean 1.708 sd 0.536
    hand-set formula (bfValue)      CV corr with run score: +0.288
    fitted linear combination       CV corr with run score: +0.301
    fitted weights (all data), scaled so boss_win = 1: boss_win=+1.00 boss_hp=-0.61 hall_win=+10.99 hall_hp=+2.11 boss_dmg=+0.67 next_win=+1.46 next_hp=-1.66 next_dmg=+1.77
    hand weights:                                     boss_win=+1.00 boss_hp=+0.50 hall_win=+0.50 hall_hp=+0.25 boss_dmg=+0.50 next_win=+0.50 next_hp=+0.25 next_dmg=+1.00
Reading: learning the weights moves the CV correlation from 0.288 to 0.301 — the hand weights were not the problem.
The fitted weights are unstable (negative HP weights, hall_win ×11) because the heads are collinear (boss win/HP/
damage 0.71–0.88, next-boss heads 0.73–0.93): eight heads carry roughly three independent signals. The ceiling is
how well the heads predict the run, not how they are weighted. Caveat: this scores the level of a state; card
choice uses the with-minus-without difference, which this test does not measure.

**Phase S gate result (pasted, 2026-11-02 12:13 UTC).** `sv9_s500` (8.1×10⁵ × 9×10⁵, 500 sims, cost ignored, val100):
n=98 dscore=−0.5724 dwin=−0.408 P=0.000. Two games hit the evaluator's 900 s hang guard (≈ 230 s/game average at 500
sims) and count as crashes — a budget artifact, not a simulator fault. Same 100 seeds, for comparison:

    sv9_s500 (500 sims)        win=0.357 floor=39.20 dscore=-0.5724
    sv9 (50 sims)              win=0.290 floor=37.35 dscore=-0.6820
    sv7 (small, 1132 sims)     win=0.350 floor=37.42 dscore=-0.6206
    sv5_s500 (mid, 500 sims)   win=0.330 floor=34.99 dscore=-0.6882
    svctrl (heuristic leaf)    win=0.160 floor=31.85 dscore=-0.9198
    base mapd085               win=0.770 floor=47.65

**Decision (pre-registered rule): Phase S closes; no batched evaluator, no 10⁷-decision run.** With inference cost
removed, the best-fitting net is the best leaf (+0.05 over the small net at 1,132 sims; +0.11 over itself at 50 sims),
so value quality does matter — but it still sits 0.57 below random rollouts. A fast evaluator would buy that +0.05
to +0.11, not the gap. Phase S2 (leaf-distribution training) still runs as pre-registered: it tests the one remaining
explanation (train/use mismatch), not more scale.

## Phase S2 — train the leaf value on the leaf distribution (pre-registered 2026-11-02 10:45 UTC)

**Why.** Phase S trained on decision-start states (the search's root value). The search asks the leaf model about
tree leaves: mid-turn states reached by arbitrary action sequences, including bad ones. `sv5_s500` showed more
simulations do not help a root-trained net (−0.70 at 150 sims, −0.72 at 500), and the small net with the worse
holdout RMSE plays better than the mid net, both consistent with a train/use distribution mismatch.

**What.** `leaf_log=1` (default off; games bit-identical with logging on, verified on 2 games): with probability
`leaf_log_p` = 0.002 a new tree leaf is logged with the mean of `leaf_log_k` = 8 extra rollouts from it (separate RNG,
search RNG and counters saved and restored). Smoke test (2 games): 507 rows, label mean 82.7, label sd 32.4, within-leaf
rollout sd 9.2 → label noise ≈ 3.3. Queued behind the pending `sv9_s500` read: guarded rebuild; 2,500 games (seeds
320000+, ≈ 6×10⁵ leaf rows expected); models 11 (h=42) and 12 (h=220) on all rows; leak test; equal-wall-clock
play tests vs `mapd085` (status `scaling`, informational; promotion only through the ordinary keep rule + dev).

**Predictions.** Leaf-holdout RMSE far below the root-trained nets' (MED). Play: `sv11` better than `sv7` (−0.57) at
equal wall-clock (MED); any cell at or above 0 (LOW). If `sv11` ≈ `sv7`, the mismatch is not the bottleneck and the
learned-leaf line closes; if `sv11` clearly beats `sv7`, scale S2 (more data, the cost-aware size).

## Pooled-500 keep rule for the evaluation-limited tracks (pre-registered 2026-11-02 00:23 UTC)

The Heart and A20 tracks are evaluation-limited: a 2-point Heart-win effect is one or two runs
on 100 seeds, and A20 wins are 1–4%, so val-300 keeps there have twice failed to replicate. From
batch 48 on, **A20 and Heart keep decisions use `research/tools/exp_pooled.sh`**: the candidate
runs val (300) + dev2 (100) + dev3 (100) with the same seeds as the base, and the keep rule is the
same paired bootstrap on the pooled 500 (dscore > 0 and P ≥ 0.9). `dev` (100000+) stays the
untouched replication slice for anything kept; `test` is unchanged. A0 and A10 keep the val-300
rule with the near-miss extension. Base slices come from `research/tools/base_slices.sh` (the
existing `_dev2`/`_dev3` replication runs are copied under the base's name; missing ones are run).

**Pooled near-miss extension (pre-registered 2026-11-02 01:48 UTC, before the dev run).** A pooled-500
result with dscore > 0 and 0.7 ≤ P < 0.9 may be extended exactly once with the dev slice (100 seeds);
keep requires pooled-600 P ≥ 0.9 AND dev-alone dscore > 0 (`research/tools/ext_pooled.sh`). Declared on
seeing `a20_esim_a23` at +0.0057, P=0.759 — recorded as such; the rule applies to every later pooled
near-miss unchanged. A keep by extension is labelled "extension keep" and still gets no further slice
(dev is consumed by the extension).

### Batch 48 (queued 2026-11-02 00:23 UTC) — mechanisms, not knobs

Diagnosis from the recorded bests (deaths by act/encounter, `death_enc`): Heart track (val 300):
104 deaths at the Heart, 66 at the act-3 boss, 44 at the act-2 boss; potions_left at death 0.11.
**CORRECTION (00:50 UTC): I first wrote "0 key-less act-3 clears" — that count was made on a file
loaded in Heart win mode, which zeroes exactly those wins. Counted on the raw column: val 37/300,
dev 17/100, dev2 12/100, dev3 19/100 = 85 of 600 runs (14%) beat the act-3 boss without all three
keys and so never reach the Heart.** These are the strong runs; at the 20% Heart conversion they
would add ≈ 3 points of Heart wins, more if the stronger decks convert better. Which key is
missing is being measured (in-process replay of the val games prints the keys held). A20 (val 300): 81 die in act 1 (53 at the
boss), 156 in act 2 (64 at the boss, 40 at elites: Slavers 19, Gremlin Leader 11, Book 10), 51 in
act 3; 150 of 288 deaths are boss fights. In a boss fight the random rollouts cannot win, the
search's only gradient is the loss value `10 × damage fraction + 0.2 × turns survived` — the
player's own HP does not appear in it, so attacking is preferred to blocking whenever death is
not immediate. Two untested value shapes and one structural map change, all default-off (binary
bit-identical on the three bests, leak test green with every new parameter on):

| name | track | change | prediction |
|---|---|---|---|
| `*_ldw35` | heart, a20, a0 | `loss_dmg_w=35`: damage credit on the win-bonus scale | more damage-greedy; probably neutral or negative |
| `*_ltw1` | heart, a20 | `loss_turn_w=1`: survival credit 1.0/turn (5×) | more defensive in losing rollouts; small positive at A20 |
| `*_brt8` / `a20_brt5` | heart, a20, a0 | `boss_rollout_turns=N`: rollouts in boss fights cut at N turns, valued by the cutoff heuristic (HP counts) | the direct test of "HP is invisible in boss-fight values"; earlier global cutoffs (3/6 turns, all fights) lost badly at A0 |
| `heart_pvh0` | heart | `potion_v_heart=0`: no credit for potions kept in the Heart fight (the last fight) | ≤ +1 pt (potions are already mostly spent) |
| `heart_a4pot` | heart | `act4_shop_potion=1`: spare gold buys potions in the act-4 shop | small positive (gold is worthless after act 4) |
| `*_esim` | heart, a20, a0 | `elite_sim=1`: an elite node's map value comes from 3 simulated fights (one per elite of the act, 100-sim search, current deck/HP, own seeds): `w_elite_hi` if ≥ 2/3 won with ≥ 35% HP left, else `w_elite_lo`; A20 also drops the blanket act-1 penalty (`w_elite_act1=0`) | the main bet: A20 decks that can take an elite get relics, decks that cannot stop dying to Slavers/Leader/Book; cost ≈ +0.6 fight-equivalents per map decision with an elite in view (logged via `secs`) |
| `a20_esim3` | a20 | the gate requiring 3/3 wins | stricter variant |

**Batch 48 results (pasted):**
- `heart_pvh0` val: win=0.093 floor=46.33 dscore=+0.0000 P=0.403; pooled val,dev2,dev3 [heart]: n=500
  heart_pvh0 win=0.080 floor=45.66 vs heart_bfbonus25_g_heart win=0.082 floor=45.66 dscore=-0.0020
  dwin=-0.002 P(improve)=0.251 → discard (32 of 300 val games differ, net zero; only 12 of 140 act-4
  games finish holding a potion).
- **`heart_a4pot` val: win=0.107 floor=46.32 dscore=+0.0133 P=0.935 → keep; pooled val,dev2,dev3
  [heart]: n=500 heart_a4pot win=0.098 floor=45.65 vs heart_bfbonus25_g_heart win=0.082 floor=45.66
  dscore=+0.0160 dwin=+0.016 P(improve)=0.996 → KEEP (pooled 500), new Heart best** (+4, +3, +1 Heart
  wins on val/dev2/dev3; floors unchanged — this is a pure Heart-conversion gain, the first one).
  Dev replication queued (`research/tools/rep_dev.sh`).

### Batch 49 (queued 01:05 UTC) — key acquisition on the Heart track

In-process replay of the Heart best's val games (same binary, bit-identical; prints the keys held),
all 300 games (replay matches the recorded run: 140 act-4 runs, 28 Heart wins; file
`research/diag/heart_bfbonus25_g_heart_keys_val.txt`): key-less act-3 clears by keys held —
`R-B` 20 (emerald missing), `-GB` 14 (ruby missing), `--B` 2, `-G-` 1 = 37; missing-key counts
emerald 22, ruby 17, sapphire 1. Every run that reached act 4 held all three. So the emerald key
(the burning elite, which the map policy only values at ≥ 60% HP and at +2) is missed in 59% of
the key-less clears and the ruby key (rest-site Recall, acts 1–2 only, ≥ 65% HP, never at the
pre-boss rest) in 46% (some lack both). Among all 84 runs that ended in act 3, 33 held all keys. Six default-off parameters, queued behind a guarded rebuild (repro_check on the three
bests + leak test, else the queue is stashed): `w_burning_act3=3`, `burn_hp=0.5`, `w_burning=4`
(emerald); `key_act3=1`, `key_rest_w=1 key_act3=1`, `key_rest_hp_preboss=0.8` (ruby). Prediction:
each converts some of the 14% key-less clears into act-4 runs; the cost is the HP/heal they trade
for the key, which the pooled rule measures as Heart wins (a run that reaches the Heart weaker
and dies there scores the same as a key-less clear: floor 55 vs 51 only). Combinations follow
the single-knob results.

- `heart_ltw1`: pooled val,dev2,dev3 [heart]: n=500  heart_ltw1 win=0.094 floor=45.70  heart_a4pot win=0.098 floor=45.65  dscore=-0.0026 dwin=-0.004 P(improve)=0.436 → discard (val −5 Heart wins, dev2 +3, dev3 0: noise around zero).
- `heart_ldw35`: pooled val,dev2,dev3 [heart]: n=500  heart_ldw35 win=0.086 floor=45.13  heart_a4pot win=0.098 floor=45.65  dscore=-0.0205 dwin=-0.012 P(improve)=0.111 → discard (as predicted: damage-greedy; floors drop too).

- `heart_brt8`: pooled val,dev2,dev3 [heart]: n=500  heart_brt8 win=0.086 floor=44.34  heart_a4pot win=0.098 floor=45.65  dscore=-0.0329 dwin=-0.012 P(improve)=0.011 → discard. **The hypothesis "HP is invisible in boss-fight values" is rejected
  at A0:** the cutoff heuristic (HP counted, damage on the win-bonus scale) plays bosses worse than full
  random rollouts valued by the small loss credit (val −5 Heart wins, P=0.021; dev2 −1; dev3 0). The
  A20 versions (`a20_ltw1/ldw35/brt8/brt5`) stay queued because A20 bosses are the ones rollouts
  never win; expectation now: negative there too.

- `heart_esim`: pooled val,dev2,dev3 [heart]: n=500  heart_esim win=0.088 floor=46.23  heart_a4pot win=0.098 floor=45.65  dscore=+0.0023 dwin=-0.010 P(improve)=0.573 → discard. Floors up (+0.6), Heart wins down (−5 of 500): on the keys track the gate
  interacts with the emerald key — a deck that fails the 2-of-3 test gets `w_elite_lo`, which outweighs the
  +2 burning-elite bonus, so key-less clears rise (val 44 vs 37). Cost: 4.60 s vs 3.35 s per game (+37%).
  The gate's real target is A20 (no keys), next in the queue.

- `a20_esim`: pooled val,dev2,dev3 [act3]: n=500  a20_esim win=0.026 floor=26.72  a20_restboss1 win=0.028 floor=27.41  dscore=-0.0154 dwin=-0.002 P(improve)=0.108 → discard. Val funnel (300): wins 9 vs 12, floor 27.5 vs 28.4; act-1 deaths 93 vs 81
  — with the blanket act-1 penalty off, act-1 elite deaths rose (Nob/Sentries/Lagavulin 23 vs 16) while
  act-2 elite deaths fell (Slavers/Leader/Book 31 vs 40) and boss deaths 144 vs 150. The 2-of-3 test at
  100 simulations is too lenient in act 1 (the simulated fight starts from the current HP, before the
  hallway fight that precedes the elite). Cost 2.07 s vs 1.39 s per game. The 3-of-3 variant is next.

- Queued from the funnel: `a20_esim_a23` — the gate with the blanket act-1 penalty kept (`w_elite_act1=-2`
  stays), so the simulated test only decides act-2/3 elites, where it reduced elite deaths. Prediction: small
  positive on floors (fewer Slavers/Leader/Book deaths), wins within noise.

- `a20_esim3` (3-of-3): **identical games to `a20_esim` (0 of 300 val games differ)** — a direct 8-seed test shows
  the threshold reaches the code (`elite_sim_pwin=0.0` changes a game, `1.0` does not), so the simulated
  outcome is practically never a 2-of-3 split with ≥ 35% HP left: the three 100-sim fights agree (all won
  or all lost). The gate is binary in practice; the threshold is not a lever. Same pooled decision as
  `a20_esim` (discard).

- `a20_esim_a23`: pooled val,dev2,dev3 [act3]: n=500  a20_esim_a23 win=0.030 floor=27.60  a20_restboss1 win=0.028 floor=27.41  dscore=+0.0057 dwin=+0.002 P(improve)=0.759 → near-miss (dev2 +0.035, dev3 +0.019, val −0.008); extended under the pooled
  near-miss rule above (dev slice, pooled 600). Val funnel: floor 28.43 vs 28.36, wins 9 vs 12, act-2 elite
  deaths 38 vs 40, boss deaths 143 vs 150, 2.14 s vs 1.39 s per game.

- `a20_ltw1`: pooled val,dev2,dev3 [act3]: n=500  a20_ltw1 win=0.026 floor=27.22  a20_restboss1 win=0.028 floor=27.41  dscore=-0.0057 dwin=-0.002 P(improve)=0.312 → discard (val exactly flat, both slices slightly negative).

- `a20_esim_a23` extension: dev +0.0078 (wins 0, P=0.697); pooled val,dev2,dev3,dev [act3]: n=600  a20_esim_a23 win=0.028 floor=27.46  a20_restboss1 win=0.027 floor=27.23  dscore=+0.0061 dwin=+0.002 P(improve)=0.807 → **no** (P < 0.9). The simulated elite
  gate is closed: at A20 its best variant is ≈ +0.006 score at P ≈ 0.8 for +54% game time, at A0 it costs
  Heart wins through the emerald key. What it did show: act-2 elite deaths are not mainly a gating problem
  (38 vs 40 with the gate), i.e. the decks that take act-2 elites would also fail the fights a stronger
  policy would take — the lever is deck strength by act 2, not elite selection.

- `a20_ldw35`: pooled val,dev2,dev3 [act3]: n=500  a20_ldw35 win=0.024 floor=26.39  a20_restboss1 win=0.028 floor=27.41  dscore=-0.0239 dwin=-0.004 P(improve)=0.024 → discard (negative on all three slices, as at A0).

- `a20_brt8` val: dscore=−0.0571, dwin=−0.020, P=0.000 (pasted from the runner) — the boss rollout cutoff loses
  at A20 as at A0. **Value-shape line closed at both ascensions**; the queued copies `a20_brt5`, A0 `ldw35`
  and A0 `brt8` were pulled from the queue (known answer, not a keep decision). A0 `esim` stays (no keys at A0,
  so the emerald-key interaction is absent; the Heart track showed floors +0.6).

- `a20_brt8` pooled: pooled val,dev2,dev3 [act3]: n=500  a20_brt8 win=0.022 floor=25.62  a20_restboss1 win=0.028 floor=27.41  dscore=-0.0409 dwin=-0.006 P(improve)=0.000 → discard.

- A0 `esim` (val-300 rule): win=0.707 floor=47.06 dscore=-0.0493 P=0.003 → discard (−13 wins vs `mapd085`):
  the gate also hurts the strong A0 agent — 100-simulation fights without potions underestimate what the
  500-simulation agent wins, so good elites are skipped. Elite gate closed on all three tracks.

**Batch 49 results (pasted):**
- `heart_wba3`: pooled val,dev2,dev3 [heart]: n=500  heart_wba3 win=0.102 floor=45.71  heart_a4pot win=0.098 floor=45.65  dscore=+0.0035 dwin=+0.004 P(improve)=0.893 → near-miss (P 0.893); val funnel: key-less clears 29 vs 37, act-4 runs 147 vs 140, Heart
  wins 33 vs 32; dev2 +1 win, dev3 flat. Extended under the pooled near-miss rule (dev slice, pooled 600).

- `heart_bhp05`: pooled val,dev2,dev3 [heart]: n=500  heart_bhp05 win=0.096 floor=45.61  heart_a4pot win=0.098 floor=45.65  dscore=-0.0027 dwin=-0.002 P(improve)=0.000 → discard (val key-less clears 37 vs 37, act-4 139 vs 140; dev2/dev3 identical games):
  HP is not what blocks the emerald key; the map value of the burning elite's path is.

- **`heart_wba3` extension: dev +0.0098 (+1 Heart win, P=0.629); pooled val,dev2,dev3,dev [heart]: n=600  heart_wba3 win=0.098 floor=45.65  heart_a4pot win=0.093 floor=45.59  dscore=+0.0045 dwin=+0.005 P(improve)=0.958 → KEEP (extension keep: pooled-600
  P ≥ 0.9 and dev-alone > 0), new Heart best.** Chain: heart_bfbonus25_g_heart 8.2% → heart_a4pot 9.3% →
  heart_wba3 9.8% Heart wins on the pooled 600. Labelled "extension keep" — dev is consumed, no further slice.

- `heart_wb4` (on top of wba3): pooled val,dev2,dev3 [heart]: n=500  heart_wb4 win=0.102 floor=45.82  heart_wba3 win=0.102 floor=45.71  dscore=-0.0004 dwin=+0.000 P(improve)=0.490; val act-4 161 vs 147, key-less clears 24 vs 29, Heart wins 35 vs 33;
  dev2 −4 wins, dev3 +2 → discard.

- `heart_ka3` (ruby key also at act-3 rest sites): pooled val,dev2,dev3 [heart]: n=500  heart_ka3 win=0.104 floor=45.87  heart_wba3 win=0.102 floor=45.71  dscore=+0.0020 dwin=+0.002 P(improve)=0.851; val key-less clears 18 vs 29, act-4 158 vs 147, Heart
  wins 34 vs 33; dev2 flat, dev3 identical → near-miss, extended.

- `heart_krw1` (rest sites +1 on the map while the ruby key is missing, through act 3): pooled val,dev2,dev3 [heart]: n=500  heart_krw1 win=0.110 floor=45.76  heart_wba3 win=0.102 floor=45.71  dscore=+0.0071 dwin=+0.008 P(improve)=0.701; val key-less
  clears 15 vs 29, act-4 156 vs 147, Heart wins 37 vs 33 (+0.019, P=0.83); dev2 −0.018, dev3 −0.004 (wins flat)
  → near-miss, extended.

- **`heart_ka3` extension: dev +0.010 (+1 Heart win, P=0.626); pooled val,dev2,dev3,dev [heart]: n=600  heart_ka3 win=0.102 floor=45.83  heart_wba3 win=0.098 floor=45.65  dscore=+0.0034 dwin=+0.003 P(improve)=0.949 → KEEP (extension keep), new Heart best.**
  Chain on the pooled 600: 8.2% → a4pot 9.3% → wba3 9.8% → ka3 10.2% Heart wins. Key-less clears on val: 37 → 29 → 18.
  Note: `heart_krw1` (= ka3 + rest-site seeking) was compared against wba3; its extension line runs as queued, but the
  decision that matters is krw1 vs ka3 (the superset step), computed from the existing runs below.

- `heart_krw1` vs `heart_ka3` (the superset step, pooled 500 from the existing runs, declared before the queued
  extension finished): pooled val,dev2,dev3 [heart]: n=500 heart_krw1 win=0.110 floor=45.76 heart_ka3 win=0.104
  floor=45.87 dscore=+0.0051 dwin=+0.006 P(improve)=0.649 → below the near-miss band: **rest-site seeking is not
  kept on top of ka3**. The queued `ext_pooled.sh heart_krw1 heart_wba3` line is informational (wrong base after
  the ka3 keep); if it promotes krw1, the best is reverted to ka3 and the results.tsv row annotated.

- `heart_krw1` stale-base extension (vs wba3): dev +0.036 (+3 Heart wins); pooled val,dev2,dev3,dev [heart]: n=600  heart_krw1 win=0.110 floor=45.80  heart_wba3 win=0.098 floor=45.65  dscore=+0.0119 dwin=+0.012 P(improve)=0.823 → no (P < 0.9), nothing promoted.
  Informational only (the superset step did not reach the near-miss band on 500, so no extension applies to it):
  pooled val,dev2,dev3,dev [heart]: n=600  heart_krw1 win=0.110 floor=45.80  heart_ka3 win=0.102 floor=45.83  dscore=+0.0086 dwin=+0.008 P(improve)=0.746. Rest-site seeking stays unkept; it is the obvious first candidate for the next Heart batch if a
  cleaner test is wanted (fresh val/dev2/dev3 on top of ka3).

- `heart_kpb08` (ruby key at the pre-boss rest when HP ≥ 80%): pooled val,dev2,dev3 [heart]: n=500  heart_kpb08 win=0.104 floor=45.92  heart_ka3 win=0.104 floor=45.87  dscore=-0.0004 dwin=+0.000 P(improve)=0.443 → discard (val key-less clears 16 vs 18,
  Heart wins equal; dev2/dev3 identical games). Batch 49 summary: of the 37 key-less clears, the act-3 burning
  bonus and the act-3 ruby key removed 19 (37 → 18) and added ≈ +4 Heart wins per 300; the remaining 18 key-less
  clears lack the emerald (path never reaches the burning elite at ≥ 60% HP) or the ruby (no eligible rest).

### Batch 50 (queued 02:42 UTC, overnight) — more Heart-labelled data for the deck-value model

Model 6 (the Heart track's bonus) was trained on `bg1` + `bg_heart`, where the Heart-labelled part is 500 games
of `heart_cur` (183 reached act 4). Expert iteration failed at A0 (models 7/8), so the prior is weak, but the
Heart case differs in that data volume, not label quality, is the limit. Queued: `bg_heart2` = 1,000 games of the
current best (`heart_ka3`, keys, `bf_heart=1`) from seed 302000 in resumable 50-game chunks (budget 5 h), then
model 12 on `bg1` + `bg_heart` + `bg_heart2` (per-file run ids), then `heart_bfb25_g12` under the pooled rule.
Prediction: LOW confidence of a keep; a replication of the model-6 result (≈ 0) is the likely outcome, and a
3× jump in any validation metric is a bug until proven otherwise.

- `heart_hm2` (sims 470, 2× at the Heart) val: dscore=+0.0524, dwin=+0.037 (+11 Heart wins: 45 vs 34), P=0.994 —
  **informational, not keepable: compute accounting shows +3.8% search actions per game (3,470,968 vs 3,342,662;
  seconds 3.37 vs 3.18)**, so the 470 did not pay for the doubled Heart fight. From those two numbers the Heart fight
  is ≈ 10% of a run's search actions, so neutrality needs ≈ 453 base simulations. Pre-registered (02:50 UTC, before
  its run): `heart_hm2b` = sims 450 + heart_mult 2, decided under the pooled rule vs `heart_ka3`; its per-game
  actions must be ≤ the base's for a keep to stand (checked and pasted with the result). If the runner promotes
  `heart_hm2` on its pooled slices meanwhile, the queue line resets the best to `heart_ka3` before `hm2b` runs and
  the hm2 keep row is annotated as budget-scaling. Why this might be real: the Heart is the one fight where
  rollout noise at 500 simulations plausibly costs wins (750 HP, Beat of Death, Invincible), and the budget doubling
  only applies there; why it might not: +11 wins on 300 is also what a lucky trajectory change looks like.

- `heart_hm2` pooled (informational, budget +3.8%): pooled val,dev2,dev3 [heart]: n=500  heart_hm2 win=0.136 floor=46.50  heart_ka3 win=0.104 floor=45.87  dscore=+0.0414 dwin=+0.032 P(improve)=0.995 (+11, +3, +2 Heart wins on val/dev2/dev3). The runner's
  automatic promotion was reset to `heart_ka3` by the guard on the next line (verified); results.tsv carries an
  annotation row. `heart_hm2b` (sims 450) is running.

- `heart_hm2b` (sims 450, 2× at the Heart) val: dscore=+0.0223, dwin=+0.007, P=0.831; **compute check: mean search
  actions 3,237,066 vs 3,342,662 for the base (−3.2%), seconds 3.15 vs 3.18 → compute-neutral (cheaper), a keep
  would be admissible.** Funnel: act-4 152 vs 158, Heart wins 36 vs 34, key-less clears 26 vs 18. Note on hm2's
  +11 wins: they came with 173 act-4 runs vs 158, which a Heart-only budget cannot cause — the sims-470 trajectory
  change was lucky on val; hm2b's +2 Heart wins at equal act-4 reach is the honest size of the Heart-fight effect.

- `heart_hm2b` pooled: pooled val,dev2,dev3 [heart]: n=500  heart_hm2b win=0.110 floor=46.26  heart_ka3 win=0.104 floor=45.87  dscore=+0.0140 dwin=+0.006 P(improve)=0.787 → near-miss (dev2 −1 win, dev3 +2 wins); extended under the pooled near-miss rule (dev
  slice, pooled 600; keep needs P ≥ 0.9 and dev-alone > 0; compute-neutral verified above).

- `a20_neow1p` (Neow ranking at A20, pooled re-test): pooled val,dev2,dev3 [act3]: n=500  a20_neow1p win=0.024 floor=26.51  a20_restboss1 win=0.028 floor=27.41  dscore=-0.0215 dwin=-0.004 P(improve)=0.086 → discard (negative on all three slices; the earlier
  val-only +0.010 was against an older base). Neow ranking is closed on all tracks (A0 ×2, A10, A20).

- `heart_hm2b` extension: dev +0.0137 (+1 Heart win, P=0.661); pooled val,dev2,dev3,dev [heart]: n=600  heart_hm2b win=0.108 floor=46.14  heart_ka3 win=0.102 floor=45.83  dscore=+0.0140 dwin=+0.007 P(improve)=0.817 → **no** (P < 0.9). The Heart search
  reallocation is positive but unproven: +0.014 score, +4 Heart wins on 600 seeds at compute parity. Closed for
  now; a cleaner version would need a per-fight budget accounting in the harness rather than a global `sims` cut.
- Batch 50 generation started 02:47 UTC: smoke cost 32.0 s/game → `bf_every=1`, 1,000 games in 50-game chunks
  from seed 302000 (≈ 2.5 h including one 2-hour-limit restart), then model 12 and `heart_bfb25_g12`.

- Batch 50: `bg_heart2` done (1,000 games of heart_ka3, 522 reached act 4, 100 Heart wins, 288 MB). Model 12 on
  bg1 + bg_heart + bg_heart2 (pasted from the trainer): `exported research/models/bf_g12.txt (epoch 24, boss_bce 0.4046
  vs base 0.6739, pair_corr 0.094)`; per-epoch val at epoch 24: `boss_bce=0.4046 (base 0.6739) hall_bce=0.1029 (base
  0.1500) pair_corr=0.155 pair_sign_acc=0.593 (n=5439) pair_corr_boss_dmg=0.160`. No 3× jump (the run-id collision
  bug showed paired corr 0.35–0.48), repro_check bit-identical, leak test PASS with the model on. `heart_bfb25_g12`
  (pooled rule vs heart_ka3) running.

### Batch 51 (queued 04:05 UTC) — the Heart track never received four A0 keeps

Diffing `best_heart_params.txt` against `best_params.txt`: the Heart track lacks `prior_mlp=1 prior_mlp_depth=0`
(root MLP move prior, +3.6 pts at A0), `potion_v=8 potion_v_boss=4` (potsave2, pooled keep at A0), `w_elite_act1=1`
(wea1p1) and `map_discount=0.85` (mapd085, unreplicated). It was seeded from the chest-era agent (`INFO_keys1`
→ `heart_cur`) before those landed, and every Heart experiment since has been tuned on top of that older base.
Queued in order of expected effect, each under the pooled rule on top of whatever is best when it starts:
`heart_pmlp`, `heart_potsave`, `heart_wea1`, `heart_mapd`. Prediction: the move prior transfers (HIGH; it is a
combat improvement independent of keys), the others are small. Lesson recorded below: when a track is seeded,
record the exact A0 commit/params it branched from and re-diff after every A0 keep.

**Batch 51 results (pasted):**
- `heart_pmlp` (root MLP move prior ported): pooled val,dev2,dev3 [heart]: n=500  heart_pmlp win=0.098 floor=45.89  heart_ka3 win=0.104 floor=45.87  dscore=-0.0059 dwin=-0.006 P(improve)=0.365 → discard. **My HIGH-confidence prediction was wrong.** Val: −6
  Heart wins (28 vs 34), act-4 reach 150 vs 158, key-less clears 22 vs 18; dev2 +2, dev3 +1. Wall-clock 4.21 s vs
  3.18 s per game (the root MLP costs real time; search actions −2%). The prior was trained on the A0 agent's
  chest-era self-play (no act 4) and tuned at the A0 ladder where it was +3.6 pts; on the keys track the extra
  Heart deaths say it steers the Heart fight worse than uniform priors. Post-hoc explanation, not a prediction.

- `heart_potsave` (potion values ported): pooled val,dev2,dev3 [heart]: n=500  heart_potsave win=0.092 floor=45.76  heart_ka3 win=0.104 floor=45.87  dscore=-0.0145 dwin=-0.012 P(improve)=0.143 → discard (val −2 Heart wins, dev2 −6, dev3 +2). The A0 potion
  saving does not transfer either: with `hall_potions=0` already on, raising `potion_v` to 8 mostly stops potions
  being used against elites, and the Heart track pays for that with elite deaths.

- `heart_wea1` (act-1 elites +1 ported): pooled val,dev2,dev3 [heart]: n=500  heart_wea1 win=0.090 floor=45.60  heart_ka3 win=0.104 floor=45.87  dscore=-0.0189 dwin=-0.014 P(improve)=0.064 → discard (negative on all three slices). Three of the four A0
  keeps fail to transfer; the Heart track's chest-era base is apparently not the reason it is behind — the keys
  agent's games are different enough (elite choices, potions, the act-4 fights) that A0 tuning does not carry.
  Lesson kept: diff tracks, but test ports; never assume transfer.

- `heart_mapd` (map discount 0.85 ported): pooled val,dev2,dev3 [heart]: n=500  heart_mapd win=0.102 floor=45.73  heart_ka3 win=0.104 floor=45.87  dscore=-0.0038 dwin=-0.002 P(improve)=0.272 → discard. **Batch 51 verdict: none of the four A0 keeps transfers
  to the Heart track** (pmlp −0.006, potsave −0.015, wea1 −0.019, mapd −0.007 on the pooled 500). The track's
  parameters stay as they are; `track_diff.sh` remains a wake-up check but ports are hypotheses, not debts.

**Batch 52 results (pasted):**
- `heart_kud`: val dscore=+0.0184 dwin=+0.013 P=0.942 (key-less clears 11 vs 18, act-4 172 vs 158, Heart wins 38 vs 34);
  dev2 −3 wins (key-less 2 vs 7, act-4 52 vs 50); dev3 +1 win; pooled val,dev2,dev3 [heart]: n=500  heart_kud win=0.108 floor=45.84  heart_ka3 win=0.104 floor=45.87  dscore=+0.0009 dwin=+0.004 P(improve)=0.539 → discard. **The mechanism works exactly as
  predicted on every slice (keys are now ≈ 96% complete on val) and the score does not move**: under the Heart win
  definition a key-less act-3 clear and a death at the Heart score the same (floor 51 vs 55, both capped at 51/51),
  so extra act-4 runs only count if they beat the Heart, and at the 20% conversion seven extra act-4 runs are ≈ 1.4
  wins per 300 — below what 500 seeds can detect. Key acquisition is closed as a lever; what remains is conversion.

- `heart_kud_krw` (undiscounted channel + rest-site seeking): val +0.029, +6 Heart wins (40 vs 34), key-less clears
  10 vs 18, act-4 169 vs 158, P=0.935; dev2 −1 win (−0.041), dev3 +1 win; pooled val,dev2,dev3 [heart]: n=500  heart_kud_krw win=0.116 floor=45.81  heart_ka3 win=0.104 floor=45.87  dscore=+0.0096 dwin=+0.012 P(improve)=0.759 → near-miss, extended (dev slice, pooled 600).

- `heart_kud_krw` extension: dev +0.025 (+1 Heart win, P=0.722); pooled val,dev2,dev3,dev [heart]: n=600  heart_kud_krw win=0.113 floor=45.95  heart_ka3 win=0.102 floor=45.83  dscore=+0.0123 dwin=+0.012 P(improve)=0.806 → no (P < 0.9).

- Three Heart mechanisms now sit at "positive, P ≈ 0.8": `heart_hm2b` (+0.014 on 600, P=0.817), `heart_kud_krw`
  (+0.012 on 600, P=0.806), and `heart_krw1` earlier (+0.009 on 600, P=0.746). Per the loop rule (combine near-misses)
  `heart_combo` = sims 450 + 2× at the Heart + undiscounted key channel + rest-site seeking is queued after the relic
  batch, under the pooled rule vs `heart_ka3`; compute-neutral by construction (hm2b's accounting). Prediction (MED):
  if the three effects are real and additive, ≈ +0.03 → a keep; if they were noise, ≈ 0.

**Batch 53 results (pasted):**
- `bfrelic10` (A0, val rule): win=0.750 floor=47.47 dscore=+0.0020 P=0.559 → discard. The relic choice changed in 77
  of 300 games and the outcome did not move; cost 5.04 s vs 3.94 s per game (+28%, more than estimated — the two
  boss fights per option are long). At A0 the tier list is already adequate, or the simulation's blindness to
  out-of-combat drawbacks cancels what it sees in combat.

- `heart_bfrelic10`: val +0.025, +6 Heart wins (40 vs 34), act-4 162 vs 158, P=0.988, 4.08 s vs 3.18 s per game; dev2 +2
  wins (P=0.844); dev3 0 wins (−0.007); pooled val,dev2,dev3 [heart]: n=500  heart_bfrelic10 win=0.120 floor=46.05  heart_ka3 win=0.104 floor=45.87  dscore=+0.0181 dwin=+0.016 P(improve)=0.986 → **KEEP (pooled 500), new Heart best**. The act-3 relic is simulated against Shield & Spear and
  the Heart, so this is a conversion mechanism — the first one that moves Heart wins with act-4 reach unchanged.

- `heart_bfrelic10` dev replication: win=0.090 floor=45.82 dscore=+0.0051 P=0.603 (Heart wins unchanged on
  dev, floors up) — a weak replication; the keep stands under the pooled rule, reported as "pooled keep, dev flat".
- `a20_bfrelic10` extension: dev −0.010; pooled val,dev2,dev3,dev [act3]: n=600  a20_bfrelic10 win=0.030 floor=27.22  a20_restboss1 win=0.027 floor=27.23  dscore=+0.0030 dwin=+0.003 P(improve)=0.743 → no. Relic choice by simulation: Heart keep, A0 flat, A20 near-miss
  unconfirmed; A10 and the 25× weight still queued.

- `a10_bfrelic10` (A10, val rule): win=0.293 floor=37.35 dscore=-0.0005 P=0.498 → discard (flat, like A0).
  Verdict so far for relic choice by simulation: it helps only where the next fights are the Heart and Shield & Spear
  (a known, fixed pair the simulation can target); against sampled next-act fights it is no better than the tier list.

- `bfrelic25` (A0): win=0.743 floor=47.35 dscore=-0.0070 P=0.359 → discard. Batch 53 closed: relic choice by
  simulation is kept on the Heart track only.

- `heart_combo` (sims 450 + 2× at the Heart + undiscounted key channel + rest-site seeking, on top of the relic keep):
  val −4 Heart wins (36 vs 40; key-less clears 11 vs 18), dev2 −2, dev3 +1; pooled val,dev2,dev3 [heart]: n=500  heart_combo win=0.110 floor=46.43  heart_bfrelic10 win=0.120 floor=46.05  dscore=-0.0038 dwin=-0.010 P(improve)=0.425 → discard. The three "positive at
  P ≈ 0.8" mechanisms do not add up on top of `heart_bfrelic10`; recorded as noise until a larger evaluation says
  otherwise. Night's Heart work is closed at `heart_bfrelic10`.

### Batch 53 (queued 04:52 UTC) — boss relics chosen by simulated fights

Boss relics are the largest single decisions in a run and are currently picked by a tier list (`boss_pol=1`) that
cannot see the deck. `bf_relic_w`: for each offered relic, a hypothetical state with the relic added plays K=2
simulated fights against the next act's boss and a sampled next-act hallway (100-simulation search, common seeds
per option; for the act-3 relic with all keys: Shield & Spear + the Heart); score = KB tier value + w × (simulated
value with the relic − without). Additive on the KB value (the shape that worked for cards), because the KB tier
carries the out-of-combat drawbacks the simulation cannot see (gold, healing, potions, card choices, pickup
transforms). Cost ≈ 16 fights × 100 simulations per relic screen, 2–3 per run. Default off; guarded rebuild; then
`bfrelic10` (A0, val rule), `heart_bfrelic10`, `a20_bfrelic10` (pooled), `a10_bfrelic10`, `bfrelic25`. Prediction
(MED): +1–3 pts at A0 and A10 where energy relics are the swing; the hypothesis fails if the simulation's blindness
to drawbacks makes it pick Ectoplasm/Sozu/Coffee Dripper-type relics.

### Batch 52 (queued 04:24 UTC) — the key bonuses were being discounted away

Replay of `heart_ka3`'s val games (bit-identical: 158 act-4 runs, 34 Heart wins; file
`research/diag/heart_ka3_keys_val.txt`): the 18 remaining key-less act-3 clears lack the emerald in 13, the ruby in 5,
the sapphire in 1. The burning-elite HP threshold was shown not to matter (`heart_bhp05`: identical games), and a
bigger bonus (`heart_wb4`) bought keys but not wins. The mechanism left: the path DP discounts every node by
`map_discount^rows`, so a burning elite ten rows away is worth 0.9^10 ≈ 0.35 of its bonus exactly when the branch
that can still reach it is chosen; by the time it is close, the path no longer connects. `key_undiscounted=1`
carries the key bonuses (burning elite, ruby-key rest sites) in a second, undiscounted DP channel and picks the
next node by the sum. Default off (identical DP), compile-checked in the profiling build; the production rebuild is a
guarded queue line (repro_check ×3 + leak test with the new parameters on, else the queue is stashed). Queued:
`heart_kud`, then `heart_kud_krw` (with rest-site seeking, which was P=0.65 discounted). Prediction (MED): emerald
misses fall by half; Heart wins +1–3 of 300 if the extra act-4 runs convert at the usual 20%.

Compute note: `elite_sim` adds strategic-layer simulations (like the `bf_*` fights, outside the
per-combat-decision budget); its per-game `secs` is reported with the result.

## Speed at equal wall-clock (started 2026-11-01 23:40 UTC)
Why: every cheap knob and the learned strategic layer are at their ceilings; the 4×-compute probe (+4.5
pts) says simulations per decision still buy wins, and the fixed-compute rule is wall-clock, so a faster
search is a legitimate strength gain. Method: (1) gprof-instrumented build in `sts_lightspeed/build_prof`
(`-O3 -pg -g`), profile `fair_eval --inproc` on three val seeds with the best params; (2) optimise the top
hotspots with semantics-preserving changes only — the optimised binary must reproduce every recorded run
bit-identically (`repro_check`), pass the leak test and a ≥300-game crash sweep (the earlier fixed-size
pile attempt was reverted for stack corruption); (3) `calibrate.sh` the speedup on the idle machine and
convert it into simulations at equal wall-clock for the val experiment.

**First profile (gprof, 3 val games; callgrind, 1 short game):** the cost is spread thin — the top entries
are the simulator's action system (`executeActions` 8.8%, `addToBot` 5.9%, `std::function` copy/destroy
managers ~10% in aggregate), the action enumerator and card usability checks (`enumerateActionsInto`
4.6%, `canUse` 3.9%, `requiresTarget` 2.9% — 111 M calls per 3 games), tree bookkeeping (`newNode` 3.8%),
and, in the forked harness, re-parsing the deck-value model file per game (`vfscanf`+`strtof` 6% of a
short game). Nothing above 9%; a 2× speedup would need a rewrite of the simulator's action queue (a
multi-day job with real risk), not micro-optimisation.
**Done (behaviour-preserving, verified):** move overloads for `BattleContext::addToBot/addToTop` (no
`std::function` copy per queued action; sim patch regenerated) and the model file preloaded in the harness
parent before forking. Bit-identical (repro_check on `mapd085` and the Heart best), leak test green,
measured speedup 1.029× on 8 inproc games (`research/tools/speedcheck.sh`). Too small to convert into
simulations (+3% sims ≈ +0.1 pt); kept because it is free. The speed line is parked: the remaining cost
is architectural.

## Replication slices (added 2026-11-01 18:35 UTC)
`dev2` (seeds 600000+, 100) and `dev3` (700000+, 100) join `dev` as held-out replication slices, never
tuned on. Use: a track best whose first dev check disagrees with val (direction) is re-checked on dev2
before anything is claimed; the Heart track, where a 2–3-point difference is one or two runs, reports
only effects that replicate on two independent slices. These slices do not touch `test`.

## Operational lessons (self-inflicted, recorded so the loop stops repeating them)

- **Tracks drift apart silently.** The Heart track was seeded from the A0 agent of its day and never re-synced; four
  later A0 keeps (move prior, potion values, act-1 elite weight, map discount) were missing for a whole day of Heart
  experiments. Rule: `research/tools/track_diff.sh` (params diff of every track against A0) runs at each hourly
  wake-up, and any A0 keep is queued as a port experiment on every other track within the next batch.

- **A tool that loads run files must take the win definition explicitly.** `pooled.py` loaded with
  the default act-3 win mode, so the first pooled Heart decision (`heart_pvh0`) was printed with
  21.6% "wins" (act-3 clears). The dscore/P were unaffected (floors and the win difference are the
  same under both definitions, as all differing games had keys), the decision (discard) stands, and
  the tool now infers `heart` from `keys=1` in the candidate's params or takes it as a 5th argument.
  Same class of error as the "49 Heart wins" correction: whenever a Heart-track number is produced
  by a new code path, check it against the harness's heart-mode summary before using it.
- **Third self-match (2026-11-01 21:30 UTC):** a cleanup command grepped the process list for
  `bf_pipeline` while its own command line contained `research/tools/bf_pipeline.sh` (in a `printf` for
  the resume file) and killed its own shell. The bracket trick protects the *pattern*, not other
  occurrences of the text in the same command. Rule: never combine a process-list grep with any other
  text in one command; list orphans by parent pid (`awk '$2==1'`) and kill by numeric pid in a separate
  command.
- **Run ids must be unique per dataset, not per seed.** `bg1` and `bg_heart` were generated from the
  same train seeds, so the seed-derived run ids collided when the two sets were pooled; base rows of one
  game were paired with option rows of the other, and model 6's first training showed paired
  correlations of 0.35–0.48 (vs 0.10–0.15 for every honest model) — a tell-tale, not a breakthrough.
  Caught before any experiment result was logged (the runner was stopped, the orphaned pipeline killed,
  the half-written run deleted). Fix: the loader assigns run ids by file (one file = one game) and asserts
  base rows == decisions. Rule: a validation metric that jumps 3× on a routine change is a bug until proven
  otherwise.
- **Never edit a script that is running.** bash reads scripts incrementally; editing
  `bf_pipeline.sh` while the queue was executing it produced a syntax error at the end of the run (after
  the last experiment, so nothing was lost). Edit tools only between queue lines that use them, or
  write the new version under a new name and point new queue lines at it.
- **Background commands die at the 2-hour limit** (the queue runner was killed 45 s into the first
  fight-data generation). The runner now records the line in flight (`research/queue.current`) and
  re-runs it on restart; long generations run in resumable 50-game chunks (`bf_gen.sh`, per-chunk log
  directories with `.done` markers); the loader truncates each file to whole rows. Restart the runner
  (`research/tools/runq.sh`, background, max timeout) whenever it is reported killed.
- **Numbers in notes are pasted, never typed.** Twice today a floor was written from memory and was
  wrong by 0.2–0.3. `research/tools/note.py NAME...` prints the exact fragment from results.tsv;
  program.md/REPORT.md lines quoting results are built from its output (or computed in a script).
- **Chains of background waiters are banned.** Batch 27–29 were queued as three waiters, each
  polling the previous one's output file; the first used `pgrep -f "new_track.sh"`, which matched
  its own command line, so all three slept forever (second occurrence of the `-f` self-match bug).
  Replacement: `research/tools/runq.sh` runs the lines of `research/queue.txt` sequentially (builds
  included, so a rebuild never races an evaluation); append lines any time, one runner (flock).
  `research/tools/repro_check.sh NAME [n]` re-runs n recorded games with the current binary and
  fails unless every column but timing is identical — run it after every rebuild, before any
  paired comparison (weights-consistency rule, now mechanised).
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

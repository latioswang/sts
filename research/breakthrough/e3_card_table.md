# E3 — per-card value table from the fork data on disk (stage 0)

Date 2026-11-16. Script: `research/analysis/s0_e3_card_table.py` (run from `/home/user/sts`:
`nice -n 10 python3 research/analysis/s0_e3_card_table.py`, about 15 s, one process). Read-only on all data.
Reading fixed in advance (PLAN.md §3, E3): if the table does not beat the agent's own picks (≥ 2 SE on either track),
the measured-tables idea needs a new argument before any compute.

## Verdict

**The table does not beat the agent's own picks on either track.** No variant gets within 2 SE of beating it.
Primary variant (card id, empirical-Bayes shrinkage), paired against the agent's own pick:

| Track | Test set | n | table − own | SE | z |
|---|---|---|---|---|---|
| A20 | branch held-out (crc32(seed)%5==0), M=8 mean | 3,974 | +0.0008 | 0.0012 | +0.63 |
| A20 | F33 split-half set, (A+B)/2 | 696 | −0.0041 | 0.0026 | −1.55 |
| A10 | branch held-out, M=8 mean | 3,965 | −0.0033 | 0.0025 | −1.32 |
| A10 | F33 split-half set, (A+B)/2 | 757 | +0.0006 | 0.0048 | +0.12 |

The table also fails **in-sample**, on the training decisions it was fitted on (a biased-up check): A20 −0.0016 ± 0.0006,
A10 −0.0006 ± 0.0013. A table that keys only on the card id cannot reproduce the agent's picks, even on its own training
data. It differs from the agent in 28% (A20) and 37% (A10) of held-out decisions and gains nothing.

## 1. Data decoded

- Format (from `FairAgent::branchEval`, `research/cpp/FairAgent.cpp:1396`). Each `branch_<seed>.bin` holds one
  card-reward decision. For each option there is one row: 666 run features of the state after the option, then
  `[o, n, mean, ok, floor, own]`. Options 0..n−1 are the offered cards and option n is skip. `mean` is the mean run
  score, min(floor,51)/51 + win, over M = 8 playouts on shared seeds. `own` flags the agent's choice.
- `split_<seed>.bin` (`branchEval2` kind 1, F33) has the same layout plus `[meanA, meanB]` (M = 16; halves m<8 and m≥8).
- Option identity: the card-count block of the features (offset 282, 384 CardIds, +0.34 per copy, +0.5 per upgraded
  copy) of option o minus that of the skip row. In every kept decision this gives exactly one card and its upgraded flag
  (0 anomalies). CardId names come from `sts_lightspeed_fid/include/constants/Cards.h`.
- Files: 44,800 read; 4,149 empty (no single-card reward screen); 3 bad size (crashed seeds); 0 invalid; 40,648 kept.

| Track | Dirs | Decisions | Train / held-out | Options per decision | By act (1/2/3) | Own = skip | Upgraded offers |
|---|---|---|---|---|---|---|---|
| A20 (asc 20) | branch_f28_w0–w6 | 19,531 | 15,557 / 3,974 | 3.986 (4: 18,872; 5: 341; 2: 304; 3: 14) | 15,797 / 3,713 / 21 | 0.100 | 0.041 |
| A10 (asc 10) | branch_f31_w1–w8, w7b, hold | 19,664 | 15,699 / 3,965 | 3.988 (4: 18,931; 5: 407; 2: 314; 3: 12) | 15,447 / 4,185 / 32 | 0.176 | 0.074 |

A20 plus A10 without `f31_hold` comes to 37,685 decisions, the F32 count and the "37,700" in PLAN.md. `f31_hold`
adds 1,510. The median decision floor is 10 (A20) and 11 (A10); decisions sit almost entirely in acts 1–2.
**Win rate is not available.** The files store only the per-option mean score, so wins cannot be separated from floors.

## 2. Tables (train seeds only)

Advantage of an option = its playout mean minus the mean over all options of that decision (skip counts as a key).
Shrinkage is normal-normal empirical Bayes by moments: s² is the pooled within-key variance, and τ² is the mean of
(m_k − prior)² − s²/n_k over keys with n ≥ 5. Variants:
`card` (prior 0, primary); `card_act` (card × act, shrunk toward the card value); `card_upg` (card × upgraded, shrunk
toward the card value); `card_vs_skip` (option minus skip in the same decision, prior 0); `card_k20` (fixed prior count 20).
Table pick = argmax over options; ties go to the agent's option (rare: duplicates or unseen cards).

| | A20 | A10 |
|---|---|---|
| keys | 172 | 178 |
| τ (between-card SD of advantage) | 0.0318 | 0.0704 |
| s (per-option noise SD) | 0.0869 | 0.1663 |
| τ act / τ upgraded / τ vs-skip | 0.0066 / 0.0069 / 0.0399 | 0.0000 / 0.0119 / 0.0954 |
| SKIP value | −0.0078 | −0.0091 |

Act adds nothing beyond the card (τ ≈ 0), and neither does the upgraded flag (τ about 0.01).
Top of the table, the same on both tracks: Reaper, Shockwave, Spot Weakness, Inflame (A20: +0.067, +0.060, +0.054,
+0.047; A10: +0.156, +0.137, +0.113, +0.081), Demon Form / Immolate (A20) and Metallicize / Impervious (A10), plus rare
colorless cards with small n. Bottom: Dual Wield, Berserk, Sentinel, Entrench, Brutality (A20 −0.03); Dual Wield, Berserk
(A10 −0.09) and small-n colorless cards. The full lists are in the script output.

## 3. Held-out scoring

**(a) Branch held-out (crc32(seed)%5==0).** Each picker is scored on the M = 8 mean of its pick. Neither the table nor
the agent sees these playouts, so this is honest without halves. These files store no half means, so an honest
oracle cannot be built here; only the biased in-sample one is shown.

| vs own (paired SE) | A20 (n=3,974, own 0.6724) | A10 (n=3,965, own 1.3179) |
|---|---|---|
| table card | +0.0008 ± 0.0012 (agree 0.716) | −0.0033 ± 0.0025 (agree 0.629) |
| table card_act | +0.0004 ± 0.0012 | −0.0033 ± 0.0025 |
| table card_upg | +0.0006 ± 0.0012 | −0.0029 ± 0.0025 |
| table card_vs_skip | −0.0004 ± 0.0012 | −0.0031 ± 0.0025 |
| table card_k20 | +0.0008 ± 0.0012 | −0.0031 ± 0.0025 |
| random | −0.0301 ± 0.0015 | −0.0574 ± 0.0028 |
| oracle in-sample (biased) | +0.0668 ± 0.0015 | +0.1368 ± 0.0028 |

**(b) F33 split-half card sets** (A20: `f33_a20card_local`; A10: `f33_a10card_w1`, `w2`). Their seeds are disjoint
from every branch seed, so the whole set is held out. The honest oracle picks on one half and scores on the other
(F33 method). The table and own are scored on (A+B)/2, and also on B alone to match the oracle's A→B score.

| vs own | A20 (n=696, own 0.6752) | A10 (n=757, own 1.3103) |
|---|---|---|
| table card | −0.0041 ± 0.0026; @B −0.0015 ± 0.0034 | +0.0006 ± 0.0048; @B +0.0012 ± 0.0060 |
| table card_act | −0.0069 ± 0.0026; @B −0.0050 ± 0.0031 | +0.0006 ± 0.0048; @B +0.0012 ± 0.0060 |
| table card_upg | −0.0029 ± 0.0025; @B −0.0011 ± 0.0033 | +0.0014 ± 0.0047; @B +0.0012 ± 0.0061 |
| table card_vs_skip | −0.0037 ± 0.0026; @B −0.0020 ± 0.0033 | −0.0019 ± 0.0048; @B −0.0006 ± 0.0062 |
| table card_k20 | −0.0038 ± 0.0026; @B −0.0011 ± 0.0034 | +0.0014 ± 0.0048; @B +0.0033 ± 0.0061 |
| random | −0.0294 ± 0.0028 | −0.0564 ± 0.0048 |
| oracle honest | −0.0097 ± 0.0032; @B −0.0093 ± 0.0041 | −0.0153 ± 0.0061; @B −0.0201 ± 0.0076 |
| oracle in-sample A→B (biased) | +0.0604 ± 0.0066 | +0.1225 ± 0.0105 |

The own, random and oracle rows reproduce the F33 log entries exactly (program.md, 2026-11-08 16:57 and 17:40 UTC).
This cross-checks the decoder.

## 4. Reading

- Pre-registered reading applies: **the table does not beat the agent's own picks** (best z = +0.65, A20 branch
  held-out, `card_k20`). The measured-tables idea needs a new argument before any compute.
- [inferred] There is between-card signal: τ is 0.03 (A20) and 0.07 (A10), about 1/3 and 2/5 of the per-option noise
  SD. The agent's picks already capture it. The table departs from the agent in 28–37% of decisions at zero net value,
  and loses to the agent even on the training decisions. The agent's choice uses deck context (its picker sees the deck)
  that a marginal per-card value cannot. Adding act or upgrade does not help, since both τ are about 0.
- [inferred] This agrees with F31–F33: honest card-reward headroom at M = 8 playouts per half is negative on both
  tracks, and decision-trained V models gained ≤ +0.003 per decision. Card-reward choice, measured this way, is not
  where the agent loses.
- Limits: score units only (floor/51 + win); win rate is not recoverable from these files. Decisions are almost all
  acts 1–2. Tables are per track (no pooling). The branch held-out set has no honest oracle because the files lack halves.

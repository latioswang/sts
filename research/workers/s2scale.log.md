# s2scale worker log

## Setup + bit-identity gate (2026-11-02)
setup.sh rc=0. Gate output (pasted):
```
repro_check research/runs/mapd085.val.a0.tsv: 16 games compared, 0 differ -> OK bit-identical
repro_check research/runs/heart_bfrelic10.val.a0.tsv: 16 games compared, 0 differ -> OK bit-identical
repro_check research/runs/a20_restboss1.val.a20.tsv: 16 games compared, 0 differ -> OK bit-identical
```
Queue: the 8 lines of research/workers/s2scale.md, run via research/tools/runq.sh with RESULTS_TSV=research/results.d/s2scale.tsv.

## Results
### Line 1 (leafdata 2500) sanity check
```
SV GEN leafdata DONE: 2500 files
files 2500 valid rows 815531 minus holdout 50000 = 765531
```
sv_11.meta: `12.060000 34.865000 765531 42 2 10459 1` -> training-row count reproduces exactly; no sv_11 retrain needed.
### Lines 2-3 (leafdata2 7500, merge)
```
SV GEN leafdata2 DONE: 7500 files
10000
```
### Incident: torch missing
Lines 4-8 ran at 16:39:38 and all failed instantly (`ModuleNotFoundError: No module named 'torch'`; the evals then exited `no model`). Nothing was trained or logged. Fix: `pip install torch --index-url https://download.pytorch.org/whl/cpu` (program.md resume step 0) -> `2.15.0+cpu`.
Trainer check after install (id 11 retrained to a scratch file, not committed):
```
SV id=11 rows=765531 hidden=42 layers=2 params=10459 holdout_rmse=12.060 predict_mean_rmse=34.865
```
matches sv_11.meta `12.060000 34.865000 765531 42 2 10459 1`. Lines 4-8 re-queued.
### Line 4 (train sv_16)
```
SV id=16 rows=3225306 hidden=42 layers=2 params=10459 holdout_rmse=11.736 predict_mean_rmse=34.737 exported /home/user/sts/research/models/sv_16.txt
```
Caveat: the holdout block is the last 50,000 of a fixed shuffle of *this* dataset, so it is a different holdout set from sv_11's; the RMSEs are not a strictly paired comparison.
### Line 5 (train sv_17)
```
SV id=17 rows=3225306 hidden=96 layers=2 params=29089 holdout_rmse=11.216 predict_mean_rmse=34.737 exported /home/user/sts/research/models/sv_17.txt
```
### Line 6 (leak test, leaf_sv=16)
```
LEAKTEST checked=123 mismatches=0 PASS
```
### Line 7 (sv_eval 16)
```
best: 20.45s  candidate: 11.76s  candidate/best = 0.575  (scale candidate sims by 1.739 for equal wall-clock)
vs mapd085: n=300	dscore=-0.3479	dwin=-0.263	P(improve)=0.000
```
note.py: `sv16` 48.7% win, floor 43.0, -0.348 vs base, P=0.000 → scaling
(context, note.py: `sv11` 42.0% win, floor 41.3, -0.449 vs base, P=0.000 → scaling; `mapd085` 75.0% win, floor 47.4, +0.018 vs base, P=0.913 → keep)
### Line 8 (sv_eval 17)
```
best: 22.04s  candidate: 31.18s  candidate/best = 1.415  (scale candidate sims by 0.707 for equal wall-clock)
vs mapd085: n=300	dscore=-0.3843	dwin=-0.297	P(improve)=0.000
```
note.py: `sv17` 45.3% win, floor 42.9, -0.384 vs base, P=0.000 → scaling

## Summary (QUEUE EMPTY 17:03:41)
note.py:
`sv16` 48.7% win, floor 43.0, -0.348 vs base, P=0.000 → scaling
`sv17` 45.3% win, floor 42.9, -0.384 vs base, P=0.000 → scaling
Both leaf-value-only configs (no rollouts) lose clearly to mapd085 at equal wall-clock. sv16 is ahead of sv11 (same size,
1/4 data) on this val split; the larger sv17 has the lower holdout RMSE but pays a 1.415 cost ratio (fewer sims) and plays worse.
Incident: torch was not installed on this VM; the first pass of lines 4-8 failed instantly with no rows written, and was
re-run after installing the CPU wheel (see above). The worker README setup step does not install torch.

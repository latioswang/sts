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

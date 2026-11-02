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

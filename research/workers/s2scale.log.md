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

# Worker task: Phase F4 fork run data for V, iteration 2 (NAME = your worker name; TRACK, W and SEED below)

Pre-registered in research/program.md ("Phase F4", 2026-11-06 16:4x UTC). The loop now runs on the **fidelity fork at 1b05bdc** (`setup.sh`
defaults to it: `latioswang/sts_lightspeed` at `research/patches/sts_lightspeed_fidelity.commit`; GCC with LTO builds fine; if LTO fails, add `-DSTS_LTO=OFF`). Generate 30,000 games of
run states on the fork with the current best of your TRACK plus exploration (`explore_eps=0.2`: each card reward and boss
relic is, with probability 0.2, picked uniformly at random), keep 32 random states per run, pack both sets into compressed
shards (`research/vf/runstore.py`) and push only the shard sets to the Hugging Face store (`research/data_store.json`; the
token is a network secret, nothing to configure).

Assignments (ASC / PARF / SEED):
- f4w27 -> W=27, TRACK=a0,  ASC=0,  PARF=research/best_params.txt,     SEED=5120000 (also builds the A0 holdout: HOLD=a0)
- f4w28 -> W=28, TRACK=a0,  ASC=0,  PARF=research/best_params.txt,     SEED=5150000
- f4w29 -> W=29, TRACK=a10, ASC=10, PARF=research/best_a10_params.txt, SEED=5180000 (also builds the A10 holdout: HOLD=a10)
- f4w30 -> W=30, TRACK=a20, ASC=20, PARF=research/best_a20_params.txt, SEED=5210000 (also builds the A20 holdout: HOLD=a20)

Before the queue: README steps 1–3 (branch, setup — it builds the fork — and the three **fork** repro_check gates; stop and
report if any is not bit-identical). `python3 research/tools/data_sync.py ls` must list `rundata_w1_k32_z`.

Queue lines for `research/queue.txt` (replace W, ASC, PARF, SEED; the two holdout lines only for f4w27 / f4w29 / f4w30, with
HOLD = a0 / a10 / a20), then start `research/tools/runq.sh` **detached**, so the 2-hour background limit never stops it: `setsid nohup research/tools/runq.sh > research/workers/.<name>_runq.out 2>&1 < /dev/null & disown` (watch that file; after a container restart, start it the same way again — it resumes from research/queue.current):
```
ASC=ASCV PARF=PARFV LOGP="--param rv_log=1 --param explore_eps=0.2" research/tools/sv_gen.sh rundata_f4_wW 30000 100 SEED
python3 research/vf/subsample_runs.py research/vf/data/rundata_f4_wW/all research/vf/data/rundata_f4_wW_k32/all --k 32
mkdir -p research/vf/data/rundata_f4_HOLD_hold/all && for k in 0 1 2 3 4 5 6 7 8 9; do ln -sf $(pwd)/research/vf/data/rundata_f4_wW/chunk_$k/run_*.bin research/vf/data/rundata_f4_HOLD_hold/all/; done && ls research/vf/data/rundata_f4_HOLD_hold/all | wc -l
python3 research/vf/runstore.py pack research/vf/data/rundata_f4_HOLD_hold/all research/vf/data/rundata_f4_HOLD_hold_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_f4_HOLD_hold_z
python3 research/vf/runstore.py pack research/vf/data/rundata_f4_wW_k32/all research/vf/data/rundata_f4_wW_k32_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_f4_wW_k32_z
python3 research/vf/runstore.py pack research/vf/data/rundata_f4_wW/all research/vf/data/rundata_f4_wW_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_f4_wW_z
```
`runstore.py pack` byte-verifies before it returns. If a push fails on HTTP 429, wait 15 minutes and re-run that line.
Hand back: the SV GEN / SUBSAMPLE / PACK / PUSH ... DONE lines (and the holdout file count, if you build one) in
research/workers/<name>.log.md, committed and pushed to worker/<name>. Do not delete anything from the store.

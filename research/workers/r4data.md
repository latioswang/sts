# Worker task: Phase R4 A10 run data (NAME = your worker name; W and SEED below)

Pre-registered in research/program.md ("Phase R4"). Generate 30,000 **A10** games of run states with the current A10
best, keep 32 random states per run, and push the subsample and the full data to the Hugging Face store
(`research/data_store.json`; the token is a network secret, nothing to configure). Assignments:
r4w7 -> W=7, SEED=800000 (also builds the A10 holdout, line 3 below); r4w8 -> W=8, SEED=830000; r4w9 -> W=9, SEED=860000.

Before the queue: if `/home/user/sts_lightspeed/build/fair_eval` is missing, run `bash research/setup.sh` and the three
repro_check gates (README step 3) — stop and report if any is not bit-identical. `python3 research/tools/data_sync.py ls`
must list `rundata`.

Queue lines (replace W and SEED; the holdout line is for W=7 only, skip it otherwise):
```
ASC=10 PARF=research/best_a10_params.txt LOGP="--param rv_log=1" research/tools/sv_gen.sh rundata_a10_wW 30000 100 SEED
python3 research/vf/subsample_runs.py research/vf/data/rundata_a10_wW/all research/vf/data/rundata_a10_wW_k32/all --k 32
mkdir -p research/vf/data/rundata_a10_hold/all && for k in 0 1 2 3 4 5 6 7 8 9; do ln -sf $(pwd)/research/vf/data/rundata_a10_w7/chunk_$k/run_*.bin research/vf/data/rundata_a10_hold/all/; done && ls research/vf/data/rundata_a10_hold/all | wc -l && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_a10_hold
STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_a10_wW_k32
STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_a10_wW
```
If a push fails on HTTP 429 (commit rate limit), wait 15 minutes and re-run that push line; it skips stored files.
Hand back: the SV GEN / SUBSAMPLE / PUSH ... DONE lines (and the holdout file count for W=7) pasted into
research/workers/<name>.log.md, committed and pushed to worker/<name>.

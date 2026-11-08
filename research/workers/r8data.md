# Worker task: Phase R8 A20 run data (NAME = your worker name; W and SEED below)

Pre-registered in research/program.md ("Phase R8"). Generate 30,000 **A20** games of run states with the current A20
best (`a20_relic20`), keep 32 random states per run, pack both sets into compressed shards (`research/vf/runstore.py`,
47–73x smaller) and push only the shard sets to the Hugging Face store (`research/data_store.json`; the token is a network
secret, nothing to configure). Assignments: r8w10 -> W=10, SEED=1000000 (also builds the A20 holdout, line 4 below);
r8w11 -> W=11, SEED=1030000; r8w12 -> W=12, SEED=1060000.

Before the queue: README steps 1–3 (branch, setup, the three repro_check gates — stop and report if any is not
bit-identical). `python3 research/tools/data_sync.py ls` must list `rundata_w1_k32_z`.

Queue lines for `research/queue.txt` (replace W and SEED; the holdout line is for W=10 only, skip it otherwise), then run
`research/tools/runq.sh` in the background (restart it after its 2-hour stop: it resumes):
```
ASC=20 PARF=research/best_a20_params.txt LOGP="--param rv_log=1" research/tools/sv_gen.sh rundata_a20_wW 30000 100 SEED
python3 research/vf/subsample_runs.py research/vf/data/rundata_a20_wW/all research/vf/data/rundata_a20_wW_k32/all --k 32
mkdir -p research/vf/data/rundata_a20_hold/all && for k in 0 1 2 3 4 5 6 7 8 9; do ln -sf $(pwd)/research/vf/data/rundata_a20_w10/chunk_$k/run_*.bin research/vf/data/rundata_a20_hold/all/; done && ls research/vf/data/rundata_a20_hold/all | wc -l
python3 research/vf/runstore.py pack research/vf/data/rundata_a20_hold/all research/vf/data/rundata_a20_hold_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_a20_hold_z
python3 research/vf/runstore.py pack research/vf/data/rundata_a20_wW_k32/all research/vf/data/rundata_a20_wW_k32_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_a20_wW_k32_z
python3 research/vf/runstore.py pack research/vf/data/rundata_a20_wW/all research/vf/data/rundata_a20_wW_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_a20_wW_z
```
`runstore.py pack` byte-verifies every file before it returns (it prints `verify OK`; a failure exits non-zero and stops
the line before the push). If a push fails on HTTP 429 (commit rate limit), wait 15 minutes and re-run that line.
Hand back: the SV GEN / SUBSAMPLE / PACK / PUSH ... DONE lines (and the holdout file count for W=10) pasted into
research/workers/<name>.log.md, committed and pushed to worker/<name>. Do not delete anything from the store.

# Worker task: Phase X1 A0 exploration run data (NAME = your worker name; W and SEED below)

Pre-registered in research/program.md ("Phase X1"). Generate 30,000 **A0** games of run states with the current A0 best
(`vr5relic20`) plus **exploration** (`explore_eps=0.2`: each card reward and boss relic is, with probability 0.2, picked
uniformly at random — the run value then sees the outcomes of choices the KB avoids), keep 32 random states per run, pack
both sets into compressed shards (`research/vf/runstore.py`) and push only the shard sets to the Hugging Face store.
Assignments: x1w13 -> W=13, SEED=2000000 (also builds the exploration holdout, line 4 below); x1w14 -> W=14, SEED=2030000;
x1w15 -> W=15, SEED=2060000.

Before the queue: README steps 1–3 (branch, setup, the three repro_check gates — stop and report if any is not
bit-identical). `python3 research/tools/data_sync.py ls` must list `rundata_w1_k32_z`.

Queue lines for `research/queue.txt` (replace W and SEED; the holdout line is for W=13 only), then run
`research/tools/runq.sh` in the background (restart it after each 2-hour stop: it resumes):
```
LOGP="--param rv_log=1 --param explore_eps=0.2" research/tools/sv_gen.sh rundata_x1_wW 30000 100 SEED
python3 research/vf/subsample_runs.py research/vf/data/rundata_x1_wW/all research/vf/data/rundata_x1_wW_k32/all --k 32
mkdir -p research/vf/data/rundata_x1_hold/all && for k in 0 1 2 3 4 5 6 7 8 9; do ln -sf $(pwd)/research/vf/data/rundata_x1_w13/chunk_$k/run_*.bin research/vf/data/rundata_x1_hold/all/; done && ls research/vf/data/rundata_x1_hold/all | wc -l
python3 research/vf/runstore.py pack research/vf/data/rundata_x1_hold/all research/vf/data/rundata_x1_hold_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_x1_hold_z
python3 research/vf/runstore.py pack research/vf/data/rundata_x1_wW_k32/all research/vf/data/rundata_x1_wW_k32_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_x1_wW_k32_z
python3 research/vf/runstore.py pack research/vf/data/rundata_x1_wW/all research/vf/data/rundata_x1_wW_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_x1_wW_z
```
`sv_gen.sh` uses `research/best_params.txt` (the A0 best) by default. `runstore.py pack` byte-verifies before it returns.
If a push fails on HTTP 429, wait 15 minutes and re-run that line. Hand back: the SV GEN / SUBSAMPLE / PACK / PUSH ... DONE
lines (and the holdout count for W=13) in research/workers/<name>.log.md, committed and pushed to worker/<name>. Do not
delete anything from the store.

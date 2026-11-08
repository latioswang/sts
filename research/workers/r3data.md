# Worker task: Phase R3 run data (NAME = your worker name; W and SEED below)

Pre-registered in research/program.md ("Phase R3"). Generate 30,000 A0 games of run states with the A0 best, keep 32
random states per run for training, and push both the subsample and the full data to the Hugging Face store
(`research/data_store.json`; the token is a network secret, nothing to configure). Assignments:
r3w4 -> W=4, SEED=440000; r3w5 -> W=5, SEED=470000; r3w6 -> W=6, SEED=500000.

Before the queue: `git fetch origin autoresearch/ironclad && git merge --no-edit origin/autoresearch/ironclad` on your
branch. If `/home/user/sts_lightspeed/build/fair_eval` is missing (container was reset), run `bash research/setup.sh`
and the three repro_check gates again (README step 3) — stop and report if any is not bit-identical.
Check the store: `python3 research/tools/data_sync.py ls` must list `rundata` and `leafdata`.

Queue lines (replace W and SEED):
```
LOGP="--param rv_log=1" research/tools/sv_gen.sh rundata_wW 30000 100 SEED
python3 research/vf/subsample_runs.py research/vf/data/rundata_wW/all research/vf/data/rundata_wW_k32/all --k 32
STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_wW_k32
STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_wW
```
Hand back: the SV GEN / SUBSAMPLE / PUSH ... DONE lines pasted into research/workers/<name>.log.md, committed and pushed
to worker/<name>. Expected: ≈ 7 h of generation (300 chunks of 100 games at ≈ 80–90 s), then uploads.

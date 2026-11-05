# Worker task: Phase P1 move-prior data on the real-game-matched simulator (NAME = your worker name; W and SEED below)

Pre-registered in research/program.md ("Phase P1"). Generate 4,000 **A0** games with the current A0 best (`vr5relic20`) on
the **fidelity simulator** (latioswang/sts_lightspeed at research/patches/sts_lightspeed_fidelity.commit) with the policy
logger at a **strong search** (`pf_log=1`, `sims=2000`: every logged root decision carries the 2,000-simulation visit
distribution — the target a better move prior learns from), pack the logs into compressed shards and push them.
Assignments: p1wW for W = 16..22, SEED = 3000000 + (W-16) x 100000 (p1w16 -> 3000000, p1w17 -> 3100000, ..., p1w22 -> 3600000).

Setup (instead of README steps 2–3): `git checkout -B worker/<NAME> origin/autoresearch/ironclad`; repo at /home/user/sts;
`STS_SIM=fidelity bash research/setup.sh` (builds the fork; its last line names the commit — must start with dfda16c).
Gates (the old repro_check gates do not apply on this simulator — results differ by design): (1)
`/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 $(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')`
must print `PASS`; (2) 20 games `--seed_start 3900000 --count 20` with the best params must finish with 0 aborted rows.
Stop and report if either fails. `python3 research/tools/data_sync.py ls` must list `rundata_w1_k32_z`.

Queue lines for `research/queue.txt` (replace W and SEED), then `research/tools/runq.sh` in the background (restart it
after each 2-hour stop: it resumes):
```
LOGP="--param pf_log=1 --param sims=2000" research/tools/sv_gen.sh poldata_p1_wW 4000 100 SEED
python3 research/vf/runstore.py pack research/vf/data/poldata_p1_wW/all research/vf/data/poldata_p1_wW_z/all --glob 'pol_*.bin' && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push poldata_p1_wW_z
```
Hand back: the gate lines, `SV GEN ... DONE`, `PACK ... verify OK`, `PUSH ... DONE` in research/workers/<NAME>.log.md,
committed and pushed to worker/<NAME>. Do not delete anything from the store.

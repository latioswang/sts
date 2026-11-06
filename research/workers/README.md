# Worker sessions (parallel compute for the Ironclad month program)

The coordinator session runs `research/program.md`. A worker is a separate cloud session (its own VM: 4 vCPUs,
16 GB RAM, 30 GB disk) that runs one self-contained batch and hands back only small files through git. Workers never
edit `research/program.md`, `research/results.tsv`, `research/REPORT.md` or `research/progress/`; the coordinator merges.

**Standing owner approval (stated by the owner in the coordinator chat, 2026-11-02 and again 2026-11-05):** when
`research/tools/runq.sh` stops at the 2-hour background limit or after a container restart, restart it immediately and
without asking — it resumes from `research/queue.current` and changes nothing about the batch. This approval covers only
that restart of your own queue runner, nothing else. (Three workers on 2026-11-05 lost 1–2 hours each waiting to be asked.)

## Every worker, in order
1. `git checkout -B worker/<NAME> origin/autoresearch/ironclad` (NAME is given in your task file).
2. The repo must be at `/home/user/sts` (the simulator build hard-codes that path; symlink it if your clone is elsewhere). `bash research/setup.sh` (since 2026-11-06 it clones the **fidelity fork** `latioswang/sts_lightspeed` at the pinned commit and builds `fair_eval`; `STS_SIM=legacy` builds the old simulator).
3. Bit-identity gate (fork; since 2026-11-06) — all three must print `OK bit-identical`, otherwise stop and report:
   `research/tools/repro_check.sh fid_x1add 16 fs1210000n1000`, `research/tools/repro_check.sh fid_a10_r81 16 fs1211000n1000`,
   `research/tools/repro_check.sh fid_a20_r121s32f 16 fs1212000n1000`.
   (Legacy-simulator batches, `STS_SIM=legacy` only: `repro_check.sh mapd085 16`, `heart_bfrelic10 16`, `a20_restboss1 16`.)
4. `export RESULTS_TSV=$PWD/research/results.d/<NAME>.tsv` in every shell that runs experiments, so result rows go to
   your own file. Write your notes (numbers pasted from tool output, never typed) to `research/workers/<NAME>.log.md`.
5. Put the lines of your task file into `research/queue.txt` and run `research/tools/runq.sh` in the background
   (max timeout 7,200,000 ms). Watch its output with a Monitor on `^(vs |KEEP|pooled |no \(pooled|== .*(RESUME|START|END rc=[1-9]))|Traceback|FAIL|QUEUE EMPTY|SV id=|VR id=|best: .*candidate`.
   Background commands die at 2 hours: restart `runq.sh`; it resumes the interrupted line from `research/queue.current`.
   The owner confirmed on 2026-11-02 at 14:28 UTC, in the coordinator session, that `runq.sh` is restarted immediately
   after every 2-hour stop.
6. After every finished line: `git add` the new files under `research/runs/`, `research/models/` (only `*.txt` and `*.meta`
   you created), `research/results.d/<NAME>.tsv`, `research/workers/<NAME>.log.md`, and any `research/best_*` file a keep
   changed; commit (end the message with the attribution lines the system gives you) and `git push -u origin worker/<NAME>`.
   Never commit `research/vf/data/` (large, gitignored).
7. When the queue is empty: final commit + push, then post a short summary in the session: the result lines, pasted.

## Rules (same as the main program)
- Equal compute: `sims` per decision fixed, or calibrated wall-clock (`research/tools/calibrate.sh` on an idle VM).
- Keep rule: paired bootstrap, dscore > 0 and P ≥ 0.9 (val 300; pooled 500 for Heart/A20 via `exp_pooled.sh`).
- Never touch the `test` split. Leak test (`fair_eval --leaktest ...`) before any new parameter combination is evaluated.

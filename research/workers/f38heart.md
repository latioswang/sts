# Worker task: Phase F38 (b) Heart path-decision data from the Heart best f16heart (branch kind 6: with key features and Heart labels) — NAME = your worker name

Pre-registered in research/program.md ("Phase F38", 2026-11-09 05:40 UTC). The playing agent is research/best_heart_params.txt (f16heart, keys=1). Offline measurement mode (never used in play):
`fair_eval --branch_eval 12 M --branch_kind 6` plays each seed by the Heart agent to its (seed mod 12)-th map choice with >= 2 legal
next nodes, then plays every option out M times from re-seeded copies and writes `pathk_<seed>.bin` (rows of 666 run features +
4 key features of the pre-move state + 20 path features + 8 tail floats; a playout's win counts only if it reached act 4).
Many seeds end with no decision (tail n = -1): normal. Kind 6 needs a build from autoresearch/ironclad at or after the commit that added it.

Assignments:
- f38w6  -> Heart data: ASC=0 PF=research/best_heart_params.txt START=5730000 N=3000 M=8  SET=f38_heart_w1
- f38w7  -> Heart data: ASC=0 PF=research/best_heart_params.txt START=5733000 N=3000 M=8  SET=f38_heart_w2
- f38w8  -> Heart data: ASC=0 PF=research/best_heart_params.txt START=5736000 N=3000 M=8  SET=f38_heart_w3
- f38w9  -> Heart data: ASC=0 PF=research/best_heart_params.txt START=5739000 N=3000 M=8  SET=f38_heart_w4
- f38w10 -> Heart held-out: ASC=0 PF=research/best_heart_params.txt START=5745000 N=800 M=16 SET=f38_hold_heart

Before the run: README steps 1–3 (branch `worker/<NAME>` from origin/autoresearch/ironclad, `bash research/setup.sh`, the three
fork repro_check gates — stop and report if any is not bit-identical). Check: `grep -c "kind == 6" research/cpp/FairAgent.cpp` must
print a number > 0, and `grep keys research/best_heart_params.txt` must print keys=1.

Run (RESUMABLE — only seeds with no line in summary.tsv yet are run):
`research/tools/f33_gen.sh research/vf/data/SET START N ASC PF 12 M 6`
then: `python3 research/vf/runstore.py pack research/vf/data/SET research/vf/data/SET_z/all --glob 'pathk_*.bin' && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push SET_z`

**Keep the container alive:** run the generator as a harness-tracked background task (the Bash tool with run_in_background), NOT
with setsid/nohup. Keep a 30-minute Monitor (or a send_later check) chained while it runs. After a container restart or the
2-hour background limit, run the same f33_gen.sh command again (tracked) — it skips finished seeds. Do not delete or regenerate
finished files. Expected time ≈ 3–4 h (held-out set ≈ 1.5–2 h). If a push fails on HTTP 429, wait 15 minutes and re-run that line.
Hand back: the last `F33 GEN ...` line and the PACK / PUSH ... DONE lines in research/workers/<name>.log.md, committed and pushed
to worker/<name>; message @parent with them (include any crashed seed: a summary row with column 10 = 2). Never commit
research/vf/data/.

# E5 — GPU plan: check spire-engine's ironclad-v1 and time its training

Prepared 2026-11-16. Sources were read at public HEAD `420d17b` (raw files) and from the live viewer
(`spire-viewer.pages.dev`). Their code was not built or run on this machine. The downloaded wasm and
weights were only inspected read-only (section headers, strings, hashes). The helper programs inside
the bootstrap script were run here against a JavaScript mock of the engine's interface.
Script: `research/tools/e5_bootstrap.sh`. Background: `x1_spire_engine_recipe.md`.

## 0. Answer in brief

- **ironclad-v1 can be evaluated exactly from public material.** The viewer serves the WebAssembly
  engine the network was released with, next to the weights. That engine already has a greedy
  "network plays" command (`net_step`), and it takes a `count`. It has no imports, so plain `node`
  can drive it headless with about 40 lines of JavaScript. This is route A, the primary one.
- **Route B** goes through public HEAD with `run_model.upgrade_state`, which converts encoding 1 to
  encoding 3. The author supports it "to measure it on this build or as a baseline". However, it drops
  two Ironclad hand features (Feel No Pain and Dark Embrace) and plays on the newer engine. Treat it as
  a diagnostic, and as the warm-start checkpoint for E5b.
- **Route C (an older commit) is not available.** The commits named by the release (`9880f58`), by
  the evaluation (`4850b5e`) and by `synergy.py` (`41bbd09`) all return 404. The public history is
  three export commits.
- E5a needs only CPU. E5b needs the GPU. Planned total is 1–2.5 hours on one RTX 4090 from vast.ai,
  about USD 0.5–1.5 of the USD 20 budget.

## 1. Can ironclad-v1 be evaluated? Evidence

### Route A: the release's own WebAssembly engine (works; primary)

| Fact | Evidence |
|---|---|
| The live site pins an engine per network. ironclad-v1 → `/assets/sts.87323b801b.wasm` (silent-v1 shares it). The current engine is a different file, `sts.f665a570c3.wasm`. | live `networks.json` field `"engine"`; live `engine.js:7` `ENGINE = "/assets/sts.f665a570c3.wasm"`; repo `web/networks.json:8` `"engine": "models/ironclad-v1.sts.wasm"` |
| Docs: "Released networks keep playing on the engine they were released with" | `docs/network-inputs.md:113-115` |
| Files and their sha256 (the file name is the sha256 prefix): weights `fb21879b8d2ff1e1…f9d191` (4,466,672 B); wasm `87323b801b676048…48fdf` (2,282,694 B) | downloaded and hashed here |
| Weights metadata: `encoding_version 1`, `run_int_len 1081`, `run_float_len 2114`, `actions 171`, d 128 / 6 layers / 4 heads, `source dh12`, `update 200`, `steps 52428800`, `git_commit 9880f582…` | safetensors header, read here |
| The wasm has no imports. It exports `memory, alloc, dealloc, call, load_net, out_ptr`. It was built by rustc 1.98.0, with no SIMD. | section parse of the wasm (export and producers sections) |
| Calling protocol: write JSON at `alloc`, `call(ptr,len)`, read the reply at `out_ptr()`; `load_net(ptr,len)` takes the safetensors bytes | `crates/sts-wasm/src/lib.rs:1-4, 36-39, 644-707`; `engine.js` instantiates with `{}` |
| `new_run` (seed, ascension, character, final_act, neow_full; defaults true/true); `net_step`: "the network takes up to `count` decisions"; `view` gives `outcome`, `obs.{act,floor,hp,deck,relics}` | `lib.rs:10-41, 540-585, 395-424` |
| `net_step` is greedy: "the likeliest legal action slot each time … as training plays it (`train_runs.py --eval`)" | `crates/sts-net/src/play.rs:1-3, 41-69` |
| The old engine has these commands: its strings include `no network loaded`, `count`, `network`, `took`, `new_run`, `final_act`, `neow_full`, and `the weights were made for another encoding`. (`net_step` itself is 8 bytes and compiles to an integer compare, so it is not a string.) The site's current `app.js` sends `{cmd:"net_step",count:1}` to every network's engine. | strings in `sts.87323b801b.wasm`; live `app.js` |
| It is an older build than HEAD: it has no `interface` string (HEAD added `INTERFACE` and `info.interface`), and `app.js` treats engines without it as "version 1" | `lib.rs:520-525`; `app.js` |
| `load_net` refuses a file made for another encoding. A successful load therefore proves the engine reads encoding 1 rows. | `crates/sts-net/src/lib.rs:252-265`; `lib.rs:684-701` |

Win definition: `outcome == "Won"`. With `final_act` true and no keys, beating the Act 3 boss (both
bosses at A20) ends the run as a win. The Ironclad never takes the Sapphire key (model card,
"Limits"). Evaluation defaults match the author's: `Envs(final_act=true)` (`crates/sts-py/src/lib.rs:1002`)
and `RunConfig::new` gives `neow_full: true, final_act: true` (`crates/sts-core/src/run/mod.rs:65`).

Seeds: the author's run j has seed 300,000 + j. `train_runs.py:154` sets `EVAL_SEED = 300_000`;
`evaluate` builds `RunEnv(256, seed=EVAL_SEED)` (`:1033-1066`); slot seeds are `seed + episode·n +
index` (`crates/sts-py/src/lib.rs:420`); `first_episodes` keeps runs in game order (`train_runs.py:1002`).
1,024 runs therefore means seeds 300,000–301,023.

Held-out seeds 900,000–901,023 overlap none of the author's default seed ranges:
- 100k and 200k (fights);
- 300,000–301,023 (eval and checkpoint choice);
- 301,024+ and 308,192+ (`seed_rates.py:108`);
- 400,000 + 10,000·asc (`win_calibration.py:29`);
- 500k (`synergy.py:52`);
- 600k and 700k (practice and scenarios; `soak_runs.py:34`);
- 910,000+ (`run_probe.py:298`; the gallery's A20 win, seed 910,047, came from there);
- 7M;
- training at 2⁴¹ (`train_runs.py` `--seed`).

What can differ from their 21.9%:
1. Their measurement used bfloat16 autocast on a GPU; the wasm runs float32. This flips the argmax
   only in near-ties.
2. The wasm was built at some commit with encoding 1, not necessarily `4850b5e`, where 21.9% was
   measured (`docs/models/ironclad-v1.md:54`). They say the A20 number did not move between the code of
   10-19 and 10-21 (`:63`).
3. Timeouts: `max_steps` 8000 action slots there (`train_runs.py` `--max-steps`), 8000 decisions here.
   The model card says "No run timed out".

Fair-play check for our "leak-free" standard: `net_step` sees only `encode_run`'s row. The draw pile is
sorted, there are no RNG counters, and Runic Dome zeroes intents (`docs/network-inputs.md`).

### Route B: public HEAD plus `upgrade_state` (should work; diagnostic only)

- `load_release` refuses the file because the encoding differs (`python/release/export_release.py:109-118`).
- `run_model.upgrade_state` (`python/core/run_model.py:414-460`) carries an encoding 1 checkpoint over.
  It keeps old weights in place, zeroes the new inputs and grows the embeddings
  (`fight_model.grow_embeddings`, `:148-160`). It drops "Feel No Pain's and Dark Embrace's hand
  columns", and orbs are new (`docs/network-inputs.md:115-121`). It has a test, `python/tests/test_upgrade.py`.
- `train_runs.load` already calls it (`train_runs.py:1089-1096`). The script wraps the release tensors
  as a run checkpoint `{"net", "args": {d,layers,heads}, "update", "steps", "runs"}`. `--init` takes
  that as a whole run network (`:1768-1775`), and `--eval` scores it (`:1691-1704`).
- Caveats: Feel No Pain and Dark Embrace matter to the Ironclad, and the HEAD engine has two more
  weeks of rule fixes. A gap between A and B therefore measures drift, not the claim. If a shape
  mismatch makes the load fail, the script reports it and E5b starts from scratch instead.

### Route C: the release commit (not available)

`raw.githubusercontent.com/jackybabes/spire-engine/<sha>/README.md` returns 404 for `9880f582…`,
`4850b5e` and `41bbd09`. `synergy.py:20-22` says an encoding 1 network "runs only with the code it was
released from: a worktree at that commit". That commit is not public.

## 2. The bootstrap script (`research/tools/e5_bootstrap.sh`)

Target: a fresh vast.ai container running as root. Use image
`pytorch/pytorch:2.7.1-cuda12.6-cudnn9-devel`, or any CUDA 12 devel image (without torch, the script
makes a venv and pip-installs `torch numpy`). Phases run in order and are selected with `E5_PHASES`.
A failed phase is reported and the next one runs; a failed setup stops the script.

| Phase | Commands (from their README / scripts) | Output |
|---|---|---|
| `setup` | apt (build-essential, git, curl, xz); rustup stable (the wasm was built with 1.98; pyo3 0.29.3 and `is_multiple_of` need a recent toolchain); Node v20.18.1 tarball; `git clone` and `checkout --detach 420d17b…`; download the weights, wasm and gallery run, checked against the sha256 above; write helper programs | `E5RESULT {"exp":"env",…}` (GPU, nproc, torch, rustc, node, commit) |
| `build` | `PYO3_PYTHON=$PY sh python/build.sh` (= `cargo build --release -p sts-py`, copied to `python/sts.abi3.so`); `python python/tests/test_sts.py`; `python python/tests/bench.py run 256 2000` | `build` line: encoding (expect 3), build seconds, random-play steps/s |
| `e5a_wasm` | `node e5_wasm_eval.cjs`: `load_net` (refusal = fatal); replay the gallery A20 win with `load`+`goto` (an engine consistency check); determinism and timing pilot (2 seeds twice); then `$JOBS = nproc−2` node processes over shards of seeds 300000+j and 900000+j at A20 (`E5_EXTRA_ASCS="0 10"` adds A0 and A10 on the 300k seeds) | per set: `{"exp":"E5a-wasm","win_rate","se","ci95_wilson","floor","reached_act2/3","timed_out","errors","sec_per_run","diff_pp","z"}`; per-run JSONL with deck and relics |
| `e5a_py` | `python python/tests/test_upgrade.py`; `e5_py_eval.py` (convert, save `ironclad-v1-enc3.pt`, greedy bf16 play with per-seed records, the same loop as `train_runs.evaluate`); `python python/training/train_runs.py --eval ironclad-v1-enc3.pt --ascension 20 --eval-runs 1024` (their own entry point) | `E5a-py-convert` (missing/unexpected keys), `E5a-py-head` per set, `E5a-agree-wasm-vs-py-s*` (per-seed agreement), the `train_runs --eval` line |
| `e5b_speed` | `train_runs.py --name e5-speed-{eager,fixed,fixed_overlap} --init ironclad-v1-enc3.pt --ascension 20 --envs 2048 --steps 128 --updates 6 --gamma 0.9995 --win-reward 3 --practice 0.3 --practice-eval 0 --eval-every 0 --port 0` plus nothing, `--fixed-shapes`, or `--fixed-shapes --overlap`; nvidia-smi sampled every 5 s | `E5b-speed-*` (median steps/s, skipping update 1), `*-gpu` (utilisation, peak memory), `E5b-projection` (GPU-hours and USD for 0.25/0.5/1/2 B steps) |
| `e5b_curve` | `train_runs.py --name e5-scratch --layers 6 --ascension 0 --envs 2048 --steps 128 --updates 100 --win-reward 3 --practice 0.3 --practice-eval 0 --eval-every 25 --eval-runs 256 --save-every 50 --port 0` + the fastest config; capped by `timeout 3h` | `E5b-curve-log` every 5 updates (floor, act reach, win rate, steps/s), `E5b-curve-eval` rows |
| `pack` | total wall time and USD at `E5_PRICE_PER_HOUR`; `tar` of `results/` (JSONL, logs, `log.csv`, `eval.csv`) | `/root/e5/e5_results.tgz` |

E5b settings follow the final stage (2,048 games × 128 steps, γ 0.9995, win reward 3, 30% practice
fights; `docs/models/ironclad-v1.md:42-47`). Two things cannot be copied:
- **Human data** (the 2020 metrics dump for boss practice and imitation) is not public. Practice falls
  back to the built-in Ironclad archetype decks.
- **Habit weights** were never published.

So the curve measures cost and the early slope, not their recipe.

### Running it on vast.ai (no project details leave our side; the script contains none)

```bash
pip install vastai && vastai set api-key "$VAST_KEY"        # on our side only, never on the rented box
vastai search offers 'gpu_name=RTX_4090 num_gpus=1 cpu_cores_effective>=24 cpu_ram>=64 disk_space>=60 cuda_vers>=12.6 reliability>0.98 inet_down>=300 rentable=true' -o 'dph_total'
vastai create instance <OFFER_ID> --image pytorch/pytorch:2.7.1-cuda12.6-cudnn9-devel --disk 60 --ssh --direct
vastai ssh-url <INSTANCE_ID>                                # -> ssh://root@HOST:PORT
scp -P PORT research/tools/e5_bootstrap.sh root@HOST:/root/
ssh -p PORT root@HOST 'nohup bash /root/e5_bootstrap.sh > /root/e5.log 2>&1 &'
ssh -p PORT root@HOST 'grep ^E5RESULT /root/e5.log'          # poll
scp -P PORT root@HOST:/root/e5/e5_results.tgz . && scp -P PORT root@HOST:/root/e5.log .
vastai destroy instance <INSTANCE_ID>                        # billing stops only on destroy (stopped boxes bill storage)
```

The CLI syntax is from general knowledge; check it against `vastai --help` before use.

**Provider choice.** vast.ai is cheaper per hour. A community RTX 4090 costs roughly USD 0.30–0.60/h
(general knowledge; check live offers). AWS has no 4090. The comparable options are g6 (L4, about half
a 4090) or g5 (A10G), at about USD 0.8–1.3/h on demand with 4–16 vCPU. Rollouts are CPU-bound, so on
vast filter on `cpu_cores_effective >= 24` and check `nproc` in the `env` line.

## 3. Expected times and cost (one RTX 4090 + 24 cores)

| Step | Wall-clock | GPU used? | Basis |
|---|---|---|---|
| setup | 3–6 min | no | apt, rustup, node, clone |
| build | 3–8 min | no | release build of the engine and the pyo3 module on 24 cores |
| E5a-wasm (2 × 1,024 A20 runs) | 5–35 min; the pilot prints an ETA | no | "a few milliseconds a decision in WebAssembly" (`docs/architecture.md:77`). A20 runs are shorter than A0's "about a thousand decisions", so about 2–10 s per run, 22 processes in parallel. +5–15 min per extra ascension |
| E5a-py (2 × 1,024 runs + `--eval`) | 5–15 min | yes, lightly | batched greedy play, 256 games at a time |
| E5b-speed (3 × 6 updates of 262,144 steps) | 10–25 min | yes | 9k steps/s eager on their 5070 Ti; 8.7 s per 262k-step update (about 30k/s) with fixed shapes (`docs/models/ironclad-v1.md`, `core/fixed_shapes.py`), plus compile time |
| E5b-curve (100 updates = 26.2 M steps) | 15–50 min (+ 4 evals × 256 runs) | yes | the same rates |
| **Total** | **about 1–2.5 h** | GPU busy about 40% of it | **USD 0.4–1.5 at 0.35–0.6/h** |

What a full replication would cost follows from the `E5b-projection` line. With the x1 estimate of
0.5–1.3 B steps for one clean pass:
- at 15–30k steps/s that is 5–24 GPU-hours (USD 2–15);
- six-day trial-and-error chains like theirs cost several times that.

E5a can also run on any CPU machine with node (two files, 7 MB), so it can be repeated off the GPU box.

**Reading the results.** At p ≈ 0.22 and n = 1,024, one standard error is 1.3 points.
- **Reproduction:** wasm on seeds 300k is within ±2.6 points of 21.9%. Expect it closer, since the
  play is deterministic and greedy; only ties and engine-version drift should move it.
- **Held-out:** wasm on seeds 900k, with its 95% interval. The gap to the 300k set estimates selection
  optimism; its standard error is about 1.8 points.
- **Route B:** a large gap from A to B (several points) points to Feel No Pain/Dark Embrace and engine
  drift. B must not be used to judge the claim.

## 4. Risks and fallbacks

1. **The old engine ignores `count`, or replies differently.** The driver loops on `net_step` until the
   run ends (tested with a mock that takes one step a call). Unknown fields degrade to null. A
   `load_net` refusal is fatal and reported.
2. **The wasm is slower than claimed** (scalar f32, up to about 150 tokens on deck-building screens).
   The pilot prints an ETA. On a 24-core box even 30 s per run finishes 2,048 runs in about 45 min.
   Alternatively, run E5a on our own CPU fleet.
3. **The engine build is not `4850b5e`.** The gallery replay checks consistency with a recorded A20
   run. Residual drift shows up as a gap that the determinism check cannot explain.
4. **The Rust or pyo3 build fails** (toolchain too old or too new). Rerun with
   `rustup default 1.98.0`, the author's compiler. If the build stays broken, E5a-wasm still runs
   (it needs no build) and the GPU phases are skipped.
5. **Route B load fails** (a shape the upgrade does not grow). This is reported in
   `E5a-py-convert.missing/unexpected`; E5b then falls back to `--layers 6` from scratch.
6. **`--fixed-shapes`/`--overlap` break on our torch version** (CUDA graphs and inductor need a C
   compiler and Python headers; the devel image has both). The speed phase records each config's
   return code and the curve uses the fastest that worked. With `torch` from pip on a non-devel image,
   inductor may fail; the eager config still runs.
7. **CPU-starved hosts.** Rollouts are rayon-parallel in Rust. A host with few effective cores lowers
   steps/s with the GPU idle. Check the `env` line and `*-gpu util_mean`.
8. **Budget.** Nothing on the box can destroy the instance, so destroy it by hand after `pack`. The
   curve has a hard `timeout 3h`. Worst case is about 4 h, roughly USD 2.
9. **Untrusted code on the rented box.** Their build scripts run there with root, so keep no
   credentials on the box. The script needs none.
10. **Not comparable to their recipe.** There is no human data and no habit weights, so the curve
    prices compute; it does not reproduce their stage chain.

## 5. Tested here vs. untested

- **Tested here:**
  - `bash -n` on the script;
  - `node --check` and `py_compile` on the helpers;
  - the node driver against a JS mock of the protocol: meta, replay, determinism, three-way sharding,
    an error mid-run, a timeout, an engine that ignores `count`;
  - the summarizer (Wilson interval, the difference from the claim, per-seed agreement);
  - `e5_logcsv.py` on a synthetic `log.csv`;
  - a dry run of the `e5a_wasm` and `pack` phases end to end with the mock.
- **Untested** (needs the box): their build, the real wasm, `upgrade_state` on the real weights, and
  `train_runs.py` with these flags.

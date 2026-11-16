#!/usr/bin/env bash
# e5_bootstrap.sh: one unattended pass on a fresh rented GPU container (Ubuntu, root, CUDA 12 devel image,
# e.g. pytorch/pytorch:2.7.1-cuda12.6-cudnn9-devel or nvidia/cuda:12.6.3-cudnn-devel-ubuntu22.04).
#
#   E5a  evaluate the released spire-engine "ironclad-v1" network greedily at A20:
#        (wasm) in the release's own WebAssembly engine, driven headless by node: exact network + engine pair;
#        (py)   at public HEAD 420d17b through run_model.upgrade_state (encoding 1 -> 3): diagnostic only.
#        Seed sets: 300000+j (the author's eval seeds, reproduction) and 900000+j (never used by the author).
#   E5b  PPO smoke runs with train_runs.py: steps/s of three speed settings, then a from-scratch learning curve.
#
# Usage (as root, in the container):
#   nohup bash e5_bootstrap.sh > /root/e5.log 2>&1 &
#   grep '^E5RESULT' /root/e5.log            # machine-readable results, one JSON object per line
#   results and run logs end up in /root/e5/e5_results.tgz
# Knobs (environment variables): E5_PHASES, E5_ASC, E5_RUNS, E5_SEED_SETS, E5_EXTRA_ASCS, E5_JOBS,
#   E5B_SPEED_UPDATES, E5B_CURVE_UPDATES, E5B_CURVE_TIMEOUT, E5_PRICE_PER_HOUR. See the defaults below.
# Nothing here needs credentials; do not put API keys on the rented machine.
set -uo pipefail

WORK=${E5_WORK:-/root/e5}
COMMIT=${E5_COMMIT:-420d17b19ec3dd112f3228c1d0770f78b9914bd5}
REPO_URL=https://github.com/jackybabes/spire-engine
SITE=https://spire-viewer.pages.dev
NET_FILE=ironclad-v1.fb21879b8d.safetensors
NET_SHA=fb21879b8d2ff1e1e42d185bc186cda6c11bf3592db23b93395912e7e7f9d191
WASM_FILE=sts.87323b801b.wasm          # networks.json pins this engine for ironclad-v1 (and silent-v1)
WASM_SHA=87323b801b67604898b2969033cc48ec54747e052cb3627a8ef91446e0c48fdf
GALLERY_FILE=a20_win_L7WC.json         # a recorded A20 Ironclad win (ironclad-runs-habits-10), replay check
GALLERY_SHA=dd3cf392bdbaa0dff8d95888b2acde9c0c9e7d637c3be77d702fb95e09ee9bf8
NODE_VER=v20.18.1
ASC=${E5_ASC:-20}
RUNS=${E5_RUNS:-1024}
SEED_SETS=${E5_SEED_SETS:-"300000 900000"}
EXTRA_ASCS=${E5_EXTRA_ASCS:-""}        # e.g. "0 10": also A0/A10 on the 300000 seeds (author: 93.1 / 75.2%)
PHASES=${E5_PHASES:-"setup build e5a_wasm e5a_py e5b_speed e5b_curve pack"}
NPROC=$(nproc)
JOBS=${E5_JOBS:-$(( NPROC > 3 ? NPROC - 2 : 1 ))}
SPEED_UPDATES=${E5B_SPEED_UPDATES:-6}
CURVE_UPDATES=${E5B_CURVE_UPDATES:-100}
CURVE_TIMEOUT=${E5B_CURVE_TIMEOUT:-3h}
PRICE=${E5_PRICE_PER_HOUR:-0.40}

REPO=$WORK/spire-engine
ASSETS=$WORK/assets
TOOLS=$WORK/tools
RES=$WORK/results
CONV=$WORK/ironclad-v1-enc3.pt
mkdir -p "$WORK" "$ASSETS" "$TOOLS" "$RES"
T_START=$(date +%s)
export PATH="$HOME/.cargo/bin:/opt/node/bin:$PATH"
export PYTHONUNBUFFERED=1

log() { echo "[$(date -u +%H:%M:%S)] $*"; }
# result '{"exp": ...}': one JSON object per line, prefixed E5RESULT, also kept in results.txt
result() { echo "E5RESULT $*"; echo "E5RESULT $*" >> "$RES/results.txt"; }
phase_on() { case " $PHASES " in *" $1 "*) return 0;; *) return 1;; esac; }
elapsed() { echo $(( $(date +%s) - $1 )); }
pick_python() {
  if [ -x "$WORK/venv/bin/python" ]; then echo "$WORK/venv/bin/python"; return; fi
  command -v python >/dev/null 2>&1 && python -c 'import torch' >/dev/null 2>&1 && { command -v python; return; }
  command -v python3 >/dev/null 2>&1 && python3 -c 'import torch' >/dev/null 2>&1 && { command -v python3; return; }
  command -v python3 || command -v python
}
fetch_checked() {  # url dest sha256
  curl -fsSL --retry 3 -o "$2" "$1" || return 1
  local got; got=$(sha256sum "$2" | cut -d' ' -f1)
  [ "$got" = "$3" ] || { log "sha256 mismatch for $2: $got"; return 1; }
}

# ---------------------------------------------------------------------------------------------
# Helper programs (written to $TOOLS; ours, not the repository's)
# ---------------------------------------------------------------------------------------------
write_tools() {
cat > "$TOOLS/e5_wasm_eval.cjs" <<'JS'
'use strict';
// Drives a spire-engine WebAssembly build (crates/sts-wasm: exports memory, alloc, dealloc, call, load_net,
// out_ptr; JSON commands; no imports) headless, as web/engine.js does in the browser.
//   node e5_wasm_eval.cjs --wasm W --net N --mode eval --asc 20 --seed0 300000 --runs 1024 --shard 0 --shards 8 --out f.jsonl
//   node e5_wasm_eval.cjs --wasm W --net N --mode determinism --asc 20 --seed0 300000 --runs 2
//   node e5_wasm_eval.cjs --wasm W --mode replay --log run.json
// --wasm MOCK --mock path.cjs uses a JavaScript stand-in (for testing this driver without the engine).
const fs = require('fs');
const path = require('path');
function arg(name, def) { const i = process.argv.indexOf('--' + name); return i >= 0 ? process.argv[i + 1] : def; }

function loadEngine(wasmPath) {
  if (wasmPath === 'MOCK') return require(path.resolve(arg('mock')))();
  const inst = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(wasmPath)), {});
  const e = inst.exports;
  const enc = new TextEncoder();
  const dec = new TextDecoder();
  const withBytes = (bytes, fn) => {
    const p = e.alloc(bytes.length);
    new Uint8Array(e.memory.buffer, p, bytes.length).set(bytes);
    const n = fn(p, bytes.length);
    e.dealloc(p, bytes.length);
    return JSON.parse(dec.decode(new Uint8Array(e.memory.buffer, e.out_ptr(), n)));
  };
  const call = (cmd) => withBytes(enc.encode(JSON.stringify(cmd)), e.call);
  call.loadNet = (buf) => withBytes(new Uint8Array(buf), e.load_net);
  return call;
}

function playRun(call, seed, asc, maxDec) {
  const t0 = process.hrtime.bigint();
  let v = call({ cmd: 'new_run', seed, ascension: asc, character: 'ironclad', final_act: true, neow_full: true });
  let error = v.error || null;
  let total = 0;
  let calls = 0;
  while (!error) {
    const r = call({ cmd: 'net_step', count: maxDec - total });
    calls += 1;
    if (r.error) { error = r.error; v = call({ cmd: 'view' }); break; }
    v = r;
    const took = typeof r.took === 'number' ? r.took : 1;
    total += took;
    if (r.outcome != null || total >= maxDec || took === 0) break;
  }
  const o = v.obs || {};
  const ms = Number(process.hrtime.bigint() - t0) / 1e6;
  return {
    seed, asc, won: v.outcome === 'Won', outcome: v.outcome == null ? null : v.outcome,
    act: o.act, floor: o.floor, hp: o.hp, max_hp: o.max_hp, gold: o.gold, boss: o.boss,
    decisions: v.steps, net_step_calls: calls, timed_out: !error && v.outcome == null, error,
    deck: (o.deck || []).map((c) => (c && (c.name || c.id)) + (c && c.upgrades ? '+' + c.upgrades : '')),
    relics: o.relics || [], ms: Math.round(ms),
  };
}

function main() {
  const mode = arg('mode', 'eval');
  const call = loadEngine(arg('wasm'));
  const out = arg('out', null);
  const emit = (rec) => { const s = JSON.stringify(rec); if (out) fs.appendFileSync(out, s + '\n'); else console.log(s); };
  if (mode === 'replay') {
    const log = JSON.parse(fs.readFileSync(arg('log'), 'utf8'));
    let v = call({ cmd: 'load', log, by: 'network' });
    if (v.error) { emit({ mode, ok: false, error: v.error }); return; }
    v = call({ cmd: 'goto', index: v.steps });
    emit({ mode, ok: !v.error, steps: v.steps, outcome: v.outcome == null ? null : v.outcome, floor: v.obs && v.obs.floor,
           hp: v.obs && v.obs.hp, error: v.error || null });
    return;
  }
  const meta = call.loadNet(fs.readFileSync(arg('net')));
  if (meta.error) { emit({ mode, fatal: 'load_net', error: meta.error }); process.exit(3); }
  const asc = parseInt(arg('asc', '20'), 10);
  const seed0 = parseInt(arg('seed0', '300000'), 10);
  const runs = parseInt(arg('runs', '1024'), 10);
  const maxDec = parseInt(arg('max-decisions', '8000'), 10);
  if (mode === 'meta') { emit({ mode, meta, net: call({ cmd: 'net' }) }); return; }
  if (mode === 'determinism') {
    for (let j = 0; j < runs; j++) {
      const a = playRun(call, seed0 + j, asc, maxDec);
      const b = playRun(call, seed0 + j, asc, maxDec);
      const same = ['won', 'floor', 'hp', 'decisions', 'gold'].every((k) => a[k] === b[k]) && a.deck.join() === b.deck.join();
      emit({ mode, seed: seed0 + j, same, a: { won: a.won, floor: a.floor, hp: a.hp, decisions: a.decisions, ms: a.ms },
             b: { won: b.won, floor: b.floor, hp: b.hp, decisions: b.decisions, ms: b.ms } });
    }
    return;
  }
  const shard = parseInt(arg('shard', '0'), 10);
  const shards = parseInt(arg('shards', '1'), 10);
  for (let j = shard; j < runs; j += shards) {
    const rec = playRun(call, seed0 + j, asc, maxDec);
    rec.j = j;
    emit(rec);
  }
}
main();
JS

cat > "$TOOLS/e5_summarize.py" <<'PY'
"""Summaries of per-run JSON lines (from e5_wasm_eval.cjs or e5_py_eval.py) as one E5RESULT JSON line.
    python e5_summarize.py --label E5a-wasm --asc 20 --seed0 300000 --runs 1024 FILE [FILE...]
    python e5_summarize.py --compare A.jsonl B.jsonl --label agree   (per-seed agreement of two evaluations)"""
import argparse, json, math, sys

def read(files):
    out = {}
    for f in files:
        with open(f) as fh:
            for line in fh:
                line = line.strip()
                if line.startswith("{"):
                    r = json.loads(line)
                    if "seed" in r and "won" in r:
                        out[r["seed"]] = r
    return out

def wilson(k, n, z=1.96):
    if n == 0:
        return [None, None]
    p = k / n
    c = (p + z * z / (2 * n)) / (1 + z * z / n)
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / (1 + z * z / n)
    return [round(c - h, 4), round(c + h, 4)]

def main():
    p = argparse.ArgumentParser()
    p.add_argument("files", nargs="*")
    p.add_argument("--label", default="eval")
    p.add_argument("--asc", type=int)
    p.add_argument("--seed0", type=int)
    p.add_argument("--runs", type=int)
    p.add_argument("--compare", nargs=2)
    p.add_argument("--claim", type=float, default=None, help="the published rate, to report the difference")
    a = p.parse_args()
    if a.compare:
        x, y = read([a.compare[0]]), read([a.compare[1]])
        common = sorted(set(x) & set(y))
        n = len(common)
        both = sum(x[s]["won"] and y[s]["won"] for s in common)
        same = sum(x[s]["won"] == y[s]["won"] for s in common)
        same_floor = sum(x[s].get("floor") == y[s].get("floor") for s in common)
        print("E5RESULT " + json.dumps({"exp": a.label, "seeds": n, "won_agree": round(same / max(n, 1), 4),
              "floor_equal": round(same_floor / max(n, 1), 4), "won_both": both,
              "won_only_a": sum(x[s]["won"] and not y[s]["won"] for s in common),
              "won_only_b": sum(y[s]["won"] and not x[s]["won"] for s in common)}))
        return
    recs = read(a.files)
    if a.seed0 is not None and a.runs is not None:
        recs = {s: r for s, r in recs.items() if a.seed0 <= s < a.seed0 + a.runs}
    rs = list(recs.values())
    n = len(rs)
    k = sum(bool(r["won"]) for r in rs)
    rate = k / n if n else float("nan")
    ms = [r["ms"] for r in rs if isinstance(r.get("ms"), (int, float))]
    out = {"exp": a.label, "asc": a.asc, "seed0": a.seed0, "runs_expected": a.runs, "runs": n, "won": k,
           "win_rate": round(rate, 4) if n else None,
           "se": round(math.sqrt(rate * (1 - rate) / n), 4) if n else None, "ci95_wilson": wilson(k, n),
           "floor": round(sum(r.get("floor") or 0 for r in rs) / n, 2) if n else None,
           "reached_act2": round(sum((r.get("act") or 0) >= 2 for r in rs) / n, 4) if n else None,
           "reached_act3": round(sum((r.get("act") or 0) >= 3 for r in rs) / n, 4) if n else None,
           "timed_out": sum(bool(r.get("timed_out")) for r in rs),
           "errors": sum(bool(r.get("error")) for r in rs),
           "sec_per_run": round(sum(ms) / len(ms) / 1000, 3) if ms else None}
    if a.claim is not None and n:
        out["claim"] = a.claim
        out["diff_pp"] = round(100 * (rate - a.claim), 2)
        out["z"] = round((rate - a.claim) / math.sqrt(a.claim * (1 - a.claim) / n), 2)
    errs = [r.get("error") for r in rs if r.get("error")]
    if errs:
        out["first_error"] = str(errs[0])[:300]
    print("E5RESULT " + json.dumps(out))

main()
PY

cat > "$TOOLS/e5_py_eval.py" <<'PY'
"""Run from the spire-engine checkout's root. Carries ironclad-v1 (encoding 1) over to this build with
run_model.upgrade_state (docs/network-inputs.md "Older networks"), saves it as a run checkpoint (usable by
train_runs.py --init/--eval) and plays it greedily as train_runs.evaluate does, keeping each run's seed.
    python e5_py_eval.py --weights W --out-ckpt C --asc 20 --runs 1024 --seeds "300000 900000" --outdir D"""
import argparse, json, os, sys, time
sys.path.insert(0, os.path.join(os.getcwd(), "python"))
import numpy as np
import torch
from release.export_release import read_safetensors
from core.run_model import RunNet, upgrade_state
from core.fight_model import masked
from core.sts_env import ENCODING_VERSION, RunEnv, RUN_FLOAT_LEN, RUN_INT_LEN

def convert(weights, out):
    tensors, meta = read_safetensors(weights)
    state = {k: v.clone() for k, v in tensors.items()}
    upgrade_state(state)
    net = RunNet(d=int(meta["d"]), layers=int(meta["layers"]), heads=int(meta["heads"]))
    missing, unexpected = net.load_state_dict(state, strict=False)
    ck = {"net": net.state_dict(), "update": int(meta.get("update", 0) or 0), "steps": int(meta.get("steps", 0) or 0),
          "runs": 0, "seconds": 0.0,
          "args": {"d": int(meta["d"]), "layers": int(meta["layers"]), "heads": int(meta["heads"]), "name": "ironclad-v1-enc3"}}
    torch.save(ck, out)
    return net, meta, list(missing), list(unexpected)

def play(net, device, seed0, runs, asc, batch=256, max_steps=8000, amp_on=True):
    batch = min(batch, runs)
    env = RunEnv(batch, seed=seed0, ascension=asc, max_steps=max_steps, character="ironclad")
    per_game = -(-runs // batch)
    done = np.zeros(batch, dtype=int)
    out = []
    while (done < per_game).any():
        obs = env.observe()
        ints = torch.from_numpy(obs["ints"]).to(device).long()
        floats = torch.from_numpy(obs["floats"]).to(device)
        mask = torch.from_numpy(obs["mask"]).to(device)
        with torch.no_grad(), torch.autocast(device.type, dtype=torch.bfloat16, enabled=amp_on and device.type == "cuda"):
            logits, _ = net(ints, floats)
        status = env.step(masked(logits.float(), mask).argmax(-1).cpu().numpy())
        for g in np.flatnonzero(status):
            if done[g] < per_game:
                e = json.loads(env.last_episode(int(g)))
                e["seed"] = seed0 + int(done[g]) * batch + int(g)  # crates/sts-py Slot::build: seed + episode * n + index
                out.append(e)
            done[g] += 1
    return sorted([e for e in out if e["seed"] < seed0 + runs], key=lambda e: e["seed"])

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--weights", required=True)
    p.add_argument("--out-ckpt", required=True)
    p.add_argument("--asc", type=int, default=20)
    p.add_argument("--runs", type=int, default=1024)
    p.add_argument("--seeds", default="300000 900000")
    p.add_argument("--outdir", required=True)
    p.add_argument("--no-amp", action="store_true")
    a = p.parse_args()
    net, meta, missing, unexpected = convert(a.weights, a.out_ckpt)
    bad = [k for k in missing if not k.startswith(("outcome.", "move_head.", "fight_outcome_head."))]
    print("E5RESULT " + json.dumps({"exp": "E5a-py-convert", "build_encoding": ENCODING_VERSION, "build_run_int_len": RUN_INT_LEN,
          "build_run_float_len": RUN_FLOAT_LEN, "file_encoding": meta.get("encoding_version"), "missing": bad,
          "unexpected": unexpected, "ok": not bad and not unexpected}), flush=True)
    if bad or unexpected:
        sys.exit(2)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    net = net.to(device).eval()
    for seed0 in [int(s) for s in a.seeds.split()]:
        t = time.time()
        eps = play(net, device, seed0, a.runs, a.asc, amp_on=not a.no_amp)
        path = os.path.join(a.outdir, f"py_a{a.asc}_s{seed0}.jsonl")
        with open(path, "w") as f:
            for e in eps:
                e["asc"] = a.asc
                f.write(json.dumps(e) + "\n")
        print(f"E5a-py seed0 {seed0}: {len(eps)} runs in {time.time() - t:.0f}s -> {path}", flush=True)

main()
PY

cat > "$TOOLS/e5_logcsv.py" <<'PY'
"""E5RESULT lines from a train_runs.py run folder's log.csv / eval.csv.
    python e5_logcsv.py RUN_DIR LABEL [every]"""
import csv, json, os, statistics, sys
run_dir, label = sys.argv[1], sys.argv[2]
every = int(sys.argv[3]) if len(sys.argv) > 3 else 1
def rows(name):
    p = os.path.join(run_dir, name)
    return list(csv.DictReader(open(p))) if os.path.exists(p) else []
log = rows("log.csv")
def num(r, k):
    try:
        return float(r[k])
    except (KeyError, TypeError, ValueError):
        return None
sps = [num(r, "steps_per_s") for r in log[1:] if num(r, "steps_per_s")]  # first update: start-up and compiles
summary = {"exp": label, "updates": len(log), "steps": num(log[-1], "steps") if log else None,
           "seconds": num(log[-1], "seconds") if log else None,
           "steps_per_s_median": round(statistics.median(sps)) if sps else None,
           "steps_per_s_max": round(max(sps)) if sps else None}
print("E5RESULT " + json.dumps(summary))
if every > 0:
    keep = ("update", "steps", "seconds", "steps_per_s", "floor", "reached_act2", "reached_act3", "win_rate")
    for i, r in enumerate(log):
        if (i + 1) % every == 0 or i == len(log) - 1:
            print("E5RESULT " + json.dumps({"exp": label + "-log", **{k: num(r, k) for k in keep}}))
    for r in rows("eval.csv"):
        print("E5RESULT " + json.dumps({"exp": label + "-eval", **{k: r.get(k) for k in ("update", "steps", "set", "runs", "floor", "win_rate", "reached_act3")}}))
PY
}

# ---------------------------------------------------------------------------------------------
phase_setup() {
  local t; t=$(date +%s)
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -y >/dev/null && apt-get install -y --no-install-recommends \
    build-essential pkg-config git curl ca-certificates xz-utils procps >/dev/null || { log "apt failed"; return 1; }
  if ! command -v cargo >/dev/null 2>&1; then
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain stable || return 1
  fi
  if ! node -e 'process.exit(parseInt(process.versions.node) >= 18 ? 0 : 1)' >/dev/null 2>&1; then
    curl -fsSL "https://nodejs.org/dist/$NODE_VER/node-$NODE_VER-linux-x64.tar.xz" | tar -xJ -C /opt || return 1
    ln -sfn "/opt/node-$NODE_VER-linux-x64" /opt/node
  fi
  PY=$(pick_python)
  if ! "$PY" -c 'import torch, numpy' >/dev/null 2>&1; then
    # nvidia/cuda images: no torch. A venv avoids the system pip's externally-managed refusal (Ubuntu 24.04).
    apt-get install -y --no-install-recommends python3 python3-dev python3-venv >/dev/null || return 1
    python3 -m venv "$WORK/venv" && "$WORK/venv/bin/pip" install -q --upgrade pip && \
      "$WORK/venv/bin/pip" install -q torch numpy || return 1
    PY="$WORK/venv/bin/python"
  fi
  if [ ! -d "$REPO/.git" ]; then git clone -q "$REPO_URL" "$REPO" || return 1; fi
  git -C "$REPO" checkout -q --detach "$COMMIT" || { git -C "$REPO" fetch -q origin "$COMMIT" && git -C "$REPO" checkout -q --detach "$COMMIT"; } || return 1
  fetch_checked "$SITE/assets/$NET_FILE" "$ASSETS/$NET_FILE" "$NET_SHA" || return 1
  fetch_checked "$SITE/assets/$WASM_FILE" "$ASSETS/$WASM_FILE" "$WASM_SHA" || return 1
  fetch_checked "$SITE/runs/$GALLERY_FILE" "$ASSETS/$GALLERY_FILE" "$GALLERY_SHA" || log "gallery run not fetched (optional)"
  curl -fsSL -o "$ASSETS/networks.json" "$SITE/networks.json" || true
  write_tools
  local gpu; gpu=$(nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader 2>/dev/null | head -1)
  result "$(printf '{"exp":"env","gpu":"%s","nproc":%s,"mem_gb":%s,"python":"%s","torch":"%s","cuda_ok":%s,"rustc":"%s","node":"%s","commit":"%s","setup_s":%s}' \
    "$gpu" "$NPROC" "$(free -g | awk '/Mem:/{print $2}')" "$("$PY" -V 2>&1)" \
    "$("$PY" -c 'import torch;print(torch.__version__)' 2>/dev/null)" \
    "$("$PY" -c 'import torch;print(str(torch.cuda.is_available()).lower())' 2>/dev/null || echo false)" \
    "$(rustc --version 2>/dev/null)" "$(node -v 2>/dev/null)" "$(git -C "$REPO" rev-parse HEAD)" "$(elapsed "$t")")"
}

phase_build() {
  local t; t=$(date +%s)
  PY=$(pick_python)
  export PYO3_PYTHON="$PY"
  ( cd "$REPO" && sh python/build.sh ) > "$RES/build.log" 2>&1 || { tail -40 "$RES/build.log"; result '{"exp":"build","ok":false}'; return 1; }
  ( cd "$REPO" && "$PY" python/tests/test_sts.py ) > "$RES/test_sts.log" 2>&1; local tst=$?
  ( cd "$REPO" && "$PY" python/tests/bench.py run 256 2000 ) > "$RES/bench.log" 2>&1
  local enc; enc=$(cd "$REPO" && "$PY" -c 'import sys; sys.path.insert(0,"python"); import sts; print(sts.ENCODING_VERSION)' 2>/dev/null)
  result "$(printf '{"exp":"build","ok":true,"test_sts_rc":%s,"encoding":"%s","build_s":%s,"bench":"%s"}' \
    "$tst" "$enc" "$(elapsed "$t")" "$(tr '\n' ' ' < "$RES/bench.log" | tr -d '"' | cut -c1-300)")"
}

phase_e5a_wasm() {
  local W="$ASSETS/$WASM_FILE" N="$ASSETS/$NET_FILE" D="node $TOOLS/e5_wasm_eval.cjs"
  PY=$(pick_python)
  $D --wasm "$W" --net "$N" --mode meta > "$RES/wasm_meta.json" 2>&1
  local meta; meta=$(head -c 1500 "$RES/wasm_meta.json")
  result "{\"exp\":\"E5a-wasm-meta\",\"reply\":$(printf '%s' "$meta" | "$PY" -c 'import json,sys;print(json.dumps(sys.stdin.read()))')}"
  grep -q '"fatal"' "$RES/wasm_meta.json" && { log "load_net refused the weights"; return 1; }
  if [ -f "$ASSETS/$GALLERY_FILE" ]; then
    result "{\"exp\":\"E5a-wasm-replay\",\"reply\":$($D --wasm "$W" --mode replay --log "$ASSETS/$GALLERY_FILE" 2>&1 | head -c 600 | "$PY" -c 'import json,sys;print(json.dumps(sys.stdin.read()))')}"
  fi
  # Pilot: 2 seeds played twice (determinism) and timing.
  local t; t=$(date +%s)
  $D --wasm "$W" --net "$N" --mode determinism --asc "$ASC" --seed0 300000 --runs 2 > "$RES/wasm_determinism.jsonl" 2>&1
  local pilot_s; pilot_s=$(elapsed "$t")
  result "$(printf '{"exp":"E5a-wasm-pilot","runs_played":4,"seconds":%s,"jobs":%s,"eta_min_per_1024":%s,"all_same":%s}' \
    "$pilot_s" "$JOBS" "$(( pilot_s * 1024 / 4 / JOBS / 60 + 1 ))" \
    "$(grep -q '"same":false' "$RES/wasm_determinism.jsonl" && echo false || echo true)")"
  local sets="" s a
  for s in $SEED_SETS; do sets="$sets $ASC:$s"; done
  for a in $EXTRA_ASCS; do sets="$sets $a:300000"; done
  for pair in $sets; do
    a=${pair%%:*}; s=${pair##*:}
    local out="$RES/wasm_a${a}_s${s}"; rm -f "$out".*.jsonl
    t=$(date +%s)
    local i pids=""
    for i in $(seq 0 $((JOBS - 1))); do
      $D --wasm "$W" --net "$N" --mode eval --asc "$a" --seed0 "$s" --runs "$RUNS" --shard "$i" --shards "$JOBS" \
         --out "$out.$i.jsonl" 2> "$out.$i.err" &
      pids="$pids $!"
    done
    wait $pids
    local claim=""
    if [ "$s" = 300000 ]; then case $a in 20) claim="--claim 0.219";; 10) claim="--claim 0.752";; 0) claim="--claim 0.931";; esac; fi
    "$PY" "$TOOLS/e5_summarize.py" --label "E5a-wasm" --asc "$a" --seed0 "$s" --runs "$RUNS" $claim "$out".*.jsonl \
      | tee -a "$RES/results.txt"
    cat "$out".*.jsonl > "$out.jsonl" && rm -f "$out".[0-9]*.jsonl
    log "wasm A$a seeds $s: $(elapsed "$t") s"
  done
}

phase_e5a_py() {
  PY=$(pick_python)
  ( cd "$REPO" && timeout 20m "$PY" python/tests/test_upgrade.py ) > "$RES/test_upgrade.log" 2>&1
  result "$(printf '{"exp":"E5a-py-test_upgrade","rc":%s}' "$?")"
  local t; t=$(date +%s)
  ( cd "$REPO" && timeout 60m "$PY" "$TOOLS/e5_py_eval.py" --weights "$ASSETS/$NET_FILE" --out-ckpt "$CONV" \
      --asc "$ASC" --runs "$RUNS" --seeds "$SEED_SETS" --outdir "$RES" ) 2>&1 | tee "$RES/py_eval.log" | grep -E '^E5RESULT|E5a-py' | tee -a "$RES/results.txt"
  local s
  for s in $SEED_SETS; do
    [ -f "$RES/py_a${ASC}_s${s}.jsonl" ] || continue
    local claim=""; [ "$s" = 300000 ] && [ "$ASC" = 20 ] && claim="--claim 0.219"
    "$PY" "$TOOLS/e5_summarize.py" --label "E5a-py-head" --asc "$ASC" --seed0 "$s" --runs "$RUNS" $claim "$RES/py_a${ASC}_s${s}.jsonl" | tee -a "$RES/results.txt"
    [ -f "$RES/wasm_a${ASC}_s${s}.jsonl" ] && "$PY" "$TOOLS/e5_summarize.py" --label "E5a-agree-wasm-vs-py-s$s" \
      --compare "$RES/wasm_a${ASC}_s${s}.jsonl" "$RES/py_a${ASC}_s${s}.jsonl" | tee -a "$RES/results.txt"
  done
  # The author's own entry point on the converted checkpoint (seeds 300000+j), as a check on e5_py_eval.py.
  if [ -f "$CONV" ]; then
    ( cd "$REPO" && timeout 60m "$PY" python/training/train_runs.py --eval "$CONV" --ascension "$ASC" --eval-runs "$RUNS" ) \
      > "$RES/train_runs_eval.log" 2>&1
    result "{\"exp\":\"E5a-py-train_runs-eval\",\"line\":$(grep -E 'won .* of ' "$RES/train_runs_eval.log" | tail -1 | "$PY" -c 'import json,sys;print(json.dumps(sys.stdin.read().strip()))')}"
  fi
  log "E5a py: $(elapsed "$t") s"
}

gpu_monitor_start() { nvidia-smi --query-gpu=timestamp,utilization.gpu,memory.used --format=csv,noheader -l 5 > "$1" 2>/dev/null & echo $!; }
gpu_monitor_stop() {  # pid file label
  kill "$1" 2>/dev/null
  awk -F', ' '{gsub(/ %/,"",$2); gsub(/ MiB/,"",$3); u+=$2; n++; if ($3>m) m=$3} END {printf "E5RESULT {\"exp\":\"%s-gpu\",\"util_mean\":%.1f,\"mem_peak_mib\":%d,\"samples\":%d}\n", lbl, (n?u/n:0), m, n}' \
    lbl="$3" "$2"
}

train() {  # name timeout extra-args...
  local name=$1 to=$2; shift 2
  rm -rf "$REPO/python/runs/$name"
  ( cd "$REPO" && timeout "$to" "$PY" python/training/train_runs.py --name "$name" --port 0 "$@" ) > "$RES/train_$name.log" 2>&1
  local rc=$?
  mkdir -p "$RES/runs/$name"; cp "$REPO/python/runs/$name/"*.csv "$RES/runs/$name/" 2>/dev/null
  return $rc
}

phase_e5b_speed() {
  PY=$(pick_python)
  local init=()
  if [ -f "$CONV" ]; then init=(--init "$CONV"); else init=(--layers 6); fi
  local common=("${init[@]}" --ascension "$ASC" --envs 2048 --steps 128 --updates "$SPEED_UPDATES" --gamma 0.9995 --win-reward 3
                --practice 0.3 --practice-eval 0 --eval-every 0 --save-every 100000)
  local best=0 best_cfg="" cfg
  for cfg in eager fixed fixed_overlap; do
    local extra=()
    case $cfg in fixed) extra=(--fixed-shapes);; fixed_overlap) extra=(--fixed-shapes --overlap);; esac
    local mon; mon=$(gpu_monitor_start "$RES/gpu_speed_$cfg.csv")
    local t; t=$(date +%s)
    train "e5-speed-$cfg" 40m "${common[@]}" "${extra[@]}"; local rc=$?
    gpu_monitor_stop "$mon" "$RES/gpu_speed_$cfg.csv" "E5b-speed-$cfg" | tee -a "$RES/results.txt"
    local line; line=$("$PY" "$TOOLS/e5_logcsv.py" "$RES/runs/e5-speed-$cfg" "E5b-speed-$cfg" 0)
    echo "$line" | tee -a "$RES/results.txt"
    result "$(printf '{"exp":"E5b-speed-%s-run","rc":%s,"wall_s":%s}' "$cfg" "$rc" "$(elapsed "$t")")"
    [ $rc -ne 0 ] && tail -25 "$RES/train_e5-speed-$cfg.log"
    local sps; sps=$(echo "$line" | "$PY" -c 'import json,sys; l=sys.stdin.read().split(" ",1)[1]; print(int(json.loads(l).get("steps_per_s_median") or 0))' 2>/dev/null || echo 0)
    if [ "$rc" -eq 0 ] && [ "${sps:-0}" -gt "$best" ]; then best=$sps; best_cfg=$cfg; fi
  done
  echo "$best_cfg" > "$RES/best_speed_cfg"
  "$PY" - "$best" "$best_cfg" "$PRICE" <<'PY' | tee -a "$RES/results.txt"
import json, sys
sps, cfg, price = float(sys.argv[1]), sys.argv[2], float(sys.argv[3])
out = {"exp": "E5b-projection", "best_cfg": cfg, "steps_per_s": sps, "price_per_hour": price}
for b in (0.25, 0.5, 1.0, 2.0):
    h = b * 1e9 / sps / 3600 if sps else None
    out[f"gpu_hours_{b}B_steps"] = round(h, 1) if h else None
    out[f"usd_{b}B_steps"] = round(h * price, 1) if h else None
print("E5RESULT " + json.dumps(out))
PY
}

phase_e5b_curve() {
  PY=$(pick_python)
  local cfg; cfg=$(cat "$RES/best_speed_cfg" 2>/dev/null)
  local extra=()
  case $cfg in fixed) extra=(--fixed-shapes);; fixed_overlap) extra=(--fixed-shapes --overlap);; esac
  local mon; mon=$(gpu_monitor_start "$RES/gpu_curve.csv")
  local t; t=$(date +%s)
  # From scratch: the released size (d 128, 6 layers, 4 heads), A0, the recipe's win reward 3, 30% archetype
  # practice fights (no human data: the 2020 metrics dump is not public).
  train e5-scratch "$CURVE_TIMEOUT" --layers 6 --ascension 0 --envs 2048 --steps 128 --updates "$CURVE_UPDATES" \
    --win-reward 3 --practice 0.3 --practice-eval 0 --eval-every 25 --eval-runs 256 --save-every 50 "${extra[@]}"
  local rc=$?
  gpu_monitor_stop "$mon" "$RES/gpu_curve.csv" "E5b-curve" | tee -a "$RES/results.txt"
  "$PY" "$TOOLS/e5_logcsv.py" "$RES/runs/e5-scratch" "E5b-curve" 5 | tee -a "$RES/results.txt"
  result "$(printf '{"exp":"E5b-curve-run","cfg":"%s","rc":%s,"wall_s":%s}' "${cfg:-eager}" "$rc" "$(elapsed "$t")")"
  [ $rc -ne 0 ] && tail -25 "$RES/train_e5-scratch.log"
}

phase_pack() {
  local hours; hours=$("${PY:-python3}" -c "print(round($(elapsed "$T_START")/3600, 2))" 2>/dev/null || echo "?")
  result "$(printf '{"exp":"total","wall_hours":%s,"usd_at_price":"%s"}' "$hours" \
    "$("${PY:-python3}" -c "print(round($(elapsed "$T_START")/3600*$PRICE, 2))" 2>/dev/null)")"
  tar -czf "$WORK/e5_results.tgz" -C "$WORK" results 2>/dev/null && log "packed $WORK/e5_results.tgz"
}

# ---------------------------------------------------------------------------------------------
PY=$(pick_python)
for ph in setup build e5a_wasm e5a_py e5b_speed e5b_curve pack; do
  phase_on "$ph" || continue
  [ "$ph" != setup ] && [ ! -f "$TOOLS/e5_wasm_eval.cjs" ] && write_tools
  log "=== phase $ph"
  t0=$(date +%s)
  "phase_$ph"; rc=$?
  result "$(printf '{"exp":"phase","phase":"%s","rc":%s,"seconds":%s}' "$ph" "$rc" "$(elapsed "$t0")")"
  if [ $rc -ne 0 ]; then
    case $ph in
      setup) log "setup failed: stopping"; phase_pack; exit 1;;
      build) log "build failed: skipping the phases that need the sts module"; PHASES=$(echo " $PHASES " | sed 's/ e5a_py / /; s/ e5b_speed / /; s/ e5b_curve / /');;
    esac
  fi
done
log "done"

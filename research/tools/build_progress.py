#!/usr/bin/env python3
"""Rebuild the progress page from the repository's own records.

Inputs: research/results.tsv (+ commit times), research/best_*_name.txt, research/models/{sv,vr}_*.meta,
research/runs/pooled*_<heart best>.txt, the queue (research/queue.txt / queue.current / queue.log),
the "Current state" section of research/program.md, and the hand-maintained research/progress/plan.json.
Numbers on the page are computed here, never typed. usage: build_progress.py OUT.html
"""
import datetime as dt, html, json, os, re, subprocess, sys, glob

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(R)
T0 = dt.datetime(2026, 10, 31, 20, 42)
now = dt.datetime.now(dt.timezone.utc).replace(tzinfo=None)
esc = html.escape

_times = {}
def ctime(c):
    if c not in _times:
        out = subprocess.run(["git", "show", "-s", "--format=%ci", c], capture_output=True, text=True, cwd=REPO).stdout.strip()
        _times[c] = out[:16] if out else now.strftime("%Y-%m-%d %H:%M")
    return _times[c]
def hours(ts):
    return round((dt.datetime.strptime(ts, "%Y-%m-%d %H:%M") - T0).total_seconds() / 3600, 2)

rows = [l.rstrip("\n").split("\t") for l in open(os.path.join(R, "results.tsv")) if l.strip()]
for wf in sorted(glob.glob(os.path.join(R, "results.d", "*.tsv"))):  # rows logged by parallel worker sessions
    rows += [l.rstrip("\n").split("\t") for l in open(wf) if l.strip()]
HEART_NAMES = lambda n: n.startswith("heart_") or n == "INFO_keys1"
FRIENDLY = {}
tracks = {"a0": [], "a0_test": [], "heart": [], "a10": [], "a20": []}
experiments = []
for i, r in enumerate(rows):
    if len(r) < 11 or i < 4:
        continue
    split, status, desc = r[1], r[9], r[10]
    try:
        win, floor = float(r[4]), float(r[5])
    except ValueError:
        continue
    name = desc.split(":")[0]
    d = desc.split(":", 1)[1].strip() if ":" in desc else ""
    d = re.sub(r"^\[[^\]]*\]\s*", "", d).split(" [")[0][:110]
    h = hours(ctime(r[0]))
    if name == "heart_hm2":
        continue  # informational (budget +3.8%); best reset
    if split == "test.a0" and status in ("MILESTONE", "FINAL"):
        tracks["a0_test"].append(dict(h=h, name=name, win=round(win * 100, 1), floor=floor, desc=d))
    elif status in ("keep", "best"):
        if split == "val.a0" and HEART_NAMES(name): tracks["heart"].append(dict(h=h, name=name, win=round(win * 100, 1), floor=floor, desc=d))
        elif split == "val.a0": tracks["a0"].append(dict(h=h, name=name, win=round(win * 100, 1), floor=floor, desc=d))
        elif split == "val.a10": tracks["a10"].append(dict(h=h, name=name, win=round(win * 100, 1), floor=floor, desc=d))
        elif split == "val.a20": tracks["a20"].append(dict(h=h, name=name, win=round(win * 100, 1), floor=floor, desc=d))
    if split.startswith("val") and status in ("keep", "discard") and r[7] and r[8]:
        tr = "heart" if (split == "val.a0" and HEART_NAMES(name)) else {"val.a0": "a0", "val.a10": "a10", "val.a20": "a20"}.get(split, "a0")
        try:
            experiments.append(dict(h=h, name=name, track=tr, d=float(r[7]), p=float(r[8]), keep=status == "keep", status=status, desc=d))
        except ValueError:
            pass
for k, v in tracks.items():  # commit times can repeat: keep the order monotone
    hm = -1
    for p in v:
        if p["h"] < hm: p["h"] = hm
        hm = p["h"]

# learned-value metas
def metas(prefix, fields):
    out = {}
    for f in glob.glob(os.path.join(R, "models", f"{prefix}_*.meta")):
        mid = os.path.basename(f)[len(prefix) + 1:-5]
        vals = open(f).read().split()
        out[mid] = {k: (float(v) if "." in v else int(v)) for k, v in zip(fields, vals)}
    return out
svmeta = metas("sv", ["rmse", "base", "rows", "hidden", "layers", "params", "leaf"])
vrmeta = metas("vr", ["rmse", "corr", "runs", "hidden", "layers", "params"])
fit = {k: dict(rows=m["rows"], hidden=m["hidden"], params=m["params"], rmse=m["rmse"], flops=6 * m["params"] * m["rows"] * 20)
       for k, m in svmeta.items() if not m["leaf"] and k.isdigit() and int(k) <= 9}

play, secs, scale = {}, {}, {}
for r in rows:
    if len(r) >= 11 and r[9] == "scaling":
        n = r[10].split(":")[0]; m = re.search(r"sims (\d+)", r[10])
        if n.startswith("f39"):  # F39 compute-scaling diagnostic: its own chart (track best at 2x / 4x search), not a leaf-value run
            tm = re.match(r"f39(a10|a20|heart)_s(\d)x", n)
            if tm: scale[n] = dict(track=tm.group(1), mult=int(tm.group(2)), sims=int(m.group(1)) if m else None, win=round(float(r[4]) * 100, 1), dscore=float(r[7]))
            continue
        play[n] = dict(win=round(float(r[4]) * 100, 1), floor=float(r[5]), dscore=float(r[7]), sims=int(m.group(1)) if m else None, split=r[1])
def mean_secs(path):
    v = [float(l.split("\t")[10]) for l in open(path) if l.split("\t")[0].isdigit() and len(l.split("\t")) > 10]
    return round(sum(v) / len(v), 2) if v else None
for n, p in play.items():
    f = os.path.join(R, "runs", f"{n}.{p['split'].split('.')[0]}.a0.tsv")
    if os.path.exists(f): secs[n] = mean_secs(f)
base_secs = mean_secs(os.path.join(R, "runs", "mapd085.val.a0.tsv"))

heart_best = open(os.path.join(R, "best_heart_name.txt")).read().strip()
heart_pooled = 0.0
for cand in (f"pooled600_{heart_best}.txt", f"pooled_{heart_best}.txt"):
    f = os.path.join(R, "runs", cand)
    if os.path.exists(f):
        m = re.search(re.escape(heart_best) + r" win=([0-9.]+)", open(f).read())
        if m: heart_pooled = float(m.group(1)); break

data = dict(tracks=tracks, experiments=experiments, fit=fit, svmeta=svmeta, vrmeta=vrmeta, play=play, secs=secs, scale=scale,
            base_secs=base_secs, heart_pooled=heart_pooled, hours_now=round((now - T0).total_seconds() / 3600, 1), pooled={})

# ---- queue status
def summarize(line):
    line = line.strip()
    pats = [(r'exp(?:_pooled)?\.sh (\S+) "([^"]*)"', lambda m: f"{m.group(1)}: {m.group(2)}"),
            (r'ext_pooled\.sh (\S+)', lambda m: f"{m.group(1)}: pooled near-miss extension"),
            (r'rep_dev\.sh (\S+)', lambda m: f"{m.group(1)}: replication on the dev slice"),
            (r'sv_eval\.sh (\d+) "([^"]*)"', lambda m: f"sv{m.group(1)} play test: {m.group(2)}"),
            (r'train_sv\.py .*--id (\d+) --rows (\d+) --hidden (\d+)', lambda m: f"train leaf model sv{m.group(1)} (h={m.group(3)})"),
            (r'train_vr\.py .*--id (\d+) --runs (\d+) --hidden (\d+)', lambda m: f"train run-value model vr{m.group(1)} ({int(m.group(2)):,} runs, h={m.group(3)})"),
            (r'sv_gen\.sh (\S+) (\d+)', lambda m: f"generate {int(m.group(2)):,} games of logged data ({m.group(1)})"),
            (r'vr_select\.sh', lambda m: "select the lowest-error run-value model"),
            (r'--leaktest', lambda m: "leak test"),
            (r'make -C .*repro_check', lambda m: "guarded rebuild (bit-identity of the three bests)"),
            (r'calibrate\.sh "rollout_turns=0"', lambda m: "Phase S control (heuristic leaf)")]
    for pat, f in pats:
        m = re.search(pat, line)
        if m: return f(m)
    return line[:90]
qfile = os.path.join(R, "queue.txt"); qcur = os.path.join(R, "queue.current"); qlog = os.path.join(R, "queue.log")
queue = [l for l in (open(qfile).read().split("\n") if os.path.exists(qfile) else []) if l.strip() and not l.strip().startswith("#")]
current = open(qcur).read().strip() if os.path.exists(qcur) else ""
started = ""
if os.path.exists(qlog):
    for l in reversed(open(qlog).read().splitlines()):
        m = re.match(r"== (\d\d:\d\d):\d\d START: (.*)", l)
        if m and current and m.group(2).strip() == current.strip(): started = m.group(1); break
progress = ""
m = re.search(r"sv_gen\.sh (\S+) (\d+) (\d+)", current)
if m:
    done = len(glob.glob(os.path.join(R, "vf", "data", m.group(1), "chunk_*", ".done")))
    total = -(-int(m.group(2)) // int(m.group(3)))
    progress = (done, total, "chunks")
else:
    m = re.search(r"run\.py (\S+) (\S+)", current) or re.search(r"exp(?:_pooled)?\.sh (\S+)", current)
    if m:
        files = glob.glob(os.path.join(R, "runs", ".inprogress", m.group(1) + "*"))
        if files:
            g = sum(1 for f in files for l in open(f) if l[:1].isdigit())
            progress = (g, 100 if "val100" in current else 300, "games")

def pill(s): return f'<span class="pill {esc(s)}">{esc(s)}</span>'
recent = [r for r in rows if len(r) >= 11 and r[9] in ("keep", "discard", "scaling", "info") and r[7]][-10:][::-1]
rec_html = "".join(f'<li>{pill(r[9])}<span><b>{esc(r[10].split(":")[0])}</b> · {esc(r[1])} · Δ {esc(r[7])}'
                   f'{(" · P=" + esc(r[8])) if r[8] else ""} · win {float(r[4]) * 100:.1f}%</span></li>' for r in recent)
cur_html = '<div class="now">idle (queue empty)</div>'
if current:
    bar = f'<div class="bar"><i style="width:{100 * progress[0] / max(1, progress[1]):.0f}%"></i></div><div class="when">{progress[0]}/{progress[1]} {progress[2]}</div>' if progress else ""
    cur_html = f'<div class="now"><b>{esc(summarize(current))}</b>{(" · started " + started + " UTC") if started else ""}</div>{bar}'
next_html = "".join(f"<li>{pill('planned')}<span>{esc(summarize(l))}</span></li>" for l in queue[:8])
if len(queue) > 8: next_html += f'<li><span></span><span class="when">… and {len(queue) - 8} more queued steps</span></li>'
state = ""
ps = open(os.path.join(R, "program.md")).read()
m = re.search(r"### Current state[^\n]*\n(- .*?)(?=\n- \*\*M5 test|\n### |\n## )", ps, re.S)
if m:
    state = re.sub(r"\s+", " ", m.group(1).lstrip("- ")).strip()
    state = re.sub(r"\*\*|`", "", state)
status_html = f'''
  <section class="status">
    <div class="card"><h2>Running now</h2>{cur_html}<div class="sub" style="margin-top:8px">{esc(state[:700])}</div></div>
    <div class="card"><h2>Next up in the queue ({len(queue)} steps)</h2><ul>{next_html or "<li><span></span><span>nothing queued</span></li>"}</ul></div>
    <div class="card"><h2>Recently decided</h2><ul>{rec_html}</ul></div>
  </section>'''

# ---- targets (computed) and plan (hand-maintained)
plan = json.load(open(os.path.join(R, "progress", "plan.json")))
cur = {"a0": tracks["a0"][-1]["win"] if tracks["a0"] else 0, "a0_test": tracks["a0_test"][-1]["win"] if tracks["a0_test"] else 0,
       "a10": tracks["a10"][-1]["win"] if tracks["a10"] else 0, "a20": tracks["a20"][-1]["win"] if tracks["a20"] else 0,
       "heart": tracks["heart"][-1]["win"] if tracks["heart"] else 0, "a20heart": 0.0}
tg = []
for t in plan["targets"]:
    v = cur.get(t["track"], 0.0); met = v >= t["target"]; scale = max(t["target"] * 1.25, v, 1)
    tg.append(f'<div class="target"><div class="lbl"><span>{esc(t["label"])}</span><b>{v:.1f}% / {t["target"]}%</b></div>'
              f'<div class="meter"><i class="{"met" if met else ""}" style="width:{100 * v / scale:.1f}%"></i><u style="left:{100 * t["target"] / scale:.1f}%"></u></div>'
              f'<div class="note">{esc(t["note"])}{" · reached" if met else ""}</div></div>')
ms = "".join(f'<li>{pill(x["status"])}<span><b>{esc(x["id"])}</b> {esc(x["label"])}{(" <span class=when>" + esc(x["when"]) + "</span>") if x["when"] else ""}</span></li>' for x in plan["milestones"])
targets_html = f'''
  <section class="card">
    <h2>Targets and milestones</h2>
    <div class="sub">Bars show the current best against each month target (the dark tick). Values come from the kept chains in results.tsv.</div>
    <div class="targets">{"".join(tg)}</div>
    <div class="status" style="margin-top:14px"><div><ul>{ms}</ul></div></div>
  </section>'''
road = "".join(f'<div><b>{esc(p["name"])}</b>{pill(p["status"])}<span>{esc(p["summary"])}</span></div>' for p in plan["phases"])
nxt = "".join(f"<li><span>{i + 1}.</span><span>{esc(t)}</span></li>" for i, t in enumerate(plan["next_up"]))
plan_html = f'''
  <section class="card">
    <h2>Plan</h2>
    <div class="roadmap">{road}</div>
    <div class="status" style="margin-top:14px"><div><h2 style="font-size:15px">Next, in order</h2><ul>{nxt}</ul></div></div>
  </section>'''
notes_html = f'''
  <section class="notes">
    <div class="card"><h2>Done</h2><ul>{"".join("<li>" + esc(t) + "</li>" for t in plan["done"])}</ul></div>
    <div class="card"><h2>Closed with evidence</h2><ul>{"".join("<li>" + esc(t) + "</li>" for t in plan["closed"])}</ul></div>
  </section>'''

tpl = open(os.path.join(R, "progress", "template.html")).read()
out = (tpl.replace("__DATA__", json.dumps(data, ensure_ascii=False))
          .replace("__UPDATED__", now.strftime("%Y-%m-%d %H:%M UTC"))
          .replace("<!--STATUS-->", status_html).replace("<!--TARGETS-->", targets_html)
          .replace("<!--PLAN-->", plan_html).replace("<!--NOTES-->", notes_html))
open(sys.argv[1] if len(sys.argv) > 1 else os.path.join(R, "progress", "index.html"), "w").write(out)
print(f"progress page written: {len(out):,} bytes; {len(experiments)} experiments, {len(play)} leaf runs, {len(vrmeta)} run-value models, queue {len(queue)} steps")

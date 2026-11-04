#!/usr/bin/env python3
"""Refreshes everything the progress dashboard shows, with no game running.

    update_progress.py [--bridge PATH]

It makes recordings from any new transcripts, replays them all, counts what they cover, and writes one CSV per chart to
reports/dashboard/. reports/regression_history.tsv keeps one row per pass. Use the build with the most cards in it.
Exits 1 if the regression test found anything worse, after writing everything else.
"""
import argparse
import csv
import datetime
import json
import os
import re
import subprocess
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)  # python3 -I leaves the script's folder off the path
import coverage as cov  # noqa: E402
from run_regression import kind_of  # noqa: E402

HERE = os.path.dirname(TOOLS)
REPORTS = os.path.join(HERE, "reports")
OUT = os.path.join(REPORTS, "dashboard")
RESULTS = os.path.join(HERE, "logs", "replay_results.jsonl")
HISTORY = os.path.join(REPORTS, "regression_history.tsv")
# Other classes' batches keep their mirror logs and plans in their own checkouts, while all transcripts land here.
CLASS_REPOS = [os.path.expanduser(f"~/Projects/{name}/research/realgame") for name in ("sts_bridge_cards", "sts_bridge_silent", "sts_bridge_watcher")]
# A build of its own, since the one a real-game batch runs from can't be rebuilt while it runs.
DEFAULT_BRIDGE = os.path.expanduser("~/Projects/sts_lightspeed_watcher/build-replay/sts_bridge")
HOURS_BACK = 24
PASS_COLUMNS = ["time", "commit", "total", "change_24h", "weakest", "weakest_left", "runs", "recordings", "pass", "limited", "fail", "worse", "seconds", "checked_share",
                "points_per_hour", "hours_to_100", "runs_left", "hours_left"]

CATEGORY_LABELS = {"cards": "卡牌", "relics": "遗物", "potions": "药水", "monsters": "怪物", "events": "事件", "powers": "能力",
                   "ascensions": "进阶等级", "class_runs": "各职业完整局"}
CLASS_LABELS = {"ironclad": "铁甲战士", "silent": "静默猎手", "defect": "故障机器人", "watcher": "观者", "any": "通用", "colorless": "无色", "curse": "诅咒", "status": "状态"}
# What each class still needs, in order, for the classes table.
CLASS_NEXT = {
    "ironclad": "继续跑新种子，直到 300 局无差异",
    "silent": "卡牌已移植并在铁甲局里跑过 26 个种子。还差用该职业开局",
    "defect": "卡牌已移植并在铁甲局里验证。还差用该职业开局",
    "watcher": "卡牌已移植，正在铁甲局里验证。还差用该职业开局",
}
FIELD_LABELS = {"plays": "打出", "upgraded": "升级后打出", "signatures": "情境", "in_hand": "在手牌回合", "runs": "局数", "uses": "使用", "fights": "战斗",
                "visits": "遇到", "options_chosen": "选过的选项比例", "runs_to_floor": "到 17 层以上的局", "clean_distinct_seeds": "无差异的不同种子"}


def write_csv(name: str, columns: list[str], rows: list[dict]) -> None:
    with open(os.path.join(OUT, name), "w", newline="") as f:
        writer = csv.DictWriter(f, columns)
        writer.writeheader()
        writer.writerows(rows)


def run_step(command: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(command, capture_output=True, text=True, check=False)


def regression(bridge: str) -> dict:
    tool = os.path.join(TOOLS, "run_regression.py")
    done = run_step([sys.executable, "-I", tool, "--skip_export", "--bridge", bridge, "--results", RESULTS])
    rows = [json.loads(line) for line in open(RESULTS)]
    seconds = re.search(r"in ([\d.]+)s", done.stdout)
    kinds = [kind_of(r) for r in rows]
    print(done.stdout.strip())
    return {"exit": done.returncode, "recordings": len(rows), "pass": kinds.count("pass"), "limited": kinds.count("limited"), "fail": kinds.count("fail"),
            "checked": sum(r.get("commands_replayed", 0) for r in rows), "total": sum(r.get("commands_total", 0) for r in rows),
            "seconds": round(float(seconds.group(1))) if seconds else 0, "worse": done.stdout.count("WORSE (")}


def commit_of(bridge: str) -> str:
    source = os.path.dirname(os.path.dirname(bridge))
    done = run_step(["git", "-C", source, "rev-parse", "--short", "HEAD"])
    return done.stdout.strip() or "?"


def append_history(row: dict) -> list[dict]:
    """Adds this pass to the log and returns the log, newest first, so the page's first row is the latest."""
    exists = os.path.exists(HISTORY)
    with open(HISTORY, "a", newline="") as f:
        writer = csv.DictWriter(f, PASS_COLUMNS, delimiter="\t")
        if not exists:
            writer.writeheader()
        writer.writerow(row)
    with open(HISTORY, newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))[::-1]


def score_history() -> list[dict]:
    with open(os.path.join(REPORTS, "coverage_history.tsv"), newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def change_since(history: list[dict]) -> float:
    latest = datetime.datetime.fromisoformat(history[-1]["time"])
    cutoff = latest - datetime.timedelta(hours=HOURS_BACK)
    earlier = [h for h in history if datetime.datetime.fromisoformat(h["time"]) <= cutoff]
    return round(float(history[-1]["total"]) - float((earlier[-1] if earlier else history[0])["total"]), 1)


def item_rows(items: dict) -> list[dict]:
    rows = []
    for category, entries in items.items():
        for name, fields in entries.items():
            detail = "，".join(f"{label} {fields[key]}" for key, label in FIELD_LABELS.items() if key in fields)
            rows.append({"id": f"{category}/{name}", "category": CATEGORY_LABELS[category], "item": name, "class": CLASS_LABELS.get(fields.get("class", ""), ""),
                         "detail": detail, "score": round(fields["score"] * 100)})
    return sorted(rows, key=lambda r: (r["score"], r["id"]))


def target_rows() -> list[dict]:
    rules = {
        "cards": (f"打出 {cov.CARD_PLAYS} 次，升级后打出 {cov.CARD_UPGRADED} 次，{cov.CARD_SIGNATURES} 种不同情境。诅咒和状态牌：在手牌里 {cov.UNPLAYABLE_IN_HAND} 回合",
                  "牌的 bug 常要配合才出现：力量、虚弱、易伤、多个敌人、精英、Boss、高进阶，这些组合就是情境"),
        "relics": (f"持有过的局 {cov.RELIC_RUNS} 局", "遗物的效果多在持有期间反复触发"),
        "potions": (f"使用 {cov.POTION_USES} 次", "药水效果比较简单，次数要求低"),
        "monsters": (f"进阶 17 以下和以上各打 {cov.MONSTER_FIGHTS} 场", f"怪物的多数加强规则到进阶 {cov.HIGH_ASCENSION} 已经生效"),
        "events": (f"遇到 {cov.EVENT_VISITS} 次，并且每个选项都选过", "事件的 bug 通常只在某个选项里"),
        "powers": (f"出现过的局 {cov.POWER_RUNS} 局", "能力大多跟着牌和怪物一起被测到"),
        "ascensions": (f"进阶 0 到 20 每级各有 {cov.ASCENSION_RUNS} 局到第 {cov.ASCENSION_FLOOR} 层以上", "每一级进阶都改变规则"),
        "class_runs": (f"每个职业 {cov.CLASS_RUNS} 局无差异，最终目标 {cov.CLASS_RUNS_STRETCH} 局",
                       "三分之三法则：连续 n 局没有差异，单局出差异的概率低于 3/n（95% 置信）。60 局对应 5%，300 局对应 1%"),
    }
    return [{"category": CATEGORY_LABELS[c], "points": cov.POINTS[c], "rule": rules[c][0], "why": rules[c][1]} for c in cov.POINTS]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bridge", default=os.environ.get("STS_BRIDGE", DEFAULT_BRIDGE))
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    now = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M")

    sources = []
    for repo in CLASS_REPOS:
        sources += ["--mirror_logs", os.path.join(repo, "logs"), "--plans", os.path.join(repo, "plans")]
    run_step([sys.executable, "-I", os.path.join(TOOLS, "export_traces.py"), *sources])
    reg = regression(args.bridge)
    done = run_step([sys.executable, "-I", os.path.join(TOOLS, "coverage.py"), "--replay_results", RESULTS, "--estimate"])
    if done.returncode != 0:
        print(done.stdout + done.stderr)
        return 2
    print("\n".join(line for line in done.stdout.splitlines() if line.startswith(("Total", "  slowest", "  score"))))

    report = json.load(open(os.path.join(REPORTS, "coverage.json")))
    history = score_history()
    guess = report["estimate"]
    categories = report["categories"]
    weakest = max(categories, key=lambda c: categories[c]["points"] - categories[c]["earned"])
    passes = append_history({"time": now, "commit": commit_of(args.bridge), "total": round(report["total"], 1), "change_24h": change_since(history), "weakest": CATEGORY_LABELS[weakest],
                             "weakest_left": round(categories[weakest]["points"] - categories[weakest]["earned"], 1), "runs": report["runs"], "recordings": reg["recordings"], "pass": reg["pass"],
                             "limited": reg["limited"], "fail": reg["fail"], "worse": reg["worse"], "seconds": reg["seconds"], "checked_share": round(100 * reg["checked"] / max(1, reg["total"])),
                             "points_per_hour": guess["points_per_hour"], "hours_to_100": guess["hours_to_100"], "runs_left": guess["runs_left"], "hours_left": guess["hours_left"]})

    write_csv("history.csv", ["time", "runs", "total", *cov.POINTS], history)
    write_csv("categories.csv", ["key", "category", "earned", "points", "share", "items", "at_zero"],
              [{"key": c, "category": CATEGORY_LABELS[c], "earned": v["earned"], "points": v["points"], "share": round(v["score"] * 100), "items": v["items"], "at_zero": v["at_zero"]}
               for c, v in categories.items()])
    write_csv("classes.csv", ["class", "cards_played", "cards", "cards_score", "relics_seen", "relics", "clean_seeds", "target", "stretch", "next"],
              [{"class": CLASS_LABELS[c], "cards_played": v["cards_played"], "cards": v["cards"], "cards_score": round(v["cards_score"] * 100), "relics_seen": v["relics_seen"],
                "relics": v["relics"], "clean_seeds": v["clean_distinct_seeds"], "target": cov.CLASS_RUNS, "stretch": cov.CLASS_RUNS_STRETCH, "next": CLASS_NEXT[c]}
               for c, v in report["classes"].items()])
    write_csv("items.csv", ["id", "category", "item", "class", "detail", "score"], item_rows(report["items"]))
    write_csv("targets.csv", ["category", "points", "rule", "why"], target_rows())
    write_csv("passes.csv", PASS_COLUMNS, passes)
    print(f"wrote {OUT}")
    return 1 if reg["exit"] else 0


if __name__ == "__main__":
    sys.exit(main())

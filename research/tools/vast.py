#!/usr/bin/env python3
"""Minimal vast.ai controller for the SOTA plan's GPU budget (owner, 2026-11-16: USD 20 to start).

The API key is a network secret injected by the session proxy for console.vast.ai; this script never reads or
prints it. No SSH: a job is a self-contained bash script run as the container's command (runtype "args"), its
stdout is read through the logs API, and the container exits when the script ends (GPU billing stops; the
stopped instance is then destroyed by `destroy`).

Budget guard (owner, 2026-11-20): only OUR spend counts against CAP: the itemised charges (`/charges/`, daily) of
instances whose label starts with "sota", from BASELINE_DAY on. Other users of this shared account (e.g. labels
"deflake-bundle", "rtm-pr66146-e2e") are reported but not counted. `create`/`rent` refuse if that spend + (offer price
+ running sota instances) x requested hours would exceed CAP. Account-wide spend since BASELINE_SPEND is still shown.

usage:
  vast.py offers [GPU_NAME]                 cheapest verified single-GPU on-demand offers
  vast.py spend                             spend since the baseline, running instances and their cost per hour
  vast.py create OFFER_ID SCRIPT HOURS [DISK_GB] [LABEL]   rent and run SCRIPT (bash) in the default image
  vast.py status ID | logs ID [TAIL] | destroy ID
"""
import json, sys, time, urllib.error, urllib.request

API = "https://console.vast.ai/api/v0"
BASELINE_SPEND = 53.2921   # lifetime spend before any rental under this budget (read 2026-11-16 17:3x UTC)
BASELINE_DAY = 1794787200    # 2026-11-16 00:00 UTC; no sota-* charge exists before it
CAP = 20.0
IMAGE = "pytorch/pytorch:2.5.1-cuda12.4-cudnn9-devel"


def call(method, path, body=None, query=None):
    url = (API + path).replace("/api/v0/../v1/", "/api/v1/")
    if query:
        url += "?" + urllib.parse.urlencode(query)
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method, headers={"Content-Type": "application/json", "Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=60) as r:
        txt = r.read().decode()
    return json.loads(txt) if txt.strip() else {}


import urllib.parse  # noqa: E402


def charges_by_label():
    q = {"select_filters": json.dumps({"day": {"gte": BASELINE_DAY, "lte": int(time.time()) + 86400}}), "limit": 500}
    by = {}
    while True:
        d = call("GET", "/charges/", query=q)
        for r in d.get("results") or []:
            lab = str((r.get("metadata") or {}).get("label"))
            by[lab] = by.get(lab, 0.0) + float(r.get("amount", 0.0))
        if not d.get("next_token"):
            return by
        q["after_token"] = d["next_token"]


def spend():
    """(our sota-* spend since BASELINE_DAY, credit, instances, account-wide spend since BASELINE_SPEND, charges by label)"""
    u = call("GET", "/users/current/")
    acct = -float(u.get("total_spend", 0.0)) - BASELINE_SPEND
    by = charges_by_label()
    s = sum(v for k, v in by.items() if k.startswith("sota"))
    inst = []
    params = {"select_filters": json.dumps({}), "order_by": json.dumps([{"col": "id", "dir": "asc"}]), "limit": 25}
    while True:   # the v0 list endpoint is gone (HTTP 410); v1 pages with next_token
        d = call("GET", "/../v1/instances/", query=params)
        inst += d.get("instances") or []
        if not d.get("next_token"):
            break
        params["after_token"] = d["next_token"]
    return s, float(u.get("credit", 0.0)), inst, acct, by


def offer_list(gpu=None, limit=40):
    q = {"rentable": {"eq": True}, "verified": {"eq": True}, "num_gpus": {"eq": 1}, "reliability2": {"gt": 0.98},
         "inet_down": {"gt": 200}, "cpu_cores_effective": {"gt": 15}, "disk_space": {"gt": 60},
         "order": [["dph_total", "asc"]], "type": "on-demand", "limit": limit}
    if gpu:
        q["gpu_name"] = {"eq": gpu}
    return call("GET", "/bundles/", query={"q": json.dumps(q)}).get("offers", [])


def offers(gpu=None):
    o = offer_list(gpu)
    for x in o[:15]:
        print(f"{x['id']}\t{x['gpu_name']}\t${x['dph_total']:.3f}/h\tcpu={x.get('cpu_cores_effective')}\tram={round(x.get('cpu_ram', 0) / 1024)}GB"
              f"\tdisk={round(x.get('disk_space', 0))}GB\tcuda={x.get('cuda_max_good')}\trel={x.get('reliability2'):.3f}\t{x.get('geolocation')}")


def main():
    a = sys.argv[1:]
    if not a or a[0] in ("-h", "--help"):
        print(__doc__); return
    cmd = a[0]
    if cmd == "offers":
        offers(a[1] if len(a) > 1 else None)
    elif cmd == "spend":
        s, credit, inst, acct, by = spend()
        other = {k: round(v, 2) for k, v in by.items() if not k.startswith("sota")}
        print(f"sota spend ${s:.2f} of cap ${CAP:.2f}; credit ${credit:.2f}; instances {len(inst)};"
              f" account-wide since baseline ${acct:.2f} (not ours: {other})")
        for i in inst:
            print(f"  id={i['id']} {i.get('gpu_name')} status={i.get('actual_status')} ${i.get('dph_total', 0):.3f}/h label={i.get('label')}"
                  f" up={round((time.time() - (i.get('start_date') or time.time())) / 3600, 2)}h")
    elif cmd == "create":
        offer, script, hours = int(a[1]), a[2], float(a[3])
        disk = int(a[4]) if len(a) > 4 else 60
        label = a[5] if len(a) > 5 else "sota-e5"
        s, _, inst, _, _ = spend()
        o = [x for x in offer_list(None, 1000) if int(x["id"]) == offer]   # the same query as `offers` (this endpoint applies hidden defaults)
        if not o:
            sys.exit(f"offer {offer} not available")   # (a lookup by id alone returns nothing on this endpoint)
        price = float(o[0]["dph_total"])
        running = sum(float(i.get("dph_total", 0)) for i in inst if str(i.get("label")).startswith("sota") and i.get("actual_status") not in ("exited", "stopped"))
        if s + (price + running) * hours > CAP:
            sys.exit(f"REFUSED by budget guard: spend ${s:.2f} + (${price:.3f} + running ${running:.3f})/h x {hours}h > cap ${CAP}")
        body = {"client_id": "me", "image": IMAGE, "env": {}, "disk": disk, "label": label, "runtype": "args",
                "onstart": "bash", "args": ["-c", open(script).read()]}
        r = call("PUT", f"/asks/{offer}/", body)
        r.pop("instance_api_key", None)
        print(json.dumps(r))
    elif cmd == "rent":   # rent GPU SCRIPT HOURS [DISK] [LABEL] [MIN_CPUS]: cheapest current offer outside CN/VN, retry the next on failure
        gpu, script, hours = a[1], a[2], float(a[3])
        disk = int(a[4]) if len(a) > 4 else 60
        label = a[5] if len(a) > 5 else "sota-e5"
        mincpu = float(a[6]) if len(a) > 6 else 24
        s, _, inst, _, _ = spend()
        running = sum(float(i.get("dph_total", 0)) for i in inst if str(i.get("label")).startswith("sota") and i.get("actual_status") not in ("exited", "stopped"))
        import os
        excl = {int(v) for v in os.environ.get("VAST_EXCLUDE", "").replace(",", " ").split()}   # offers/hosts that failed before
        cands = [x for x in offer_list(gpu, 60) if x.get("cpu_cores_effective", 0) >= mincpu and int(x["id"]) not in excl
                 and int(x.get("machine_id", -1)) not in excl
                 and not any(t in str(x.get("geolocation")) for t in ("CN", "China", "VN", "Vietnam"))]
        body = {"client_id": "me", "image": IMAGE, "env": {}, "disk": disk, "label": label, "runtype": "args",
                "onstart": "bash", "args": ["-c", open(script).read()]}
        for x in cands[:6]:
            price = float(x["dph_total"])
            if s + (price + running) * hours > CAP:
                sys.exit(f"REFUSED by budget guard: spend ${s:.2f} + (${price:.3f} + running ${running:.3f})/h x {hours}h > cap ${CAP}")
            try:
                r = call("PUT", f"/asks/{x['id']}/", body)
            except urllib.error.HTTPError as e:
                print(f"offer {x['id']} failed: {e.code} {e.read().decode()[:200]}"); continue
            r.pop("instance_api_key", None)  # never print or log the instance-scoped key
            print(f"rented offer {x['id']} {x['gpu_name']} ${price:.3f}/h cpu={x.get('cpu_cores_effective')} {x.get('geolocation')}: {json.dumps(r)}")
            return
        sys.exit("no offer could be rented")
    elif cmd == "status":
        i = call("GET", f"/instances/{int(a[1])}/", query={"owner": "me"})
        i = i.get("instances", i)
        print(json.dumps({k: i.get(k) for k in ("id", "actual_status", "cur_state", "status_msg", "gpu_name", "dph_total", "start_date", "label")}, default=str))
    elif cmd == "logs":
        tail = a[2] if len(a) > 2 else "200"
        r = call("PUT", f"/instances/request_logs/{int(a[1])}/", {"tail": tail})
        url = r.get("result_url")
        if not url:
            print(json.dumps(r)); return
        for _ in range(40):
            try:
                with urllib.request.urlopen(url, timeout=20) as f:
                    sys.stdout.write(f.read().decode(errors="replace")); return
            except Exception:
                time.sleep(1.5)
        sys.exit("logs not ready")
    elif cmd == "destroy":
        print(json.dumps(call("DELETE", f"/instances/{int(a[1])}/", {})))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()

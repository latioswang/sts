#!/usr/bin/env python3
"""Move generated training data through a private Hugging Face dataset repo (git carries code, models and results;
this carries the GB-scale data under research/vf/data/<NAME>/all/, which is gitignored).

Config: repo id from env STS_DATA_REPO, else research/data_store.json {"repo_id": ...}. Token: env HF_TOKEN, a
prior `hf auth login`, or none in the VM when a cloud network secret adds it in transit to huggingface.co. Never
commit or print the token. Network hosts and Xet notes: research/data_store.json.

usage:
  data_sync.py push NAME [--dry-run]  upload research/vf/data/NAME/all/* (symlinks resolved) to NAME/all/<shard>/;
                                      files already in the repo are skipped, so an interrupted push resumes
  data_sync.py pull NAME [NAME ...]   download into research/vf/data/NAME/_hf/ and link every file into NAME/all/
  data_sync.py ls                     data sets in the repo with file counts
  data_sync.py selftest [--hosts]     who am I, repo private?, upload 3 random files (41 MB), download, compare
                                      sha256, delete them; --hosts also prints every host name the client contacted,
                                      including hf_xet's (Rust) traffic, via an in-process logging proxy
Files are sharded into 256 subfolders (first 2 hex digits of sha1(file name)) to stay well under the Hub's
per-folder file-count guidance.
"""
import glob, hashlib, json, os, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # research/
DATA = os.path.join(ROOT, "vf", "data")
BATCH_FILES = int(os.environ.get("STS_PUSH_BATCH", 500))   # fewer, larger commits: the Hub rate-limits commits (429)
BATCH_BYTES = 2 << 30


def repo_id():
    r = os.environ.get("STS_DATA_REPO")
    if not r:
        p = os.path.join(ROOT, "data_store.json")
        r = json.load(open(p)).get("repo_id") if os.path.exists(p) else None
    if not r:
        sys.exit("no data repo: set STS_DATA_REPO or research/data_store.json {\"repo_id\": \"user/name\"}")
    return r


def shard(base):
    return hashlib.sha1(base.encode()).hexdigest()[:2]


def remote_path(name, base):
    return f"{name}/all/{shard(base)}/{base}"


def push(name, data=DATA, dry=False, api=None, repo=None):
    files = sorted(glob.glob(os.path.join(data, name, "all", "*")))
    if not files:
        sys.exit(f"push {name}: no files under {os.path.join(data, name, 'all')}")
    plan = [(remote_path(name, os.path.basename(f)), os.path.realpath(f)) for f in files]
    total = sum(os.path.getsize(p) for _, p in plan)
    if dry:
        print(f"push {name} (dry run): {len(plan)} files, {total / 1e6:.1f} MB, e.g. {plan[0][0]}")
        return
    from huggingface_hub import HfApi, CommitOperationAdd
    from huggingface_hub.errors import HfHubHTTPError
    api = api or HfApi(); repo = repo or repo_id()
    have = {f for f in api.list_repo_files(repo, repo_type="dataset") if f.startswith(name + "/all/")}
    todo = [(r, p) for r, p in plan if r not in have]
    print(f"push {name}: {len(plan)} files, {len(plan) - len(todo)} already in {repo}, {len(todo)} to upload", flush=True)
    batch, size, done = [], 0, 0
    def flush_batch():
        nonlocal batch, size, done
        if not batch:
            return
        left = batch
        for attempt in range(8):
            try:
                api.create_commit(repo, [CommitOperationAdd(path_in_repo=r, path_or_fileobj=p) for r, p in left],
                                  commit_message=f"{name}: {len(left)} files", repo_type="dataset")
                break
            except HfHubHTTPError as e:
                # A 5xx can arrive after the commit landed (seen: 502 from /commit, files there), so never blindly
                # resend: re-list, and commit only what is still missing. Re-uploaded bytes dedupe on Xet.
                code = getattr(getattr(e, "response", None), "status_code", None) or 0
                if not (code >= 500 or code == 429) or attempt == 7:
                    raise
                wait = 600 if code == 429 else min(15 << attempt, 600)   # 429 = commit rate limit: back off for minutes
                print(f"push {name}: commit got HTTP {code}; retry {attempt + 1}/7 in {wait}s", flush=True)
                time.sleep(wait)
                have_now = set(api.list_repo_files(repo, repo_type="dataset"))
                left = [(r, p) for r, p in left if r not in have_now]
                if not left:
                    break
        done += len(batch)
        print(f"push {name}: {done}/{len(todo)} files uploaded", flush=True)
        batch, size = [], 0
    for r, p in todo:
        batch.append((r, p)); size += os.path.getsize(p)
        if len(batch) >= BATCH_FILES or size >= BATCH_BYTES:
            flush_batch()
    flush_batch()
    print(f"PUSH {name} DONE: {len(plan)} files in {repo}")


def pull(names, data=DATA, repo=None):
    from huggingface_hub import snapshot_download
    repo = repo or repo_id()
    for name in names:
        stage = os.path.join(data, name, "_hf")
        # 2026-11-06: list and fetch only this set (the store holds >10^5 files; snapshot_download walked all of them and hit
        # rate limits / disconnects). Same files, same layout under stage/ as before.
        from huggingface_hub import hf_hub_download
        for f in set_files(name, repo):
            for attempt in range(6):
                try:
                    hf_hub_download(repo, f, repo_type="dataset", local_dir=stage); break
                except Exception as e:
                    if attempt == 5: raise
                    print(f"retry {f}: {type(e).__name__}", flush=True); time.sleep(10 * (attempt + 1))
        out = os.path.join(data, name, "all"); os.makedirs(out, exist_ok=True)
        got = glob.glob(os.path.join(stage, name, "all", "*", "*"))
        for f in got:
            dst = os.path.join(out, os.path.basename(f))
            if not os.path.lexists(dst):
                os.symlink(os.path.realpath(f), dst)
        print(f"PULL {name} DONE: {len(got)} files linked into {out}")


def set_files(name, repo):
    """Files of one data set, listed from its own folder only (no full-store walk)."""
    from huggingface_hub import HfApi
    for attempt in range(6):
        try:
            return sorted(e.path for e in HfApi().list_repo_tree(repo, path_in_repo=f"{name}/all", recursive=True,
                                                                repo_type="dataset") if not hasattr(e, "tree_id"))
        except Exception as e:
            if type(e).__name__ in ("EntryNotFoundError", "RepositoryNotFoundError") or "404" in str(e): return []
            if attempt == 5: raise
            time.sleep(10 * (attempt + 1))


def has(name, repo=None):
    """Exit 0 and print '<name>\t<n> files' if the set exists in the store, else exit 1 (cheap: one folder listing)."""
    repo = repo or repo_id(); fs = set_files(name, repo)
    if not fs: sys.exit(1)
    print(f"{name}\t{len(fs)} files")


def ls(repo=None):
    from huggingface_hub import HfApi
    repo = repo or repo_id(); counts = {}
    for f in HfApi().list_repo_files(repo, repo_type="dataset"):
        if "/all/" in f:
            counts[f.split("/")[0]] = counts.get(f.split("/")[0], 0) + 1
    for k in sorted(counts):
        print(f"{k}\t{counts[k]} files")
    print(f"{len(counts)} data sets in {repo}")


def tunnel_log():
    """Start an in-process HTTPS proxy that records the host of every CONNECT tunnel, and point this process at it.

    Logging alone misses hosts: huggingface_hub 2.x logs through `httpx2`/`httpcore2`, and the Xet backend
    (`hf_xet`, Rust) does its uploads and downloads outside Python logging entirely. Both clients honour
    HTTPS_PROXY, so every https connection either makes passes through here, whatever the language. An
    HTTPS_PROXY that is already set (e.g. a cloud egress proxy) is chained to, so the real network path is
    unchanged; TLS stays end to end because only the tunnel is relayed. Must run before huggingface_hub is
    imported. Hosts listed in NO_PROXY bypass all proxies and are not seen."""
    import socket, threading, urllib.parse
    up = os.environ.get("HTTPS_PROXY") or os.environ.get("https_proxy")
    upstream = None
    if up:
        u = urllib.parse.urlsplit(up if "://" in up else "http://" + up)
        upstream = (u.hostname, u.port or 80)
    seen = set()
    srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0)); srv.listen(128)

    def pipe(src, dst):   # copy one direction; pass a clean EOF on as a half-close, tear down on errors
        socks, how = (dst,), socket.SHUT_WR
        try:
            while d := src.recv(1 << 16):
                dst.sendall(d)
        except OSError:
            socks, how = (src, dst), socket.SHUT_RDWR
        for sk in socks:
            try:
                sk.shutdown(how)
            except OSError:
                pass

    def handle(c):
        try:
            head = b""
            while b"\r\n\r\n" not in head:
                d = c.recv(4096)
                if not d:
                    return c.close()
                head += d
            method, target = head.split(b"\r\n", 1)[0].decode("latin-1").split()[:2]
            if method != "CONNECT":
                c.sendall(b"HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n"); return c.close()
            host, port = target.rsplit(":", 1); host = host.strip("[]")
            seen.add(host)
            if upstream:
                s = socket.create_connection(upstream); s.sendall(head)   # upstream answers the CONNECT itself
            else:
                s = socket.create_connection((host, int(port)))
                c.sendall(b"HTTP/1.1 200 Connection Established\r\n\r\n")
                rest = head.split(b"\r\n\r\n", 1)[1]
                if rest:
                    s.sendall(rest)
            back = threading.Thread(target=pipe, args=(s, c), daemon=True); back.start()
            pipe(c, s); back.join(); s.close(); c.close()
        except Exception as e:   # a failed tunnel surfaces as a client-side error; keep the host recorded
            print(f"tunnel_log: {e!r}", file=sys.stderr)
            try:
                c.close()
            except OSError:
                pass

    def serve():
        while True:
            c, _ = srv.accept()
            threading.Thread(target=handle, args=(c,), daemon=True).start()

    threading.Thread(target=serve, daemon=True).start()
    os.environ["HTTPS_PROXY"] = os.environ["https_proxy"] = f"http://127.0.0.1:{srv.getsockname()[1]}"
    return seen


def selftest(hosts=False):
    import logging, re
    logged, tunnels = set(), set()
    if hosts:
        tunnels = tunnel_log()
        class H(logging.Handler):
            def emit(self, rec):
                logged.update(re.findall(r"https?://([^/:\s'\"]+)", rec.getMessage()))
        for lg in ("httpx2", "httpcore2", "httpx", "httpcore", "urllib3", "huggingface_hub"):
            l = logging.getLogger(lg); l.setLevel(logging.DEBUG); l.addHandler(H())
    import importlib.util
    from huggingface_hub import HfApi, __version__, constants
    from huggingface_hub.errors import LocalTokenNotFoundError
    from huggingface_hub.utils import get_session, hf_raise_for_status
    api, repo = HfApi(), repo_id()
    xet = not constants.HF_HUB_DISABLE_XET and importlib.util.find_spec("hf_xet") is not None
    print(f"huggingface_hub {__version__}, xet transfers: "
          + ("on (hf_xet)" if xet else "off (HF_HUB_DISABLE_XET set or hf_xet missing)"))
    try:
        print(f"token user: {api.whoami()['name']}")
    except LocalTokenNotFoundError:
        # No token in this VM. A cloud environment's network secret adds it in transit to huggingface.co requests,
        # which whoami() refuses to try; every other call here just sends the request and lets the proxy add it.
        r = get_session().get(f"{constants.ENDPOINT}/api/whoami-v2")
        if r.status_code == 401:
            sys.exit("SELFTEST FAILED: no token reached huggingface.co. Set HF_TOKEN, run `hf auth login`, or (cloud) "
                     "add a network secret for host huggingface.co, header Authorization, prefix Bearer.")
        hf_raise_for_status(r)
        print(f"token user: {r.json()['name']} (no token in this VM; added in transit, e.g. by a network secret)")
    info = api.repo_info(repo, repo_type="dataset")
    print(f"repo {repo}: private={info.private}")
    if not info.private:
        print("WARNING: the data repo is public; make it private on huggingface.co")

    def clean(msg):   # push() skips names already in the repo, so leftovers from a crashed run must go first
        if any(f.startswith("_selftest/") for f in api.list_repo_files(repo, repo_type="dataset")):
            api.delete_folder("_selftest", repo, repo_type="dataset", commit_message=msg)
    clean("selftest: remove leftovers of an interrupted run")
    try:
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            os.makedirs(os.path.join(a, "_selftest", "all"))
            sums = {}
            for i, mb in enumerate((1, 8, 32)):
                blob = os.urandom(mb << 20); fn = f"run_selftest{i}.bin"
                open(os.path.join(a, "_selftest", "all", fn), "wb").write(blob)
                sums[fn] = hashlib.sha256(blob).hexdigest()
            push("_selftest", data=a, api=api, repo=repo)
            pull(["_selftest"], data=b, repo=repo)
            ok = all(hashlib.sha256(open(os.path.join(b, "_selftest", "all", fn), "rb").read()).hexdigest() == s
                     for fn, s in sums.items())
    finally:
        clean("selftest cleanup")
    if hosts:   # tunnels are what was actually opened; logs also name hosts that only appear in response headers
        print("hosts contacted: " + " ".join(sorted(tunnels)))
        print("  named in Python HTTP logs (not necessarily contacted): " + " ".join(sorted(logged)))
    print("SELFTEST OK" if ok else "SELFTEST FAILED: downloaded bytes differ")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a or a[0] not in ("push", "pull", "ls", "has", "selftest"):
        sys.exit(__doc__)
    if a[0] == "push":
        push(a[1], dry="--dry-run" in a)
    elif a[0] == "pull":
        pull(a[1:])
    elif a[0] == "ls":
        ls()
    elif a[0] == "has":
        has(a[1])
    else:
        selftest(hosts="--hosts" in a)

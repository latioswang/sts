#!/usr/bin/env python3
"""Move generated training data through a private Hugging Face dataset repo (git carries code, models and results;
this carries the GB-scale data under research/vf/data/<NAME>/all/, which is gitignored).

Config: repo id from env STS_DATA_REPO, else research/data_store.json {"repo_id": ...}. Token: env HF_TOKEN (or a
prior `hf auth login`). Never commit or print the token.

usage:
  data_sync.py push NAME [--dry-run]  upload research/vf/data/NAME/all/* (symlinks resolved) to NAME/all/<shard>/;
                                      files already in the repo are skipped, so an interrupted push resumes
  data_sync.py pull NAME [NAME ...]   download into research/vf/data/NAME/_hf/ and link every file into NAME/all/
  data_sync.py ls                     data sets in the repo with file counts
  data_sync.py selftest [--hosts]     who am I, repo private?, upload 3 random files (41 MB), download, compare
                                      sha256, delete them; --hosts also prints every host name the client contacted
Files are sharded into 256 subfolders (first 2 hex digits of sha1(file name)) to stay well under the Hub's
per-folder file-count guidance.
"""
import glob, hashlib, json, os, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # research/
DATA = os.path.join(ROOT, "vf", "data")
BATCH_FILES, BATCH_BYTES = 500, 2 << 30


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
    api = api or HfApi(); repo = repo or repo_id()
    have = {f for f in api.list_repo_files(repo, repo_type="dataset") if f.startswith(name + "/all/")}
    todo = [(r, p) for r, p in plan if r not in have]
    print(f"push {name}: {len(plan)} files, {len(plan) - len(todo)} already in {repo}, {len(todo)} to upload", flush=True)
    batch, size, done = [], 0, 0
    def flush_batch():
        nonlocal batch, size, done
        if not batch:
            return
        api.create_commit(repo, [CommitOperationAdd(path_in_repo=r, path_or_fileobj=p) for r, p in batch],
                          commit_message=f"{name}: {len(batch)} files", repo_type="dataset")
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
        snapshot_download(repo, repo_type="dataset", local_dir=stage, allow_patterns=[f"{name}/all/*/*"])
        out = os.path.join(data, name, "all"); os.makedirs(out, exist_ok=True)
        got = glob.glob(os.path.join(stage, name, "all", "*", "*"))
        for f in got:
            dst = os.path.join(out, os.path.basename(f))
            if not os.path.lexists(dst):
                os.symlink(os.path.realpath(f), dst)
        print(f"PULL {name} DONE: {len(got)} files linked into {out}")


def ls(repo=None):
    from huggingface_hub import HfApi
    repo = repo or repo_id(); counts = {}
    for f in HfApi().list_repo_files(repo, repo_type="dataset"):
        if "/all/" in f:
            counts[f.split("/")[0]] = counts.get(f.split("/")[0], 0) + 1
    for k in sorted(counts):
        print(f"{k}\t{counts[k]} files")
    print(f"{len(counts)} data sets in {repo}")


def selftest(hosts=False):
    import logging, re
    seen = set()
    if hosts:
        class H(logging.Handler):
            def emit(self, rec):
                seen.update(re.findall(r"https?://([^/:\s'\"]+)", rec.getMessage()))
        for lg in ("httpx", "httpcore", "urllib3", "huggingface_hub"):
            l = logging.getLogger(lg); l.setLevel(logging.DEBUG); l.addHandler(H())
    from huggingface_hub import HfApi
    api, repo = HfApi(), repo_id()
    print(f"token user: {api.whoami()['name']}")
    info = api.repo_info(repo, repo_type="dataset")
    print(f"repo {repo}: private={info.private}")
    if not info.private:
        print("WARNING: the data repo is public; make it private on huggingface.co")
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
        api.delete_folder("_selftest", repo, repo_type="dataset", commit_message="selftest cleanup")
    if hosts:
        print("hosts contacted: " + " ".join(sorted(seen)))
    print("SELFTEST OK" if ok else "SELFTEST FAILED: downloaded bytes differ")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a or a[0] not in ("push", "pull", "ls", "selftest"):
        sys.exit(__doc__)
    if a[0] == "push":
        push(a[1], dry="--dry-run" in a)
    elif a[0] == "pull":
        pull(a[1:])
    elif a[0] == "ls":
        ls()
    else:
        selftest(hosts="--hosts" in a)

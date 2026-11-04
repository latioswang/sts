"""Prints a filtered view of one game method, for comparing a rule with the simulator.

Usage: jm.py <class under com.megacrit.cardcrawl> <method substring> [regex]
Set JAVAP to a Java 8 javap and STS_GAME_JAR if the game isn't in ~/.sts_realgame. Output comes from your own copy of the
game, so keep it out of the repo.
"""
import os, re, subprocess, sys
jp = os.environ.get("JAVAP", os.path.expanduser("~/Projects/sts_lightspeed/.modbuild/tools/jdk8/Contents/Home/bin/javap"))
jar = os.environ.get("STS_GAME_JAR", os.path.expanduser("~/.sts_realgame/desktop-1.0.jar"))
cls, method = sys.argv[1:3]
pat = sys.argv[3] if len(sys.argv) > 3 else r"invoke|new |getstatic|getfield|putfield|ldc|bipush|sipush|iconst|if|goto|return|fmul|f2i|i2f|irem|idiv"
out = subprocess.run([jp, "-c", "-p", "-constants", "-cp", jar, "com.megacrit.cardcrawl." + cls], capture_output=True, text=True).stdout.splitlines()
idx = [i for i, l in enumerate(out) if method in l and re.match(r"  [a-z]", l)]
for start in idx:
    end = next((i for i in range(start + 1, len(out)) if re.match(r"  [a-z]", out[i])), len(out))
    print(out[start].strip()[:110])
    for l in out[start + 2:end]:
        if re.search(pat, l) and "Iterator" not in l and "hasNext" not in l and ".next:" not in l:
            l = re.sub(r"\s+", " ", l)
            l = re.sub(r"#\d+\s*// ", "", l).replace("com/megacrit/cardcrawl/", "")
            print(l[:100])

#!/usr/bin/env python3
"""Starts tools/captures.sh with --edit, sends it SIGINT (or SIGTERM) once the
"edit:" line has printed, and checks the edited file is restored.
A plain `cmd &` from a non-interactive shell starts the child with SIGINT
ignored, which bash cannot trap, so the signal has to be sent to a child that
starts with the default disposition: python's subprocess does that.
Usage: interrupt-test.py <repo root> [INT|TERM]"""
import os, signal, subprocess, sys, time

root = sys.argv[1]
sig = getattr(signal, "SIG" + (sys.argv[2] if len(sys.argv) > 2 else "INT"))
path = "desert/src/world.cpp"
cmd = ["tools/captures.sh", "--edit", path,
       "return glm::vec3{r, g, b} / 255.f;", "return glm::vec3{r, g, b} / 25.f;",
       "/tmp/captures-check/cases.txt"]
before = os.stat(os.path.join(root, path)).st_mtime
time.sleep(1)
p = subprocess.Popen(cmd, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
for line in p.stdout:
    print(line.rstrip()[:120])
    if line.startswith("edit:"):
        break
edited = subprocess.run(["git", "diff", "--quiet", path], cwd=root).returncode != 0
print("edited while running:", edited)
p.send_signal(sig)
rest = p.stdout.read()
rc = p.wait()
print("exit status:", rc)
clean = subprocess.run(["git", "diff", "--quiet", path], cwd=root).returncode == 0
after = os.stat(os.path.join(root, path)).st_mtime
print("restored:", clean, "mtime newer:", after > before)
sys.exit(0 if (edited and clean and after > before) else 1)

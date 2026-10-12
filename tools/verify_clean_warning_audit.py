"""Verify a complete clean rebuild including explicitly recorded repair steps.

Never discard the unsuccessful log. Require every enabled compilation unit to
have a successful compilation in the recorded sequence and Ninja to be current.
"""
from pathlib import Path
from hashlib import sha256
import argparse
import json
import re
import shlex
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument("--output",required=True,type=Path)
p.add_argument("--initial-clean",required=True,type=Path)
p.add_argument("--successful-build",action="append",required=True,type=Path)
a=p.parse_args()
source=Path(__file__).resolve().parents[1];build=source.parent/"build"
assert not a.output.exists()
assert "Cleaning all built files" in a.initial_clean.read_text()
expected=set()
for line in subprocess.check_output(["ninja","-t","commands"],cwd=build,text=True).splitlines():
    command=shlex.split(line)
    if "-c" in command and "-o" in command:
        expected.add(command[command.index("-o")+1])
latest={};records=[]
for index,log in enumerate([a.initial_clean,*a.successful_build]):
    t=log.read_text()
    warnings=re.findall(r"^.*\bwarning:.*$",t,re.M)
    errors=re.findall(r"^.*\berror:.*$",t,re.M)
    if index: assert not errors and "FAILED:" not in t,t[-2000:]
    active=None
    for line in t.splitlines():
        start=re.search(r"Building (?:CXX|C) object (.+\.o)",line)
        if start:
            active=start[1];latest[active]=dict(warnings=[],failed=False)
        elif "Linking " in line: active=None
        if re.search(r"\bwarning:",line):
            assert active,"Unattributed warning: "+line
            latest[active]["warnings"].append(line)
        if line.startswith("FAILED:") and active:latest[active]["failed"]=True
    records.append(dict(path=str(log.resolve()),sha256=sha256(log.read_bytes()).hexdigest(),
        warnings=len(warnings),errors=len(errors),initial_repair_needed=bool(errors)))
assert expected and expected<=set(latest),sorted(expected-set(latest))
for obj in expected:assert not latest[obj]["warnings"] and not latest[obj]["failed"],(obj,latest[obj])
current=subprocess.check_output(["ninja","-n"],cwd=build,text=True)
assert "no work to do" in current and not re.search(r"\[\d+/\d+\]",current),current
result=dict(clean_rebuild_completed=True,all_enabled_units_compiled=True,
    compilation_units=len(expected),compiler_warnings=0,
    repair_history_retained=True,records=records,
    binary_sha256=sha256((build/"inde").read_bytes()).hexdigest(),
    limits=["Initial compile repair and discovered legacy color/test warnings remain in the historical logs; the last compilation of every enabled unit is warning-free",
      "Not a guarantee for disabled targets or other SDK/compiler versions"])
a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print("PASS clean rebuild and warning audit",a.output)

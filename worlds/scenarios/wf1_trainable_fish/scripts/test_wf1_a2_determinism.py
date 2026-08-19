from __future__ import annotations
import argparse,filecmp,shutil,subprocess
from pathlib import Path

def run(demo,root,name):
    p=root/name
    if p.exists(): shutil.rmtree(p)
    p.mkdir(parents=True)
    r=subprocess.run([str(demo),name],cwd=root,capture_output=True,text=True)
    if r.returncode: raise SystemExit(r.stdout+r.stderr)
    return p
def main():
    p=argparse.ArgumentParser();p.add_argument("--demo",required=True);a=p.parse_args(); demo=Path(a.demo).resolve();root=demo.parents[5]
    x=run(demo,root,"build/worlds/scenarios/wf1_trainable_fish/a2_det_a"); y=run(demo,root,"build/worlds/scenarios/wf1_trainable_fish/a2_det_b")
    for name in ("summary.txt","training.csv","evaluation.csv","seeds.csv"):
        if not filecmp.cmp(x/name,y/name,shallow=False): raise SystemExit(f"WF1-A.2 determinism mismatch: {name}")
    shutil.rmtree(x);shutil.rmtree(y);print("WF1-A.2 determinism OK");return 0
if __name__=="__main__":raise SystemExit(main())
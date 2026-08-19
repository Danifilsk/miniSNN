from __future__ import annotations
import argparse
import csv
from pathlib import Path

REQUIRED={"terminal_reward","terminal_boundary","positive_reward","training_world_seed_base","evaluation_world_seed_base","positive_rewards","training_starvations","untrained_eval_mean_food","trained_eval_mean_food","control_eval_mean_food","untrained_success","trained_success","control_success","first_32_wait_rate","last_32_wait_rate","changed_weight_seed_count","improved_seed_count","POLICY_COLLAPSE","result"}
def summary(path:Path)->dict[str,str]:
    return dict(line.split("=",1) for line in path.read_text(encoding="utf-8").splitlines() if "=" in line)
def main()->int:
    p=argparse.ArgumentParser(); p.add_argument("--directory",required=True); d=Path(p.parse_args().directory)
    for name in ("summary.txt","training.csv","evaluation.csv","seeds.csv"):
        if not (d/name).is_file(): raise SystemExit(f"WF1-A.2 checker: missing {name}")
    s=summary(d/"summary.txt")
    if REQUIRED-s.keys(): raise SystemExit("WF1-A.2 checker: incomplete summary")
    if s["terminal_reward"]!="0" or s["terminal_boundary"]!="STARVATION_DEATH": raise SystemExit("WF1-A.2 checker: terminal ablation changed the boundary")
    if s["positive_reward"]!="EAT_APPLIED_ENERGY_GAIN": raise SystemExit("WF1-A.2 checker: reward contract changed")
    rows=list(csv.DictReader((d/"evaluation.csv").open(encoding="utf-8",newline="")))
    if len([r for r in rows if r["phase"]=="UNTRAINED"])!=96 or len([r for r in rows if r["phase"]=="TRAINED"])!=96 or len([r for r in rows if r["phase"]=="CONTROL"])!=96: raise SystemExit("WF1-A.2 checker: held-out protocol mismatch")
    for r in rows:
        if r["phase"] in {"UNTRAINED","TRAINED","CONTROL"} and r["weight_signature_before"]!=r["weight_signature_after"]: raise SystemExit("WF1-A.2 checker: evaluation changed weights")
    training=list(csv.DictReader((d/"training.csv").open(encoding="utf-8",newline="")))
    if len(training)!=1024: raise SystemExit("WF1-A.2 checker: training episode count mismatch")
    if any(int(r["terminal_reward_count"]) != int(r["starvation_count"]) for r in training): raise SystemExit("WF1-A.2 checker: terminal boundary not recorded")
    print("WF1-A.2 terminal reward ablation validation OK"); return 0
if __name__=="__main__": raise SystemExit(main())
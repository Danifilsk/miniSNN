from __future__ import annotations
import argparse, csv
from pathlib import Path

def values(path: Path) -> dict[str, str]:
    return dict(line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines() if "=" in line)

def main() -> int:
    parser=argparse.ArgumentParser(); parser.add_argument("--directory",required=True); directory=Path(parser.parse_args().directory)
    summary_path=directory/"summary.txt"; evaluation_path=directory/"evaluation.csv"
    if not summary_path.is_file() or not evaluation_path.is_file(): raise SystemExit("WF1-A.4P checker: missing artifacts")
    s=values(summary_path)
    for k,v in {"experiment":"WF1-A.4P","source_protocol":"WF1-A.2","behavior_tuning":"NONE","reward_learning_rate":"0.01","eligibility_tau":"UNCHANGED","training_episodes_per_seed":"64","evaluation_episodes_per_seed":"12"}.items():
        if s.get(k)!=v: raise SystemExit(f"WF1-A.4P checker: {k} mismatch")
    with evaluation_path.open(newline="",encoding="utf-8") as h: rows=list(csv.DictReader(h))
    if len(rows)!=144: raise SystemExit("WF1-A.4P checker: expected 144 held-out rows")
    groups={"UNTRAINED":0,"TRAINED":0,"CONTROL":0}
    for row in rows:
        groups[row["phase"]]+=1
        if row["phase"]=="TRAINED" and row["weight_signature_before"]!=row["weight_signature_after"]: raise SystemExit("WF1-A.4P checker: evaluation weights changed")
        if int(row["world_seed"]) < 990000: raise SystemExit("WF1-A.4P checker: old held-out world seed")
    if groups!={"UNTRAINED":48,"TRAINED":48,"CONTROL":48}: raise SystemExit("WF1-A.4P checker: phase counts mismatch")
    print("WF1-A.4P pilot validation OK"); return 0
if __name__=="__main__": raise SystemExit(main())
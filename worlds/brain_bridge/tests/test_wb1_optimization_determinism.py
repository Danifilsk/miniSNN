#!/usr/bin/env python3
"""Compare the WB1 contract test compiled with O0 and O2."""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

FLAGS=("-std=c11","-Wall","-Wextra","-Wpedantic","-Wformat=2","-Wstrict-prototypes","-DMINISNN_TESTING")
def run(command:list[str], cwd:Path)->str:
    result=subprocess.run(command,cwd=cwd,text=True,capture_output=True,check=False)
    if result.returncode!=0:
        raise SystemExit("WB1 O0/O2 FAILED:\n"+result.stdout+result.stderr)
    return result.stdout

def main()->int:
    parser=argparse.ArgumentParser(); parser.add_argument("--compiler",required=True); parser.add_argument("--bridge-root",required=True); args=parser.parse_args()
    bridge=Path(args.bridge_root).resolve(); repository=bridge.parent.parent; core=repository/"core"; domain=repository/"worlds"/"domain"; kernel=repository/"worlds"/"kernel"; core_sources=sorted((core/"src").glob("*.c"))
    if not core_sources: raise SystemExit("WB1 O0/O2 FAILED: Core sources unavailable")
    sources=[kernel/"src"/"minisnn_worlds_kernel.c",kernel/"src"/"minisnn_worlds_kernel_snapshot.c",kernel/"src"/"minisnn_worlds_kernel_command_log.c"]
    with tempfile.TemporaryDirectory(prefix="wb1_o0_o2_") as directory:
        root=Path(directory); outputs=[]
        for optimization in ("-O0","-O2"):
            executable=root/("wb1"+optimization[1:]+".exe")
            command=[args.compiler,*FLAGS,optimization,f"-I{bridge/'include'}",f"-I{core/'include'}",f"-I{domain/'include'}",f"-I{kernel/'include'}",str(bridge/"tests"/"test_wb1_trainable_brain.c"),str(bridge/"src"/"minisnn_worlds_brain_bridge.c"),str(bridge/"src"/"minisnn_worlds_trainable_brain.c"),str(domain/"src"/"minisnn_worlds_domain.c"),*(str(p) for p in sources),*(str(p) for p in core_sources),"-lm","-o",str(executable)]
            outputs.append(run(command,root)+run([str(executable)],root))
        if outputs[0] != outputs[1]:
            raise SystemExit("WB1 O0/O2 FAILED: output differs")
        summary_hash = hashlib.sha256(outputs[0].encode("utf-8")).hexdigest()
        values = {}
        for line in outputs[0].splitlines():
            key, separator, value = line.partition("=")
            if separator:
                values[key] = value
        required = (
            "topology_signature", "config_signature", "initial_weight_signature",
            "learned_weight_signature", "post_episode_reset_weight_signature",
            "evaluation_weight_signature", "loaded_weight_signature", "core_tick",
        )
        if any(key not in values for key in required):
            raise SystemExit("WB1 O0/O2 FAILED: stable learning summary is incomplete")
        if values["initial_weight_signature"] == values["learned_weight_signature"] or \
           values["learned_weight_signature"] != values["post_episode_reset_weight_signature"] or \
           values["learned_weight_signature"] != values["evaluation_weight_signature"] or \
           values["loaded_weight_signature"] == values["evaluation_weight_signature"]:
            raise SystemExit("WB1 O0/O2 FAILED: stable learning summary violates the WB1 contract")
    print(f"WB1 O0 summary_sha256={summary_hash}")
    print(f"WB1 O2 summary_sha256={summary_hash}")
    print("WB1 O0/O2 deterministic validation OK"); return 0
if __name__=="__main__": raise SystemExit(main())
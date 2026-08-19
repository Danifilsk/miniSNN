#!/usr/bin/env python3
"""Compile and run WB1 on POSIX when a native C11 environment is available."""
from __future__ import annotations
import argparse, os, shutil, subprocess, tempfile
from pathlib import Path
FLAGS=("-std=c11","-Wall","-Wextra","-Wpedantic","-Wformat=2","-Wstrict-prototypes","-DMINISNN_TESTING")
def main()->int:
 parser=argparse.ArgumentParser();parser.add_argument("--bridge-root",required=True);args=parser.parse_args();compiler=shutil.which("cc")
 if os.name=="nt" or compiler is None: print("WB1 POSIX smoke UNAVAILABLE");return 0
 bridge=Path(args.bridge_root).resolve();repo=bridge.parent.parent;core=repo/"core";domain=repo/"worlds"/"domain";kernel=repo/"worlds"/"kernel";core_sources=sorted((core/"src").glob("*.c"))
 
 with tempfile.TemporaryDirectory(prefix="wb1_posix_") as directory:
  root=Path(directory);exe=root/"wb1";sources=[kernel/"src"/"minisnn_worlds_kernel.c",kernel/"src"/"minisnn_worlds_kernel_snapshot.c",kernel/"src"/"minisnn_worlds_kernel_command_log.c"]
  command=[compiler,*FLAGS,f"-I{bridge/'include'}",f"-I{core/'include'}",f"-I{domain/'include'}",f"-I{kernel/'include'}",str(bridge/"tests"/"test_wb1_trainable_brain.c"),str(bridge/"src"/"minisnn_worlds_brain_bridge.c"),str(bridge/"src"/"minisnn_worlds_trainable_brain.c"),str(domain/"src"/"minisnn_worlds_domain.c"),*(str(p) for p in sources),*(str(p) for p in core_sources),"-lm","-o",str(exe)]
  if subprocess.run(command,cwd=root,check=False).returncode!=0 or subprocess.run([str(exe)],cwd=root,check=False).returncode!=0: print("WB1 POSIX smoke FAIL");return 1
 print("WB1 POSIX smoke PASS");return 0
if __name__=="__main__":raise SystemExit(main())
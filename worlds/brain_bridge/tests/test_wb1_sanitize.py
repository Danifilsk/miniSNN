#!/usr/bin/env python3
"""Run the WB1 contract test under ASan/UBSan when available."""
from __future__ import annotations
import argparse
from pathlib import Path
import subprocess
import tempfile
FLAGS=("-std=c11","-Wall","-Wextra","-Wpedantic","-Wformat=2","-Wstrict-prototypes","-DMINISNN_TESTING","-fsanitize=address,undefined","-fno-omit-frame-pointer")
def run(command:list[str],cwd:Path)->subprocess.CompletedProcess[str]: return subprocess.run(command,cwd=cwd,text=True,capture_output=True,check=False)
def main()->int:
 parser=argparse.ArgumentParser();parser.add_argument("--compiler",required=True);parser.add_argument("--bridge-root",required=True);args=parser.parse_args();bridge=Path(args.bridge_root).resolve();repo=bridge.parent.parent;core=repo/"core";domain=repo/"worlds"/"domain";kernel=repo/"worlds"/"kernel";core_sources=sorted((core/"src").glob("*.c"))
 with tempfile.TemporaryDirectory(prefix="wb1_sanitize_") as directory:
  root=Path(directory);probe=root/"probe.c";probe.write_text("int main(void){return 0;}\n",encoding="ascii");probe_exe=root/"probe.exe"
  if run([args.compiler,*FLAGS,str(probe),"-o",str(probe_exe)],root).returncode!=0 or run([str(probe_exe)],root).returncode!=0: print("WB1 sanitizer: UNAVAILABLE");return 0
  executable=root/"wb1.exe";sources=[kernel/"src"/"minisnn_worlds_kernel.c",kernel/"src"/"minisnn_worlds_kernel_snapshot.c",kernel/"src"/"minisnn_worlds_kernel_command_log.c"]
  build=run([args.compiler,*FLAGS,f"-I{bridge/'include'}",f"-I{core/'include'}",f"-I{domain/'include'}",f"-I{kernel/'include'}",str(bridge/"tests"/"test_wb1_trainable_brain.c"),str(bridge/"src"/"minisnn_worlds_brain_bridge.c"),str(bridge/"src"/"minisnn_worlds_trainable_brain.c"),str(domain/"src"/"minisnn_worlds_domain.c"),*(str(p) for p in sources),*(str(p) for p in core_sources),"-lm","-o",str(executable)],root)
  result=run([str(executable)],root) if build.returncode==0 else build
  if build.returncode!=0 or result.returncode!=0: print("WB1 sanitizer: FAIL\n"+build.stdout+build.stderr+result.stdout+result.stderr);return 1
 print("WB1 sanitizer: PASS");return 0
if __name__=="__main__":raise SystemExit(main())
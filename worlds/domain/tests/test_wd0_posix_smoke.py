#!/usr/bin/env python3
"""Compile and run representative WD0 coverage on a clean POSIX C11 toolchain."""
from __future__ import annotations
import argparse, os, shutil, subprocess, tempfile
from pathlib import Path
FLAGS=['-std=c11','-Wall','-Wextra','-Wpedantic','-Wformat=2','-Wstrict-prototypes']
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--domain-root',required=True); a=ap.parse_args()
    cc=shutil.which('cc')
    if os.name=='nt' or cc is None:
        print('WD0 POSIX smoke UNAVAILABLE'); return 0
    domain=Path(a.domain_root).resolve(); kernel=domain.parent/'kernel'
    ks=[kernel/'src'/n for n in ('minisnn_worlds_kernel.c','minisnn_worlds_kernel_snapshot.c','minisnn_worlds_kernel_restore.c','minisnn_worlds_kernel_command_log.c')]
    tests=['test_wd0_domain.c','test_wd0_actions.c','test_wd0_perception.c','test_wd0_atomicity.c','test_wd0_stress.c']
    with tempfile.TemporaryDirectory(prefix='wd0_posix_') as td:
        root=Path(td)
        for name in tests:
            exe=root/(Path(name).stem)
            cmd=[cc,*FLAGS,'-DMINISNN_WORLDS_DOMAIN_TESTING','-DMINISNN_WORLDS_KERNEL_TESTING','-DORGANISM_COUNT=16U','-DFOOD_COUNT=32U','-DTICK_COUNT=8U',f'-I{domain/"include"}',f'-I{kernel/"include"}',f'-I{domain/"app"}',str(domain/'tests'/name),str(domain/'src'/'minisnn_worlds_domain.c')]
            if name=='test_wd0_stress.c': cmd.append(str(domain/'app'/'wd0_test_controller.c'))
            cmd += [*(str(x) for x in ks),'-o',str(exe)]
            if subprocess.run(cmd,check=False).returncode!=0 or subprocess.run([str(exe)],check=False,cwd=root).returncode!=0:
                print('WD0 POSIX smoke FAIL'); return 1
        demo=root/'wd0_demo'; results=root/'results'; results.mkdir()
        cmd=[cc,*FLAGS,f'-I{domain/"include"}',f'-I{kernel/"include"}',str(domain/'app'/'wd0_domain_demo.c'),str(domain/'src'/'minisnn_worlds_domain.c'),*(str(x) for x in ks),'-o',str(demo)]
        if subprocess.run(cmd,check=False).returncode!=0 or subprocess.run([str(demo),str(results)],check=False,cwd=root).returncode!=0:
            print('WD0 POSIX smoke FAIL'); return 1
        if subprocess.run(['python',str(domain/'scripts'/'check_wd0.py'),'--domain-root',str(domain),'--directory',str(results)],check=False,cwd=root).returncode!=0:
            print('WD0 POSIX smoke FAIL'); return 1
    print('WD0 POSIX smoke PASS'); return 0
if __name__=='__main__': raise SystemExit(main())

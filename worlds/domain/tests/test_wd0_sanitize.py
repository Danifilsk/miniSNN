#!/usr/bin/env python3
"""Run complete focused WD0 coverage when ASan/UBSan is genuinely available."""
from __future__ import annotations
import argparse
from pathlib import Path
import subprocess
import tempfile

FLAGS=["-std=c11","-Wall","-Wextra","-Wpedantic","-Wformat=2","-Wstrict-prototypes",
       "-fsanitize=address,undefined","-fno-omit-frame-pointer"]

def run(cmd, cwd=None):
    return subprocess.run(cmd,text=True,capture_output=True,check=False,cwd=cwd)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--compiler',required=True); ap.add_argument('--domain-root',required=True); a=ap.parse_args()
    domain=Path(a.domain_root).resolve(); kernel=domain.parent/'kernel'
    ks=[kernel/'src'/n for n in ('minisnn_worlds_kernel.c','minisnn_worlds_kernel_snapshot.c','minisnn_worlds_kernel_restore.c','minisnn_worlds_kernel_command_log.c')]
    with tempfile.TemporaryDirectory(prefix='wd0_sanitize_') as td:
        root=Path(td); probe=root/'probe.c'; exe=root/'probe.exe'; probe.write_text('int main(void){return 0;}\n',encoding='ascii')
        c=run([a.compiler,*FLAGS,str(probe),'-o',str(exe)])
        if c.returncode!=0 or run([str(exe)],root).returncode!=0:
            print('WD0 sanitizer: UNAVAILABLE'); return 0
        tests=['test_wd0_domain.c','test_wd0_actions.c','test_wd0_perception.c','test_wd0_invariants.c','test_wd0_atomicity.c','test_wd0_stress.c']
        for name in tests:
            exe=root/(Path(name).stem+'.exe')
            cmd=[a.compiler,*FLAGS,'-DMINISNN_WORLDS_DOMAIN_TESTING','-DMINISNN_WORLDS_KERNEL_TESTING',
                 '-DORGANISM_COUNT=16U','-DFOOD_COUNT=32U','-DTICK_COUNT=8U',
                 f'-I{domain/"include"}',f'-I{kernel/"include"}',f'-I{domain/"app"}',
                 str(domain/'tests'/name),str(domain/'src'/'minisnn_worlds_domain.c')]
            if name=='test_wd0_stress.c': cmd.append(str(domain/'app'/'wd0_test_controller.c'))
            cmd += [*(str(x) for x in ks),'-o',str(exe)]
            c=run(cmd)
            if c.returncode!=0:
                print('WD0 sanitizer: FAIL\n'+c.stdout+c.stderr); return 1
            r=run([str(exe)],root)
            if r.returncode!=0:
                print('WD0 sanitizer: FAIL\n'+r.stdout+r.stderr); return 1
        # Demo under sanitizers + real checker.
        demo=root/'wd0_domain_demo.exe'; results=root/'results'; results.mkdir()
        cmd=[a.compiler,*FLAGS,f'-I{domain/"include"}',f'-I{kernel/"include"}',str(domain/'app'/'wd0_domain_demo.c'),
             str(domain/'src'/'minisnn_worlds_domain.c'),*(str(x) for x in ks),'-o',str(demo)]
        c=run(cmd)
        if c.returncode!=0:
            print('WD0 sanitizer: FAIL\n'+c.stdout+c.stderr); return 1
        r=run([str(demo),str(results)],root)
        if r.returncode!=0:
            print('WD0 sanitizer: FAIL\n'+r.stdout+r.stderr); return 1
        r=run(['python',str(domain/'scripts'/'check_wd0.py'),'--domain-root',str(domain),'--directory',str(results)],root)
        if r.returncode!=0:
            print('WD0 sanitizer: FAIL\n'+r.stdout+r.stderr); return 1
    print('WD0 sanitizer: PASS'); return 0
if __name__=='__main__': raise SystemExit(main())

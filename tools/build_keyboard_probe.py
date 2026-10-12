"""Build keyboard task probes with Release objects or explicitly focal instrumentation."""
import argparse, hashlib, json, shlex, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--mode',choices=('release','debug','asan'),default='release')
p.add_argument('--prepare-only',action='store_true')
a=p.parse_args();source=Path(__file__).resolve().parents[1];build=source.parent/'build'
output=build/('inde_ui_keyboard_smoke' if a.mode=='release' else 'inde_ui_keyboard_'+a.mode)
if a.mode=='release':
    subprocess.run(['python3',str(source/'tools/build_ui_workspace_smoke.py'),'--build',str(build),'--test','keyboard','--output',str(output)],check=True)
else:
    lines=subprocess.check_output(['ninja','-C',str(build),'-t','commands','inde'],text=True).splitlines()
    tokens=shlex.split(next(s for s in reversed(lines) if ' -o inde ' in s))
    objects=[s for s in tokens if s.endswith('.o') and not s.endswith('/src/main.cpp.o')]
    libs=tokens[tokens.index('-o')+2:];libs=libs[:libs.index('&&')] if '&&' in libs else libs
    names=('keyboard_commands.cpp','main_window_keyboard.cpp','writing_workspace_keyboard.cpp',
        'cartography_workspace_keyboard.cpp','workspace_keyboard.cpp','composition_page_keyboard.cpp',
        'overlay_dialog.cpp','context_menu.cpp','calendar_picker.cpp','timeline_view.cpp','proofreading_controller.cpp')
    objects=[o for o in objects if not any(o.endswith('/src/ui/'+n+'.o') for n in names)]
    flags=['-std=c++20','-O0','-g1','-UNDEBUG','-Wall','-Wextra','-Wpedantic','-I'+str(source/'include'),
           *shlex.split(subprocess.check_output(['pkg-config','--cflags','gtkmm-4.0'],text=True))]
    if a.mode=='asan':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    cache=build/('keyboard-focal-'+a.mode);cache.mkdir(exist_ok=True)
    headers=hashlib.sha256()
    for header in sorted([*source.joinpath('include').rglob('*.hpp'),*source.joinpath('src').rglob('*.hpp')]):headers.update(header.read_bytes())
    for name in names:
        unit=source/'src/ui'/name;obj=cache/(name+'.o');stamp=cache/(name+'.sha256')
        digest=hashlib.sha256(unit.read_bytes()+headers.digest()+repr(flags).encode()).hexdigest()
        if not obj.exists() or not stamp.exists() or stamp.read_text()!=digest:
            print('Compiling focal',a.mode,name,flush=True)
            subprocess.run(['c++',*flags,'-c',str(unit),'-o',str(obj)],check=True);stamp.write_text(digest)
        objects.append(str(obj))
    if a.prepare_only:
        print('Prepared focal objects only; no linked-driver or passing-test claim.',flush=True)
        raise SystemExit(0)
    subprocess.run(['c++',*flags,str(source/'tests/ui_keyboard_smoke.cpp'),*objects,*libs,'-o',str(output)],cwd=build,check=True)
    (cache/'build.json').write_text(json.dumps(dict(mode='focal '+a.mode,units=names,driver_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),limits='Remaining production objects and dependencies Release. LSan disabled in ASan runs.'),indent=2)+'\n')
print(output,flush=True)

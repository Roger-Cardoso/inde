"""Compare the recorded P5-B revision with current persistent sessions.
Creates only a new synthetic evidence directory; never opens authors' projects.
"""
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import sys

BASE=Path(__file__).resolve().parents[2]
REVISION='e605b3726522fd0d17943287c7950a6ed7777e23'
root=Path(sys.argv[1]).absolute();root.mkdir(parents=True,exist_ok=False)
current=Path(sys.argv[2]).absolute() if len(sys.argv)>2 else BASE/'build-p5/inde_project_file_benchmark'
baseline=root/'baseline-source';baseline.mkdir()

def run(args,**kwargs):return subprocess.run(list(map(str,args)),check=True,**kwargs)

paths=subprocess.check_output(['git','ls-tree','-r','--name-only',REVISION,'cpp'],cwd=BASE,text=True).splitlines()
paths=[p for p in paths if p.startswith('cpp/project_file')]+['tests/project_file/CMakeLists.txt','verificar_base.py']
provenance={}
for name in paths:
    data=subprocess.check_output(['git','show',REVISION+':'+name],cwd=BASE)
    path=baseline/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    provenance[name]=hashlib.sha256(data).hexdigest()
for name in ['metadata','fontes']:(baseline/name).symlink_to(BASE/name,target_is_directory=True)
driver=(BASE/'cpp/project_file_benchmark.cpp').read_bytes();(baseline/'cpp/project_file_benchmark.cpp').write_bytes(driver)
p=baseline/'tests/project_file/CMakeLists.txt'
p.write_text(p.read_text()+'\nadd_executable(inde_project_file_benchmark ${P5_ROOT}/cpp/project_file_benchmark.cpp)\ntarget_link_libraries(inde_project_file_benchmark PRIVATE inde_project_file)\n')
with (root/'baseline-build.log').open('w') as log:
    run(['cmake','-S',baseline/'tests/project_file','-B',baseline/'build','-G','Ninja','-DCMAKE_BUILD_TYPE=Release'],stdout=log,stderr=log)
    run(['cmake','--build',baseline/'build','--parallel','2','--target','inde_project_file_benchmark','inde_project_file_cli'],stdout=log,stderr=log)
rows=[]
for size in [1,4,16]:
    modes=[('baseline',baseline/'build/inde_project_file_benchmark','whole'),('whole_current',current,'whole'),('blocks_current',current,'blocks')]
    for label,binary,mode in modes:
        tag=f'{size}MiB-{label}'
        with (root/(tag+'.log')).open('w') as log:
            result=run([binary,root/tag,size,mode],stdout=subprocess.PIPE,stderr=log,text=True)
        row=json.loads(result.stdout);row['implementation']=label;row['median_save_ms']=statistics.median(row['save_ms']);rows.append(row)
        (root/(tag+'.json')).write_text(json.dumps(row,ensure_ascii=False,indent=2)+'\n')
        print(tag,round(row['median_save_ms'],2),'ms',row['new_blob_bytes'],'new bytes',flush=True)
    old,whole,blocks=rows[-3:]
    assert old['database_bytes']==whole['database_bytes']==blocks['database_bytes']
    assert all(r['history_and_restore_verified'] for r in [old,whole,blocks])
    assert blocks['new_blob_bytes']<old['new_blob_bytes']/8,'History growth regression'
# Prove old materializers refuse the new profile without altering it, and
# continue to read new states written in the previous profile.
old_cli=baseline/'build/inde_project_file'
file=root/'1MiB-blocks_current/Projeto.inde';before=hashlib.sha256(file.read_bytes()).hexdigest()
r=subprocess.run([str(old_cli),'reabrir',str(file),str(root/'old-reader-blocks')],capture_output=True,text=True)
assert r.returncode==1 and 'transport_profile' in r.stderr and not (root/'old-reader-blocks').exists()
assert hashlib.sha256(file.read_bytes()).hexdigest()==before
with (root/'old-reader-refusal.txt').open('w') as out:out.write(r.stderr)
run([old_cli,'reabrir',root/'1MiB-whole_current/Projeto.inde',root/'old-reader-whole'],stdout=subprocess.DEVNULL)
(root/'comparison.json').write_text(json.dumps(dict(format='INDE-P5C-COMPARISON-1',baseline_revision=REVISION,
    benchmark_sha256=hashlib.sha256(driver).hexdigest(),baseline_sources=provenance,
    method='same deterministic SQLite fixture; new store per mode; six small updates; persistent FileSession.save; sequential Release binaries; timings are local observations, not UI latency guarantees',
    old_reader_refuses_blocks_without_changes=True,old_reader_reads_current_whole_profile=True,results=rows),ensure_ascii=False,indent=2)+'\n')

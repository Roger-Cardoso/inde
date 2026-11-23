import json,shlex,subprocess,concurrent.futures
from pathlib import Path
root=Path('/home/roger/Documents/Projects/GitHub/INDE/obj01-audit');db=json.loads((root/'build-nativo/compile_commands.json').read_text());names=['object_native_ui.cpp','object_native_smoke.cpp','object_math_native.cpp','object_math_cairo.cpp']
def compile_one(name):
 item=next(x for x in db if Path(x['file']).name==name);args=shlex.split(item['command']);out=['/usr/bin/g++-13'];i=1
 while i<len(args):
  if args[i] in ['-include','-o','-MF','-MT']:
   opt=args[i];value=args[i+1];i+=2
   if opt=='-o':out+=['-o','/tmp/gtk-gcc13-'+name+'.o']
   if opt=='-MF':out+=['-MF','/tmp/gtk-gcc13-'+name+'.d']
   if opt=='-MT':out+=['-MT',name]
  elif args[i]=='-Winvalid-pch':i+=1
  else:out.append(args[i]);i+=1
 with open('/tmp/gtk-gcc13-'+name+'.txt','w') as f:r=subprocess.run(out,cwd=item['directory'],stdout=f,stderr=subprocess.STDOUT)
 return name,r.returncode
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
 results=list(pool.map(compile_one,names))
print(json.dumps(results));assert all(code==0 for name,code in results)

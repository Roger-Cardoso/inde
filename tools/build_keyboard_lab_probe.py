"""Link the laboratory keyboard task probe against its current production library."""
import shlex,subprocess
from pathlib import Path
source=Path(__file__).resolve().parents[1];build=source.parent/'build'
line=subprocess.check_output(['ninja','-t','commands','inde_text_blocks_lab'],cwd=build,text=True).splitlines()[-1]
tokens=shlex.split(line);tail=tokens[tokens.index('-o')+2:];tail=tail[:tail.index('&&')]
flags=['-std=c++20','-O2','-Wall','-Wextra','-Wpedantic','-I'+str(source/'include'),*shlex.split(subprocess.check_output(['pkg-config','--cflags','gtkmm-4.0'],text=True))]
output=build/'inde_ui_keyboard_lab_smoke'
subprocess.run(['c++',*flags,str(source/'tests/ui_keyboard_lab_smoke.cpp'),'CMakeFiles/inde_text_blocks_lab.dir/src/ui/rendering_policy.cpp.o',*tail,'-o',str(output)],cwd=build,check=True)
print(output)

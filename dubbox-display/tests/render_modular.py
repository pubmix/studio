"""Render real modular UI with software drawing and test real touch handlers."""
from pathlib import Path
import runpy,sys,subprocess
root=Path(__file__).resolve().parents[1]
sys.argv=[str(root/'tests/render_ui.py'),sys.argv[1] if len(sys.argv)>1 else '/tmp/studio-modular-preview']
h=runpy.run_path(str(root/'tests/render_ui.py'))
w=h['w'];cmd=h['cmd']
p=w/'main.cpp';s=p.read_text().replace('#include "screen_mixer.h"','#include "screen_mixer.h"\n#include "modular_panel.h"')
s=s.replace('State st;','State st;\nint auditions=0, edits=0;\nvoid auditionNote(int,int){++auditions;}\nvoid setModularPatch(int i,const modular::Patch& p){assert(modular::valid(p));st.modularPatch[i]=p;++edits;}')
s=s.replace('screen<4','screen<5')
s=s.replace('char path[512];','if(screen==4){drawHeader("MODULAR PATCH",kTabPattern);modularpanel::draw(0);drawTransport(true);}\nchar path[512];')
s=s.replace('puts("PASS:', '''modularpanel::draw(0);
modularpanel::touchDown(0,350,305); // oscillator 1 output
modularpanel::touchDown(0,70,530); // filter audio input
assert(teensylink::st.modularPatch[0].cable[modular::FilterIn]==modular::Osc1);
modularpanel::touchDown(0,750,415); // envelope output -> incompatible audio
modularpanel::touchDown(0,70,530);
assert(teensylink::st.modularPatch[0].cable[modular::FilterIn]==modular::Osc1);
modularpanel::touchDown(0,610,604); // unplug selected input
assert(teensylink::st.modularPatch[0].cable[modular::FilterIn]==-1);
modularpanel::touchDown(0,760,604);assert(teensylink::auditions==1);
modularpanel::touchDown(0,920,604);modularpanel::touchDown(0,920,604);
assert(modular::equal(teensylink::st.modularPatch[0],modular::Patch()));
puts("PASS: modular cable connect/type rejection/unplug/audition/starter restore; four rack renders.");
puts("PASS:''')
p.write_text(s)
cmd=cmd[:-2]+[str(root/'src/modular_panel.cpp')]+cmd[-2:]
subprocess.run(cmd,check=True);subprocess.run([str(w/'preview'),str(w)],check=True)

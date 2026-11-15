from pathlib import Path
import re,io
from PIL import Image
import resvg_py
root=Path(__file__).resolve().parents[2]
s=(root/'tools/home_menu_preview/device_admin_mock.html').read_text(encoding='utf-8')
items=[]
for key in ('miniDevice','deviceIcon','loadingIcon','warningIcon','doneIcon','updateIcon'):
 svg=re.search(r"const "+key+r"='([^']+)';",s).group(1)
 size=16 if key=='miniDevice' else 76
 svg=svg.replace('<svg ', '<svg xmlns="http://www.w3.org/2000/svg" fill="none" stroke="white" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" ',1)
 if size==16: svg=svg.replace('viewBox=', 'width="16" height="16" viewBox=')
 else:
  svg=svg.replace('xmlns="http://www.w3.org/2000/svg"','').replace('viewBox=', 'x="17" y="17" width="42" height="42" viewBox=')
  svg='<svg xmlns="http://www.w3.org/2000/svg" width="76" height="76" viewBox="0 0 76 76"><circle cx="38" cy="38" r="37.5" fill="none" stroke="white" stroke-width="1"/>'+svg+'</svg>'
 png=resvg_py.svg_to_bytes(svg_string=svg,background='black')
 im=Image.open(io.BytesIO(png)).convert('L'); stride=(size+7)//8;bits=bytearray(stride*size)
 for y in range(size):
  for x in range(size):
   if im.getpixel((x,y))>=128:bits[y*stride+x//8] |= 0x80>>(x%8)
 name=key[0].upper()+key[1:];items.append('inline constexpr uint8_t k'+name+'[] = {\n'+ '\n'.join('  '+','.join('0x%02X'%x for x in bits[i:i+16])+',' for i in range(0,len(bits),16))+'\n};')
 out=root/'tmp/admin-framebuffer';out.mkdir(parents=True,exist_ok=True); im.save(out/(key+'.png'))
(root/'src/ui/pages/DeviceAdminIcons.h').write_text('// Generated from the approved preview SVGs by tools/admin_framebuffer/export_icons.py.\n#pragma once\n#include <stdint.h>\nnamespace DeviceAdminIcons {\n'+'\n'.join(items)+'\n}\n',encoding='utf-8')
print('Exported the approved SVG silhouettes into 6 monochrome icons')

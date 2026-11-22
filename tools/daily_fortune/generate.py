from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
rows=[]
for line in (root/'docs/DAILY_FORTUNE_LIBRARY.md').read_text(encoding='utf-8-sig').splitlines():
 if re.match(r'^\| \d{3} \|',line):
  cols=[x.strip() for x in line.strip('|').split('|')]
  if len(cols)!=6 or int(cols[0])!=len(rows)+1: raise ValueError('Invalid fortune row: '+line)
  rows.append(cols)
if len(rows)!=90: raise ValueError('Expected 90 fortunes')
for row in rows:
 if any('"' in x or '\\' in x for x in row): raise ValueError('Unsupported quoting')
out=['#pragma once','#include <stdint.h>','namespace DailyFortuneData {','struct Entry { const char* name; const char* theme; const char* line; const char* detail1; const char* detail2; };','inline constexpr Entry kEntries[] = {']
out += ['  {'+', '.join('"'+x+'"' for x in r[1:])+'},' for r in rows]
out += ['};','inline constexpr uint16_t kCount = sizeof(kEntries) / sizeof(kEntries[0]);','}  // namespace DailyFortuneData','']
target=root/'src/data/DailyFortuneData.h';text='\n'.join(out)
import sys
if '--check' in sys.argv:
 if not target.exists() or target.read_text(encoding='utf-8-sig')!=text: raise SystemExit('Fortune data is out of date')
else: target.write_text(text,encoding='utf-8')
print('Fortune library:',len(rows))

#include "../../src/services/LrcParser.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
  uint32_t times[8]={}; const char* text=nullptr;
  assert(LrcParser::parse("[00:12.50]你好",times,8,text)==1 && times[0]==12500 && !strcmp(text,"你好"));
  assert(LrcParser::parse("[01:02.1][02:03.045]重复",times,8,text)==2 && times[0]==62100 && times[1]==123045);
  assert(LrcParser::parse("\xEF\xBB\xBF[00:01]开头",times,8,text)==1 && times[0]==1000);
  assert(LrcParser::parse("[ar:作者]",times,8,text)==0);
  assert(LrcParser::parse("[00:60.10]错误",times,8,text)==0);
  assert(LrcParser::parse("[00:01.]",times,8,text)==0);
  assert(LrcParser::parse("[00:01.20]",times,8,text)==0);
  assert(LrcParser::parse("",times,8,text)==0);
  std::puts("PASS: LRC timestamp formats, multi-tag, BOM and malformed input");
}

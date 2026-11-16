#include <SD_MMC.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <fstream>
#include <stdexcept>
#include "src/services/TxtReaderService.h"
#include "src/services/SdCardService.h"
#include "src/services/SdBitmapFont.h"
#include "src/services/UiFontCache.h"
#include "src/ui/pages/TxtReaderPage.h"
#include "src/ui/SdUiText.h"
#include "src/bsp/DisplayMonoTft.h"
#include "src/bsp/SdCardSession.h"

SdCardService::Status SdCardService::refresh(){return TestFs::unavailable?Status::NotInserted:Status::Ready;}
SdCardService::Status SdCardService::status()const{return TestFs::unavailable?Status::NotInserted:Status::Ready;}
DisplayMonoTft::DisplayMonoTft():display_(SPI){}
bool DisplayMonoTft::begin(){display_.setRotation(1);display_.clearDisplay();text_.begin(display_);return true;}
int DisplayMonoTft::width()const{return display_.getDisplayWidth();}
int DisplayMonoTft::height()const{return display_.getDisplayHeight();}
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas(){return display_;}
U8G2_FOR_ST73XX& DisplayMonoTft::text(){return text_;}
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void write(const char* name,const std::string& value){std::ofstream out(std::filesystem::u8path(TestFs::root+"/Books/"+name),std::ios::binary);out<<value;check(out.good(),"fixture write failed");}
int16_t width(void*,uint32_t){return 20;}
void settle(TxtReaderService& reader){for(int i=0;i<100000&&reader.busy();++i)reader.tick(width,nullptr);}
void load(SdBitmapFont& f){for(int i=0;i<10000&&f.state()==SdBitmapFont::State::Checking;++i)f.tick();check(f.state()==SdBitmapFont::State::Ready,"font readiness");}
void save(DisplayMonoTft& d,const char* name){
  uint8_t raw[8064];d.canvas().copyFrameBufferTo(raw);
  std::ofstream out(std::string("tmp/txt-reader-")+name+".pgm",std::ios::binary);out<<"P5\n384 168\n255\n";
  for(int y=0;y<168;++y)for(int x=0;x<384;++x){int16_t px,py;d.canvas().logicalToPhysical(x,y,px,py);
    out.put(static_cast<char>((raw[(py/2)*42+px/4]&(1U<<(7-((px%4)*2+py%2))))?0:255));}
}
int main(){try{
  using State=TxtReaderService::State;SdCardService sd;TxtReaderService r;
  const auto fixture = std::filesystem::absolute("tmp/txt-reader-card").lexically_normal();
  check(fixture.parent_path()==(std::filesystem::current_path()/"tmp"),"fixture outside workspace tmp");
  std::filesystem::remove_all(fixture);
  std::filesystem::create_directories(fixture);TestFs::root="tmp/txt-reader-card";
  check(std::filesystem::is_empty(TestFs::root),"use fresh test card");
  r.scan(sd);check(r.state()==State::Empty&&std::filesystem::is_directory(TestFs::root+"/Books"),"auto create /Books");
  TestFs::unavailable=true;r.scan(sd);check(r.state()==State::Missing,"missing SD");TestFs::unavailable=false;
  std::string original(202,'A');write("a.TXT",std::string("\xEF\xBB\xBF")+original);write("ignored.md","skip");
  r.scan(sd);check(r.count()==1,"TXT extension/filter");check(r.open(0),"open UTF8 BOM");settle(r);
  check(r.indexed()&&r.totalPages()>1&&r.pageReady(),"progressive page index");
  std::string reconstructed;
  for(uint32_t p=0;p<r.totalPages();++p){r.go(p);settle(r);for(auto& line:r.content().lines){std::string value=line;if(value.rfind("　",0)==0)value.erase(0,3);reconstructed+=value;}}
  check(reconstructed==original,"pagination lost/duplicated characters");
  r.go(1);settle(r);auto second=r.content();r.go(0);settle(r);r.go(1);settle(r);
  check(!memcmp(&second,&r.content(),sizeof(second)),"back/forward pagination mismatch");
  r.bookmark();r.toggleTheme();r.closeBook();check(TestPsram::blocks.empty()&&!TestFs::handles,"exit leaked file/index");
  r.scan(sd);check(r.open(0),"resume open");settle(r);check(r.currentPage()==1&&r.marked()&&r.dark(),"resume/bookmark/theme persistence");
  r.closeBook();write("a.TXT","changed content");r.scan(sd);r.open(0);settle(r);check(r.currentPage()==0&&!r.marked(),"modified file retained stale progress");r.closeBook();
  write("a.TXT",std::string("a\r\nb\rc\nd"));r.scan(sd);r.open(0);settle(r);check(r.totalPages()==1,"CRLF created extra page");r.closeBook();
  write("a.TXT",std::string("\xC0\xAF",2));r.scan(sd);r.open(0);settle(r);check(r.state()==State::Error,"invalid UTF8 accepted");check(!TestFs::handles&&TestPsram::blocks.empty(),"UTF8 error leaked resources");
  write("a.TXT","valid");r.scan(sd);TestPsram::fail=true;check(!r.open(0)&&r.state()==State::Error,"allocation failure not surfaced");TestPsram::fail=false;
  r.scan(sd);r.open(0);TestFs::unavailable=true;TestFs::readLatencyMs=1;
  // A file can already be buffered: use a longer fixture to cross the read boundary.
  TestFs::unavailable=false;r.closeBook();write("a.TXT",std::string(3000,'B'));r.scan(sd);r.open(0);TestFs::unavailable=true;settle(r);
  check(r.state()==State::Error&&!TestFs::handles&&TestPsram::blocks.empty(),"card loss leaked resources");TestFs::unavailable=false;
  r.end();
  // Render the actual firmware page and NBFT glyphs to the ST7305 framebuffer.
  std::filesystem::remove_all("tmp/txt-reader-card/Books");std::filesystem::create_directory("tmp/txt-reader-card/Books");
  const char* sample="清晨推开窗，风带着山里的凉意。远处的屋顶还藏在雾中，只有一条小路，安安静静地伸向树林。\n我把书放进背包，决定今天走得慢一些。不赶时间，也不急着寻找答案。\n路旁的树叶落了一地，脚步声轻得像是翻过一页纸。阳光穿过枝叶，在石阶上留下细碎的影子。";
  write("在路上，慢慢走.txt",sample);write("月光落在书页上.txt",sample);write("写给明天的自己.txt",sample);
  TestPrefs::entries.clear();r.scan(sd);check(r.count()==3,"Chinese filename fixture scan");r.end();
  std::filesystem::create_directories(TestFs::root+"/Font");
  for(const char* name:{"font-14.bin","font-20.bin","font-24.ui.bin","font-24.bin"})
    std::filesystem::copy_file(std::string("tools/font_builder/output/")+name,TestFs::root+"/Font/"+name,std::filesystem::copy_options::overwrite_existing);
  SdCardSession::invalidate();UiFontCache fixed;check(fixed.begin(),"UI cache open");
  for(int i=0;i<10000&&fixed.state()==UiFontCache::State::Loading;++i)fixed.tick();check(fixed.state()==UiFontCache::State::Ready,"UI cache ready");
  SdBitmapFont f14,f20,f24;check(f14.open(14),"14 open");load(f14);check(f20.open(20),"20 open");load(f20);check(f24.openUiSubset(24),"24 open");load(f24);
  DisplayMonoTft d;d.begin();TxtReaderPage page;page.enter(sd);
  {SdUiText::FixedScope scope(d,fixed);page.render(d,f14,f20,f24);save(d,"shelf");}
  page.input(false,false,false,true,sd);for(int i=0;i<10000&&page.busy();++i)page.update(f20);
  {SdUiText::FixedScope scope(d,fixed);page.render(d,f14,f20,f24);save(d,"reading");}
  page.input(false,false,false,true,sd);
  {SdUiText::FixedScope scope(d,fixed);page.render(d,f14,f20,f24);save(d,"menu");}
  page.input(false,false,true,false,sd);page.input(false,false,false,true,sd);
  {SdUiText::FixedScope scope(d,fixed);page.render(d,f14,f20,f24);save(d,"jump");}
  page.input(true,false,false,false,sd);page.input(false,false,true,false,sd);page.input(false,false,false,true,sd);
  {SdUiText::FixedScope scope(d,fixed);page.render(d,f14,f20,f24);save(d,"dark");}
  page.input(true,false,false,false,sd);check(page.input(true,false,false,false,sd)==false,"shelf Left did not exit");
  page.leave();f14.end();f20.end();f24.end();fixed.end();check(!TestFs::handles&&TestPsram::blocks.empty(),"render exit leaked resources");
  puts("PASS: directory creation, UTF8/BOM/CRLF, exact paging, resume/bookmarks/theme, modified files, SD loss, allocation errors, buttons, actual NBFT framebuffer export");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}

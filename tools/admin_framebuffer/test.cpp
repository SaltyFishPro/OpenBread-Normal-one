#include "src/ui/pages/DeviceAdminPage.h"
#include "src/bsp/DisplayMonoTft.h"
#include "src/bsp/SdCardSession.h"
#include "src/ui/SdUiText.h"
#include <FS.h>
#include <fstream>
#include <cstdio>
#include <stdexcept>
DisplayMonoTft::DisplayMonoTft():display_(SPI) {}
bool DisplayMonoTft::begin(){display_.setRotation(1);display_.clearDisplay();text_.begin(display_);return true;}
int DisplayMonoTft::width()const{return display_.getDisplayWidth();}
int DisplayMonoTft::height()const{return display_.getDisplayHeight();}
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas(){return display_;}
U8G2_FOR_ST73XX& DisplayMonoTft::text(){return text_;}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void save(DisplayMonoTft& d,const char* name){
 uint8_t raw[8064];d.canvas().copyFrameBufferTo(raw);
 std::ofstream bin(std::string("tmp/admin-framebuffer/")+name+".bin",std::ios::binary);bin.write(reinterpret_cast<char*>(raw),sizeof(raw));
 std::ofstream out(std::string("tmp/admin-framebuffer/")+name+".pgm",std::ios::binary);out<<"P5\n384 168\n255\n";
 for(int y=0;y<168;++y)for(int x=0;x<384;++x){int16_t px,py;d.canvas().logicalToPhysical(x,y,px,py);out.put(static_cast<char>((raw[(py/2)*42+px/4]&(1U<<(7-((px%4)*2+py%2))))?0:255));}
}
int main(){try{
 TestFs::root="tmp/ui-sd-font/valid";UiFontCache cache;check(cache.begin(),"cache open");
 for(int i=0;i<10000&&cache.state()==UiFontCache::State::Loading;++i)cache.tick();
 check(cache.state()==UiFontCache::State::Ready,"cache load");
 SdBitmapFont ui20,ui24;check(ui20.openUiSubset(20)&&ui24.openUiSubset(24),"resident open");
 for(auto* f:{&ui20,&ui24}){for(int i=0;i<10000&&f->state()==SdBitmapFont::State::Checking;++i)f->tick();check(f->state()==SdBitmapFont::State::Ready,"resident load");}
 DisplayMonoTft d;d.begin();SdUiText::FixedScope scope(d,cache,true);
 DeviceAdminPage::View v;v.ssid="OpenBread-Manage-1A2B";v.code="482916";
 const size_t reads=TestFs::reads;
 DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);save(d,"firmware-entry");
 v.starting=true;DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);save(d,"firmware-starting");
 v.starting=false;v.failed=true;DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);save(d,"firmware-failed");
 v.failed=false;v.active=true;DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);save(d,"firmware-ready");
 v.confirm=true;DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);save(d,"firmware-confirm");
 v.confirm=false;v.status="SD 卡未插入";DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);save(d,"firmware-no-sd");
 v.active=false;v.closed=true;DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);save(d,"firmware-closed");
 v.updating=true;DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);save(d,"firmware-updating");
 // Fully hidden and partially translated pages must preserve native clipping.
 d.canvas().clearDisplay();DeviceAdminPage::renderView(d,-168,v,&ui20,&ui24);
 uint8_t hidden[8064];d.canvas().copyFrameBufferTo(hidden);
 for(uint8_t b:hidden)check(b==0,"off-screen render wrapped coordinates");
 v.updating=false;v.closed=false;
 DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);
 uint8_t first[8064],again[8064];d.canvas().copyFrameBufferTo(first);
 DeviceAdminPage::renderView(d,0,v,&ui20,&ui24);d.canvas().copyFrameBufferTo(again);
 check(memcmp(first,again,sizeof(first))==0,"repaint is not deterministic");
 check(TestFs::reads==reads,"render read SD");check(TestFs::handles==0,"render retained SD handle");
 std::puts("PASS: production ST7305 8064-byte buffers exported; SD reads=0, open handles=0");return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}


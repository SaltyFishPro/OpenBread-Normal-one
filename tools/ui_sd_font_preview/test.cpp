#include <FS.h>
#include <esp_heap_caps.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "src/bsp/DisplayMonoTft.h"
#include "src/bsp/UsbSerial.h"
#include "src/services/UiFontCodepoints.h"
#include "src/services/TimeService.h"
#include "src/services/WifiProvisionService.h"
#include "src/services/BluetoothService.h"
#include "src/services/RemoteService.h"
#include "src/services/OtaService.h"
#include "src/ui/SdUiText.h"
#include "src/ui/PopupView.h"
#include "src/ui/pages/HomePage.h"
#include "src/ui/pages/SdUsbPage.h"
#include "src/ui/pages/ComputerStatusPage.h"
#include "src/ui/pages/TimeCalibrationPage.h"
#include "src/ui/pages/FocusClockPage.h"
#include "src/ui/pages/RemotePage.h"
#include "src/ui/pages/SettingsPage.h"
#include "src/ui/pages/AnswersBookPage.h"
#include "src/ui/pages/ReaderPage.h"
#include "src/ui/pages/SystemUpdatePage.h"
#include "src/services/SdCardService.h"

namespace {
TimeService::Snapshot clockData;
WifiProvisionService::State wifiState = WifiProvisionService::State::Idle;
BluetoothService::State btState = BluetoothService::State::Connected;
}
const TimeService::Snapshot& TimeService::snapshot() const { return clockData; }
void TimeService::cancelNtpSync() {}
bool TimeService::requestNtpSync(uint32_t,const char*,const char*,bool) { return false; }
WifiProvisionService::State WifiProvisionService::state() const { return wifiState; }
WifiProvisionService::Error WifiProvisionService::error() const { return Error::None; }
const char* WifiProvisionService::staIp() const { return "192.168.1.20"; }
const char* WifiProvisionService::targetSsid() const { return "面包 WiFi"; }
const char* WifiProvisionService::apSsid() const { return "Normal One"; }
const char* WifiProvisionService::targetPass() const { return ""; }
bool WifiProvisionService::canStartPortal() const { return true; }
void WifiProvisionService::startPortal(uint32_t) {}
bool WifiProvisionService::isRadioActive() const { return false; }
void WifiProvisionService::cancelProvision(TimeService&) {}
BluetoothService::State BluetoothService::state() const { return btState; }
BluetoothService::Error BluetoothService::error() const { return Error::None; }
const char* BluetoothService::deviceName() const { return "Normal One"; }
void BluetoothService::stop() {}
bool BluetoothService::start(uint32_t) { return false; }
RemoteService::State RemoteService::state() const { return State::Ready; }
uint32_t RemoteService::triggerCount() const { return 3; }
uint32_t RemoteService::pagerUpCount() const { return 2; }
uint32_t RemoteService::pagerDownCount() const { return 3; }
uint32_t RemoteService::pagerPauseCount() const { return 1; }
bool RemoteService::triggerCameraShutter(uint32_t,BluetoothService&) { return false; }
bool RemoteService::triggerPagerUp(uint32_t,BluetoothService&) { return false; }
bool RemoteService::triggerPagerDown(uint32_t,BluetoothService&) { return false; }
bool RemoteService::triggerPagerPause(uint32_t,BluetoothService&) { return false; }
bool UsbSerial::isEnabled() { return true; }
bool UsbSerial::setEnabled(bool enabled) { return enabled; }
OtaService::State OtaService::state() const { return State::Idle; }
bool OtaService::requestCheck(uint32_t) { return false; }
bool OtaService::requestDownload(uint32_t) { return false; }
bool OtaService::requestApply() { return false; }
void OtaService::cancel() {}
OtaService::Error OtaService::error() const { return Error::None; }
const OtaService::ManifestInfo& OtaService::manifest() const { static ManifestInfo info;return info; }
uint32_t OtaService::downloadedBytes() const { return 0; }
uint32_t OtaService::totalBytes() const { return 0; }
uint8_t OtaService::progressPercent() const { return 0; }
bool OtaService::hasPostApplyResult() const { return false; }
bool OtaService::postApplySucceeded() const { return false; }
SdCardService::Status SdCardService::refresh() {
  if (TestFs::handles) throw std::runtime_error("remount with open font file");
  return Status::Ready;
}

DisplayMonoTft::DisplayMonoTft() : display_(SPI) {}
bool DisplayMonoTft::begin() {
  display_.setRotation(1); display_.clearDisplay(); text_.begin(display_);
  text_.setFont(u8g2_font_7x14_tf); text_.setFontMode(1);
  text_.setForegroundColor(ST7305_COLOR_BLACK); text_.setBackgroundColor(ST7305_COLOR_WHITE);
  return true;
}
void DisplayMonoTft::clear() { display_.clearDisplay(); }
int DisplayMonoTft::width() const { return display_.getDisplayWidth(); }
int DisplayMonoTft::height() const { return display_.getDisplayHeight(); }
size_t DisplayMonoTft::frameBufferBytes() const { return 8064; }
void DisplayMonoTft::captureFrame(uint8_t* out) const { display_.copyFrameBufferTo(out); }
void DisplayMonoTft::restoreFrame(const uint8_t* in) { display_.copyFrameBufferFrom(in); }
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }
U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }

void check(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
void settle(UiFontCache& font) {
  for (int i=0; i<10000 && font.state()==UiFontCache::State::Loading; ++i) font.tick();
}
void root(const char* card) { TestFs::root = std::string("tmp/ui-sd-font/")+card; }
void save(DisplayMonoTft& d, const std::string& name) {
  uint8_t raw[8064]; d.captureFrame(raw);
  std::ofstream out("tmp/ui-sd-font/"+name+".pgm", std::ios::binary);
  out << "P5\n" << d.width() << " " << d.height() << "\n255\n";
  for (int16_t y=0; y<d.height(); ++y) for (int16_t x=0; x<d.width(); ++x) {
    int16_t px,py; d.canvas().logicalToPhysical(x,y,px,py);
    out.put(static_cast<char>((raw[(py/2)*42+px/4] & (1U<<(7-((px%4)*2+py%2)))) ? 0 : 255));
  }
}

int main() {
 try {
  UiFontCache cache;
  DisplayMonoTft d; d.begin();
  root("missing"); check(!cache.begin(), "missing font accepted"); cache.end();
  root("crc"); check(cache.begin(), "CRC case did not start"); settle(cache);
  check(cache.state()==UiFontCache::State::Error && !TestFs::handles && !TestPsram::used,"CRC cleanup");
  root("valid"); cache.begin();
  for (int i=0;i<1000 && !cache.cacheBytes();++i) cache.tick();
  cache.cancelLoad();
  check(!TestFs::handles && !TestPsram::used,"cancel after allocation leaked");
  for (int mode=0;mode<3;++mode) {
    TestPsram::available=mode!=0; TestPsram::failAllocation=mode==1;
    TestPsram::largest=mode==2?1024:8*1024*1024;
    cache.begin(); settle(cache);
    check(cache.state()==UiFontCache::State::Error && !TestPsram::used && !TestFs::handles,"PSRAM failure fallback");
  }
  TestPsram::available=true; TestPsram::failAllocation=false; TestPsram::largest=8*1024*1024;
  cache.begin();TestClock::nowMs+=120001;cache.tick();
  check(cache.state()==UiFontCache::State::Error && !TestFs::handles && !TestPsram::used,"timeout cleanup");
  // Measure the old one-block lookup path with this exact fixed character set.
  SdBitmapFont uncached;check(uncached.open(14,false),"baseline open");
  for (int i=0;i<10000 && uncached.state()==SdBitmapFont::State::Checking;++i)uncached.tick();
  const auto baselineStart=TestFs::reads;
  for (auto cp:UiFontCodepoints::kValues) {SdBitmapFont::Glyph glyph;uncached.glyph(cp,glyph);}
  const auto baselineReads=TestFs::reads-baselineStart;uncached.end();
  // Slow SD must still finish through the full-font fallback path (validfull
  // has no font-14.ui.bin), even past the old 20s sleep threshold and across
  // the millis() wrap. No cache changes mid-load.
  root("validfull");
  TestClock::nowMs=UINT32_MAX-1000U;TestFs::readLatencyMs=10;
  const auto loadStart=TestClock::nowMs;
  const auto readsStart=TestFs::reads;
  cache.begin(); settle(cache);
  const auto loadElapsed=TestClock::nowMs-loadStart;
  const auto loadReads=TestFs::reads-readsStart;
  TestFs::readLatencyMs=0;
  check(loadElapsed>20000U,"slow SD scenario did not cross idle threshold");
  check(loadReads<baselineReads/2,"preload did not reduce SD reads");
  std::printf("PASS: slow SD fallback survives 20s/wrap: %lums; reads=%zu vs old extraction-only=%zu; timeout cleanup\n",
              static_cast<unsigned long>(loadElapsed),loadReads,baselineReads);
  check(cache.state()==UiFontCache::State::Ready,"valid UI font not ready");
  check(cache.cacheBytes()<=64*1024 && !TestFs::handles,"UI budget or open SD handle");
  std::vector<UiFontCache::Glyph> reference;
  reference.reserve(sizeof(UiFontCodepoints::kValues)/sizeof(uint32_t));
  for (auto cp:UiFontCodepoints::kValues) {
    const auto* g=cache.glyph(cp); check(g,"required cached record missing");
    reference.push_back(*g);
  }
  cache.end();
  // Fast path: the prebuilt ui subset must yield identical glyphs with a
  // handful of sequential reads instead of a full-font CRC and random lookups.
  root("valid");
  const auto subsetReadsStart=TestFs::reads;
  cache.begin(); settle(cache);
  const auto subsetReads=TestFs::reads-subsetReadsStart;
  check(cache.state()==UiFontCache::State::Ready,"ui subset font not ready");
  check(subsetReads<64,"ui subset did not reduce SD reads");
  check(!TestFs::handles,"ui subset left SD handle open");
  for (size_t i=0;i<reference.size();++i) {
    const auto* g=cache.glyph(UiFontCodepoints::kValues[i]);
    check(g,"ui subset record missing");
    check(!memcmp(g,&reference[i],sizeof(*g)),"ui subset glyph differs from full font");
  }
  std::printf("PASS: ui subset load reads=%zu (vs full fallback %zu); %zu glyphs byte-identical\n",
              subsetReads,loadReads,reference.size());
  std::ofstream dump("tmp/ui-sd-font/glyphs.txt");
  for (auto cp:UiFontCodepoints::kValues) {
    const auto* g=cache.glyph(cp); check(g,"required cached record missing");
    if (g->length==UINT16_MAX) { dump<<cp<<" missing\n";continue; }
    dump<<cp<<' '<<g->width<<' '<<g->height<<' '<<g->xOffset<<' '<<g->yOffset<<' '<<g->advance<<' ';
    char hex[3]; for(unsigned i=0;i<g->length;++i) {std::snprintf(hex,sizeof(hex),"%02x",g->bitmap[i]);dump<<hex;}
    dump<<'\n';
  }
  dump.close();
  const auto reads=TestFs::reads;
  const auto allocations=TestPsram::allocations;
  TestFs::unavailable=true; // Equivalent to USB owning the card / card removed after load.
  d.text().setFont(u8g2_font_7x14_tf);
  const auto latinWidth=d.text().getUTF8Width("Normal One 123");
  uint8_t original[8064],current[8064];
  d.clear(); d.text().drawUTF8(3,28,"Normal One 123"); d.captureFrame(original);
  {
    SdUiText::FixedScope fixed(d,cache);
    check(d.text().getUTF8Width("Normal One 123")==latinWidth,"ASCII metrics changed");
    d.clear(); d.text().drawUTF8(3,28,"Normal One 123"); d.captureFrame(current);
    check(!memcmp(original,current,sizeof(original)),"ASCII pixels changed");
    d.clear(); d.text().drawUTF8(3,28,u8"设置 面包 123"); save(d,"reference-text");
    const auto fixedWidth=d.text().getUTF8Width(u8"面包");
    { SdUiText::Scope unavailable(d,nullptr); check(d.text().getUTF8Width(u8"面包")==fixedWidth,"empty inner scope reset UI renderer"); }
    check(d.text().getUTF8Width(u8"面包")==fixedWidth,"nested scope not restored");
    // Negative/off-screen transitions must never draw a spurious edge pixel.
    d.clear(); d.text().drawUTF8(400,28,u8"面包"); d.captureFrame(current);
    d.clear(); d.captureFrame(original); check(!memcmp(original,current,sizeof(original)),"off-screen text leaked pixels");
    d.text().setFontMode(0);d.text().setForegroundColor(ST7305_COLOR_WHITE);
    d.text().setBackgroundColor(ST7305_COLOR_BLACK);
    d.clear();d.text().drawUTF8(5,28,u8"面包 ABC");save(d,"opaque-text");
    for (uint8_t rotation=0;rotation<=1;++rotation) {
      d.canvas().setRotation(rotation);d.clear();
      d.text().drawUTF8(-8,6,u8"面包 ABC");
      d.text().drawUTF8(d.width()-12,24,u8"面包 ABC");
      d.text().drawUTF8(7,d.height()+3,u8"面包 ABC");
      save(d,"clip-"+std::to_string(rotation));
    }
    d.text().setFontMode(1);d.text().setForegroundColor(ST7305_COLOR_BLACK);
    d.text().setBackgroundColor(ST7305_COLOR_WHITE);
    HomePage home; home.begin(); HomePage::ClockData clock;
    clock.valid=true;clock.hour=12;clock.minute=34;clock.year=2026;clock.month=10;clock.day=24;clock.weekday=6;
    home.setClockData(clock); home.setBatteryData({80,true});
    for(unsigned i=0;i<13;++i) {
      if(i) {home.handleInput(false,true,false,1000*i);home.update(1000*i+500);}
      d.clear();home.render(d,0,1000*i+500);save(d,"home-"+std::to_string(i));
      check(home.focusIndex()==i,"home navigation changed");
    }
    // Loading/error notices invalidate an already cached home layer and leave
    // no pixels behind once the fixed glyphs become ready.
    d.captureFrame(original);
    check(home.setFontStatus(HomePage::FontStatus::Loading),"loading status ignored");
    d.clear();home.render(d,0,12500);d.captureFrame(current);save(d,"home-loading");
    check(memcmp(original,current,sizeof(original)),"loading notice absent");
    check(home.setFontStatus(HomePage::FontStatus::Unavailable),"error status ignored");
    d.clear();home.render(d,0,12500);save(d,"home-unavailable");
    check(home.setFontStatus(HomePage::FontStatus::Ready),"ready status ignored");
    d.clear();home.render(d,0,12500);d.captureFrame(current);
    check(!memcmp(original,current,sizeof(original)),"notice pixels remain after ready");
    check(!home.setFontStatus(HomePage::FontStatus::Ready),"unchanged status requests redraw");
    SdUsbPage usb; d.clear();usb.render(d,0,HomePage::sdUsbIcon(),true);save(d,"usb");
    ComputerStatusPage computer; computer.begin(1000); d.clear();
    computer.render(d,0,HomePage::computerStatusIcon(),false,5000);save(d,"computer");
    PopupView::ConfirmState confirm;PopupView::open(confirm,0);
    d.clear(); PopupView::drawConfirm(d,0,confirm,"确定重启设备？","取消","重启",1000);save(d,"popup");
    TimeService time;TimeCalibrationPage timePage;d.clear();
    timePage.renderDetail(0,3,0,d,time);save(d,"time");
    FocusClockPage focus;focus.handleDetailEnter(3,0);d.clear();
    focus.renderDetail(3,0,0,d,1000,time);save(d,"focus");
    BluetoothService bt;RemoteService remote;RemotePage remotePage;d.clear();
    remotePage.renderDetail(4,0,0,d,bt,remote);save(d,"bluetooth");
    for(uint8_t item=1;item<=2;++item) {
      d.canvas().setRotation(0);d.clear();remotePage.renderDetail(4,item,0,d,bt,remote);
      save(d,item==1?"camera":"pager");
    }
    d.canvas().setRotation(1);
    SettingsPage settings;WifiProvisionService wifi;
    for(auto state:{WifiProvisionService::State::Idle,WifiProvisionService::State::Connected}) {
      wifiState=state;d.clear();settings.renderDetail(0,2,0,0,d,"设备ID: 1234","Flash: 16MB","SD: ready",wifi,time);
      save(d,state==WifiProvisionService::State::Idle?"wifi-off":"wifi-on");
    }
    d.clear();settings.renderDetail(0,7,0,0,d,"设备ID: 1234","Flash: 16MB","SD: ready",wifi,time);save(d,"about");
  }
  check(TestFs::reads==reads && TestPsram::allocations==allocations,"offline UI performed SD I/O/allocations");
  check(d.text().getUTF8Width(u8"面包")==0,"outer renderer not restored");
  TestFs::unavailable=false;
  SdBitmapFont external; check(external.open(14),"external font open");
  for(int i=0;i<1000 && external.state()==SdBitmapFont::State::Checking;++i) external.tick();
  check(external.state()==SdBitmapFont::State::Ready,"external font load");
  {
    SdUiText::FixedScope fixed(d,cache);
    const auto width=d.text().getUTF8Width(u8"面包");
    { SdUiText::Scope inner(d,&external);check(d.text().getUTF8Width(u8"龘𠮷")==SdBitmapText::measureUTF8(external,u8"龘𠮷"),"external Unicode lost"); }
    check(d.text().getUTF8Width(u8"面包")==width,"content renderer did not restore fixed cache");
  }
  SdBitmapFont large;
  check(large.open(20,false),"answer20");
  for(int i=0;i<1000 && large.state()==SdBitmapFont::State::Checking;++i) large.tick();
  {
    SdUiText::FixedScope fixed(d,cache);
    AnswersBookPage answers;answers.handleDetailEnter(9,0);
    d.clear();answers.renderDetail(9,0,0,d,1000,&external,&large);save(d,"answer-prompt");
    answers.handleDetailInput(9,0,false,false,true,1000);
    answers.update(6000);answers.update(6360);
    d.clear();answers.renderDetail(9,0,0,d,7000,&external,&large);save(d,"answer");
    answers.handleDetailInput(9,0,true,false,false,7100);
    d.clear();answers.renderDetail(9,0,0,d,7500,&external,&large);save(d,"answer-history");
    SystemUpdatePage page;SystemUpdatePage::View view;
    view.state=OtaService::State::UpdateAvailable;view.version="0.1.3";
    view.notes="优化中文显示\n改善系统更新体验\n支持更多功能";
    page.prepareEnter();
    for(int i=0;i<1000;++i) {page.update(view,true,false,1000,d.text(),&external);if(!page.isPreparingLogs())break;}
    d.clear();page.render(d,0,view,1000,&external);save(d,"update");
    view.state=OtaService::State::Downloading;view.percent=52;
    d.clear();page.render(d,0,view,1500,nullptr);save(d,"update-progress");
    view.state=OtaService::State::UpdateAvailable;
    const std::string mixedNotes=std::string(u8"面包")+std::string(30,'W');
    view.notes=mixedNotes.c_str();page.prepareEnter();
    page.update(view,true,false,2000,d.text(),nullptr);
    d.clear();page.render(d,0,view,2000,nullptr);save(d,"update-offline");
    check(page.logPageCount()==1,"mixed offline notes wrapped incorrectly");
    d.clear();d.text().setFont(u8g2_font_7x14_tf);d.text().setFontMode(1);
    d.text().setForegroundColor(ST7305_COLOR_BLACK);
    d.text().drawUTF8(164,86,(std::string(u8"面包")+std::string(14,'W')).c_str());
    d.text().drawUTF8(164,108,std::string(16,'W').c_str());save(d,"update-offline-reference");
    external.end();large.end();
    ReaderService reader;reader.begin();ReaderPage readerPage;SdCardService sd;
    readerPage.handleDetailEnter(2,0,external);
    readerPage.handleDetailInput(2,0,false,false,false,false,true,reader,sd,external);
    for(int i=0;i<1000 && external.state()==SdBitmapFont::State::Checking;++i)external.tick();
    check(large.open(24,false),"reader24");
    for(int i=0;i<1000 && large.state()==SdBitmapFont::State::Checking;++i)large.tick();
    for(int i=0;i<1000;++i)if(readerPage.updateMeaning(reader,external))break;
    d.clear();readerPage.renderDetail(2,0,0,d,reader,external,&large);save(d,"reader");
    reader.reset();
  }
  external.end();large.end();cache.end();
  check(!TestFs::handles && !TestPsram::used,"final leak");
  std::printf("PASS: cached glyphs, 13 home entries, portrait/USB/popups/settings; offline reads=0; peak PSRAM=%zu; errors/cancel/retry/nested scopes/cleanup\n",TestPsram::peak);
  return 0;
 } catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}

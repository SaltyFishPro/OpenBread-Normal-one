#include "src/bsp/SdCardSession.h"
#include "src/app/FirmwareInfo.h"
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
#include "src/ui/pages/TimerPage.h"
#include "src/ui/pages/RemotePage.h"
#include "src/ui/pages/SettingsPage.h"
#include "src/ui/pages/WifiPage.h"
#include "src/ui/pages/AnswersBookPage.h"
#include "src/ui/pages/ReaderPage.h"
#include "src/ui/pages/SystemUpdatePage.h"
#include "src/services/SdCardService.h"

namespace {
TimeService::Snapshot clockData;
WifiProvisionService::State wifiState = WifiProvisionService::State::Idle;
unsigned timeRequests = 0, timeCancels = 0;
BluetoothService::State btState = BluetoothService::State::Connected;
}
const TimeService::Snapshot& TimeService::snapshot() const { return clockData; }
void TimeService::cancelNtpSync() { ++timeCancels; }
bool TimeService::requestNtpSync(uint32_t,const char*,const char*,bool) { ++timeRequests; return false; }
WifiProvisionService::State WifiProvisionService::state() const { return wifiState; }
WifiProvisionService::Error WifiProvisionService::error() const { return Error::None; }
const char* WifiProvisionService::staIp() const { return "192.168.1.20"; }
const char* WifiProvisionService::targetSsid() const { return "面包 WiFi"; }
const char* WifiProvisionService::apSsid() const { return "OpenBread-Setup-1A2B"; }
const char* WifiProvisionService::targetPass() const { return ""; }
bool WifiProvisionService::canStartPortal() const { return true; }
bool WifiProvisionService::isPortalActive() const {
  return wifiState == State::PortalReady || wifiState == State::Connecting ||
         wifiState == State::Connected || wifiState == State::Failed;
}
bool WifiProvisionService::canRetryConnect() const { return wifiState == State::Failed; }
bool WifiProvisionService::retryConnect() { return false; }
uint32_t WifiProvisionService::holdRemainingSeconds(uint32_t) const { return 0; }
uint32_t WifiProvisionService::portalRemainingSeconds(uint32_t) const { return 0; }
void WifiProvisionService::startPortal(uint32_t, Purpose, int8_t, const char*) { wifiState=State::PortalReady; }
bool WifiProvisionService::connectSaved(uint8_t index) {
  if (!networks_.credentials(index,targetSsid_,sizeof(targetSsid_),targetPass_,sizeof(targetPass_))) return false;
  fromSaved_=true;wifiState=State::Connecting;return true;
}
bool WifiProvisionService::forgetNetwork(uint8_t index) { return networks_.forget(index); }
bool WifiProvisionService::editPortal() { wifiState=State::PortalReady;return true; }
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
void root(const char* card) { SdCardSession::invalidate(); TestFs::root = std::string("tmp/ui-sd-font/")+card; }
void save(DisplayMonoTft& d, const std::string& name) {
  uint8_t raw[8064]; d.captureFrame(raw);
  std::ofstream out("tmp/ui-sd-font/"+name+".pgm", std::ios::binary);
  out << "P5\n" << d.width() << " " << d.height() << "\n255\n";
  for (int16_t y=0; y<d.height(); ++y) for (int16_t x=0; x<d.width(); ++x) {
    int16_t px,py; d.canvas().logicalToPhysical(x,y,px,py);
    out.put(static_cast<char>((raw[(py/2)*42+px/4] & (1U<<(7-((px%4)*2+py%2)))) ? 0 : 255));
  }
}

bool framePixel(const uint8_t* frame, DisplayMonoTft& display, int16_t x, int16_t y) {
  int16_t px, py;
  display.canvas().logicalToPhysical(x, y, px, py);
  return (frame[(py / 2) * 42 + px / 4] & (1U << (7 - ((px % 4) * 2 + py % 2)))) != 0;
}

bool rowHasInk(const uint8_t* frame, DisplayMonoTft& display, int16_t top, int16_t bottom,
               int16_t left, int16_t right) {
  for (int16_t y = top; y <= bottom; ++y) {
    for (int16_t x = left; x < right; ++x) {
      if (framePixel(frame, display, x, y)) return true;
    }
  }
  return false;
}

void residentSubsetTests() {
  root("valid");
  for(uint16_t size : {20,24}) {
    SdBitmapFont resident, full;
    const size_t loadReads=TestFs::reads;
    check(resident.openUiSubset(size),"resident open");
    for(int i=0;i<1000 && resident.state()==SdBitmapFont::State::Checking;++i)resident.tick();
    check(resident.state()==SdBitmapFont::State::Ready && resident.residentBytes() &&
          resident.residentBytes()<=128*1024 && !TestFs::handles,"resident budget/handle");
    const size_t subsetReads=TestFs::reads-loadReads;
    check(full.open(size,false),"resident reference open");
    for(int i=0;i<1000 && full.state()==SdBitmapFont::State::Checking;++i)full.tick();
    check(full.state()==SdBitmapFont::State::Ready,"resident reference ready");
    for(uint32_t cp:UiFontCodepoints::kValues) {
      SdBitmapFont::Glyph a,b;
      const bool found=resident.glyph(cp,a);
      check(found==full.glyph(cp,b),"resident coverage differs from full font");
      if(!found)continue;
      check(a.width==b.width && a.height==b.height && a.xOffset==b.xOffset &&
            a.yOffset==b.yOffset && a.advance==b.advance,"resident metrics differ");
      for(uint16_t row=0;row<a.height;++row) {
        uint8_t ar[128],br[128];
        check(resident.readRow(a,row,ar,sizeof(ar)) && full.readRow(b,row,br,sizeof(br)) &&
              !memcmp(ar,br,(a.width+7)/8),"resident pixels differ");
      }
    }
    full.end();
    const size_t reads=TestFs::reads, allocations=TestPsram::allocations;
    SdCardSession::invalidate();
    TestFs::unavailable=true;
    for(uint32_t cp:UiFontCodepoints::kValues) {
      SdBitmapFont::Glyph glyph;
      if(!resident.glyph(cp,glyph))continue;
      uint8_t row[128];
      for(uint16_t y=0;y<glyph.height;++y)check(resident.readRow(glyph,y,row,sizeof(row)),"offline resident row");
    }
    check(TestFs::reads==reads && TestPsram::allocations==allocations && !TestFs::handles,
          "resident offline rendering reads/allocates");
    TestFs::unavailable=false;
    resident.end();
    TestPsram::failAllocation=true;
    check(resident.openUiSubset(size),"resident allocation fallback open");
    for(int i=0;i<1000 && resident.state()==SdBitmapFont::State::Checking;++i)resident.tick();
    SdBitmapFont::Glyph glyph;
    check(resident.state()==SdBitmapFont::State::Ready && !resident.residentBytes() &&
          resident.glyph('A',glyph),"resident allocation fallback lost font");
    resident.end();TestPsram::failAllocation=false;
    check(resident.openUiSubset(size),"resident cancel setup");
    resident.end();resident.tick();
    check(!TestPsram::used && !TestFs::handles,"resident cancel leaks");
    check(resident.openUiSubset(size),"resident session change setup");
    SdCardSession::invalidate();resident.tick();
    check(resident.state()==SdBitmapFont::State::Error && !TestPsram::used && !TestFs::handles,
          "resident partial load survived SD change");
    resident.end();
    root("crc");
    check(resident.openUiSubset(size),"resident CRC fixture open");
    for(int i=0;i<1000 && resident.state()==SdBitmapFont::State::Checking;++i)resident.tick();
    check(resident.error()==SdBitmapFont::Error::Crc && !TestPsram::used && !TestFs::handles,
          "resident bad CRC published/leaked");
    resident.end();root("valid");
    printf("PASS: resident %u all UI glyphs match full font; offline reads=0; sequential load reads=%zu; fallback/cancel/session cleanup\n",size,subsetReads);
  }
}

int main() {
 try {
  residentSubsetTests();
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
    static const char* const settingsItems[] = {"重启设备", "系统更新", "WiFi配网", "时间校准",
                                                 "设备自检", "USB串口", "恢复默认设置", "关于设备", "关于作者"};
    static const char* const musicItems[] = {"音乐列表", "收藏歌曲"};
    static const char* const readerItems[] = {"单词阅读", "提词器"};
    static const char* const focusItems[] = {"专注时钟"};
    static const char* const wirelessItems[] = {"蓝牙连接", "蓝牙拍照", "翻页器"};
    static const char* const gamesItems[] = {"敲木鱼", "飞机世界大战"};
    HomePage motion; motion.begin(); motion.setClockData(clock);
    motion.setBatteryData({80,true}); motion.setSubmenuLabels(settingsItems,9);
    uint8_t motionStart[8064], motionMiddle[8064];
    d.clear(); motion.render(d,0,2000); d.captureFrame(motionStart);
    motion.handleInput(false,true,false,false,false,2000);
    d.clear(); motion.render(d,0,2000); d.captureFrame(current);
    check(!memcmp(motionStart,current,sizeof(current)),"category slide changed its start frame");
    d.clear(); motion.render(d,0,2120); d.captureFrame(motionMiddle);
    save(d,"home-category-slide");
    check(motion.isSliding() && motion.focusIndex()==0,"category changed before slide end");
    check(memcmp(motionStart,motionMiddle,sizeof(motionMiddle)),"category slide lacks middle frame");
    motion.update(2241); motion.setSubmenuLabels(musicItems,2);
    check(!motion.isSliding() && motion.focusIndex()==1,"category slide did not settle");
    d.clear(); motion.render(d,0,3000); d.captureFrame(motionStart);
    motion.handleInput(false,false,false,true,false,3000);
    d.clear(); motion.render(d,0,3000); d.captureFrame(current);
    check(!memcmp(motionStart,current,sizeof(current)),"submenu slide changed its start frame");
    d.clear(); motion.render(d,0,3120); d.captureFrame(motionMiddle);
    save(d,"home-submenu-slide");
    check(motion.isSliding() && motion.submenuIndex()==1,"submenu target lost during slide");
    check(memcmp(motionStart,motionMiddle,sizeof(motionMiddle)),"submenu slide lacks middle frame");
    for (int16_t y=88;y<98;++y) for (int16_t x=24;x<240;++x)
      check(framePixel(motionMiddle,d,x,y),"submenu text crossed category rule");
    motion.update(3241);
    check(!motion.isSliding() && motion.submenuIndex()==1,"submenu slide did not settle");
    d.clear(); motion.render(d,0,3241); save(d,"home-submenu-settled");
    motion.handleInput(false,false,true,false,false,4000);
    d.clear(); motion.render(d,0,4120); d.captureFrame(motionMiddle);
    check(motion.isSliding() && motion.submenuIndex()==0,"up slide lost its target");
    motion.update(4241);
    check(!motion.isSliding() && motion.submenuIndex()==0,"up slide did not settle");
    motion.handleInput(true,false,false,false,false,5000);
    d.clear(); motion.render(d,0,5120); d.captureFrame(motionMiddle);
    check(motion.isSliding() && motion.focusIndex()==1,"left slide changed focus early");
    motion.update(5241);
    check(!motion.isSliding() && motion.focusIndex()==0,"left slide did not return to settings");
    motion.setSubmenuLabels(settingsItems,9);
    motion.handleInput(false,true,false,false,false,6000);
    motion.handleInput(true,false,false,false,false,6060);
    motion.update(6301);
    check(motion.focusIndex()==0 && !motion.isSliding(),"rapid reverse lost category focus");
    SettingsPage transitionSettings;
    WifiProvisionService transitionWifi;
    TimeService transitionTime;
    const auto drawAboutTransition = [&](int16_t yOffset) {
      d.clear(); motion.render(d,0,7000);
      if (yOffset < d.height())
        d.canvas().drawFilledRectangle(0,yOffset,d.width()-1,d.height()-1,ST7305_COLOR_WHITE);
      transitionSettings.renderDetail(0,SettingsPage::kAboutDeviceItemIndex,0,yOffset,d,
                                      "设备ID: 1234","Flash: 16MB","SD: ready",
                                      transitionWifi,transitionTime);
    };
    d.clear(); motion.render(d,0,7000); d.captureFrame(motionStart);
    drawAboutTransition(d.height()); d.captureFrame(current);
    check(!memcmp(motionStart,current,sizeof(current)),"detail entry changed XMB start frame");
    drawAboutTransition(d.height()/2); d.captureFrame(motionMiddle);
    save(d,"home-detail-transition");
    for (int16_t y=0;y<d.height()/2;++y) for (int16_t x=0;x<d.width();++x)
      check(framePixel(motionStart,d,x,y)==framePixel(motionMiddle,d,x,y),
            "detail entry exposed a different background");
    check(memcmp(motionStart,motionMiddle,sizeof(motionMiddle)),"detail page did not enter");
    drawAboutTransition(0); d.captureFrame(current);
    check(memcmp(motionStart,current,sizeof(current)),"detail endpoint still shows XMB");
    PopupView::ConfirmState homeConfirm;
    PopupView::open(homeConfirm,7000);
    d.clear(); motion.render(d,0,7200);
    PopupView::drawConfirm(d,0,homeConfirm,"确定重启设备？","取消","重启",7200,true);
    d.captureFrame(current); save(d,"home-confirm");
    check(memcmp(motionStart,current,sizeof(current)),"XMB confirm popup not visible");
    PopupView::handleInput(homeConfirm,true,false,false,false,false,7300);
    check(PopupView::update(homeConfirm,7500)==PopupView::Result::Cancelled,
          "XMB confirm cancel did not return");
    check(motion.focusIndex()==0 && motion.submenuIndex()==0,
          "XMB focus changed while showing confirmation");
    check(transitionSettings.popupForSelection(0,0)==SettingsPage::ConfirmKind::Restart,
          "XMB restart confirmation mapping lost");
    for (uint8_t i=0;i<6;++i) {
      const uint32_t stepMs=8000U+static_cast<uint32_t>(i)*300U;
      motion.handleInput(false,false,false,true,false,stepMs);
      motion.update(stepMs+250U);
    }
    check(motion.submenuIndex()==SettingsPage::kFactoryResetItemIndex &&
          transitionSettings.popupForSelection(0,motion.submenuIndex())==
              SettingsPage::ConfirmKind::FactoryReset,
          "XMB factory reset confirmation mapping lost");
    for(unsigned i=0;i<13;++i) {
      if(i) {home.handleInput(false,true,false,false,false,1000*i);home.update(1000*i+500);}
      const char* const* labels = nullptr; uint8_t count = 0;
      switch (i) {
        case 0: labels = settingsItems; count = 9; break;
        case 1: labels = musicItems; count = 2; break;
        case 2: labels = readerItems; count = 2; break;
        case 3: labels = focusItems; count = 1; break;
        case 4: labels = wirelessItems; count = 3; break;
        case 5: labels = gamesItems; count = 2; break;
        default: break;
      }
      home.setSubmenuLabels(labels, count);
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
    uint8_t sleepFrame[8064];
    d.clear(); home.renderSleepClock(d, 12000); d.captureFrame(sleepFrame); save(d, "home-sleep");
    // The GIF's black background must match the sleep clock's black canvas.
    for (const auto& point : {std::pair<int16_t, int16_t>{5, 30}, {138, 30},
                              {5, 162}, {138, 162}, {150, 30}}) {
      check(framePixel(sleepFrame, d, point.first, point.second),
            "sleep emoji background became white");
    }
    d.clear(); home.renderSleepTransition(d, 12000, 0, original, sleepFrame); d.captureFrame(current);
    check(!memcmp(current, original, sizeof(original)), "sleep transition start differs from home");
    for (uint16_t progress : {30U, 500U, 750U}) {
      d.clear(); home.renderSleepTransition(d, 12000, progress, original, sleepFrame); d.captureFrame(current);
      if (progress == 500U) save(d, "home-sleep-transition");
      const int16_t lineX = static_cast<int16_t>(
          (static_cast<uint32_t>(progress) * d.width() + 500U) / 1000U);
      const int16_t shiftX = static_cast<int16_t>(
          (static_cast<uint32_t>(progress) * 256U + 500U) / 1000U);
      for (int16_t y = 0; y < d.height(); ++y) {
        for (int16_t x = 0; x < d.width(); ++x) {
          if (x < lineX) check(framePixel(current, d, x, y) == framePixel(sleepFrame, d, x, y),
                               "sleep reveal differs from clock");
          if (x > lineX) check(framePixel(current, d, x, y) ==
                               framePixel(original, d, static_cast<int16_t>(x - shiftX), y),
                               "home pixels did not shift with scan line");
        }
      }
    }
    d.clear(); home.renderSleepTransition(d, 12000, 1000, original, sleepFrame); d.captureFrame(current);
    check(!memcmp(current, sleepFrame, sizeof(sleepFrame)), "sleep transition end differs from clock");
    SdUsbPage usb; d.clear();usb.render(d,0,HomePage::sdUsbIcon(),true);save(d,"usb");
    ComputerStatusPage computer; computer.begin(1000); d.clear();
    computer.render(d,0,HomePage::computerStatusIcon(),false,5000);save(d,"computer");
    PopupView::ConfirmState confirm;PopupView::open(confirm,0);
    d.clear(); PopupView::drawConfirm(d,0,confirm,"确定重启设备？","取消","重启",1000);save(d,"popup");
    TimeService time;TimeCalibrationPage timePage;d.clear();
    timePage.renderDetail(0,3,0,d,time);save(d,"time");
    FocusClockPage focus;focus.handleDetailEnter(3,0);d.clear();
    focus.renderDetail(3,0,0,d,1000,time);save(d,"focus");
    TimerPage timer;timer.handleDetailEnter(6,0);
    d.clear();timer.renderDetail(6,0,0,d,0,nullptr);save(d,"timer-preset");
    timer.handleDetailInput(6,0,false,true,false,false,false,100);
    d.clear();timer.renderDetail(6,0,0,d,100,nullptr);save(d,"timer-custom");
    timer.handleDetailInput(6,0,false,false,false,false,true,200);
    d.clear();timer.renderDetail(6,0,0,d,1200,nullptr);save(d,"timer-running");
    timer.handleDetailBack(6,0,1300);
    d.clear();timer.renderDetail(6,0,0,d,1400,nullptr);save(d,"timer-popup");
    BluetoothService bt;RemoteService remote;RemotePage remotePage;d.clear();
    remotePage.renderDetail(4,0,0,d,bt,remote);save(d,"bluetooth");
    for(uint8_t item=1;item<=2;++item) {
      d.canvas().setRotation(0);d.clear();remotePage.renderDetail(4,item,0,d,bt,remote);
      save(d,item==1?"camera":"pager");
    }
    d.canvas().setRotation(1);
    SettingsPage settings;WifiProvisionService wifi;
    check(settings.detailPageCount(0,7)==2,"about page count");
    d.clear();settings.renderDetail(0,7,0,0,d,"设备ID：123456789ABC","Flash空间：16 MB","SD卡：3.6G可用/3.8G总容量",wifi,time);save(d,"about");
    uint8_t aboutFrame[8064], versionFrame[8064]; d.captureFrame(aboutFrame);
    check(framePixel(aboutFrame,d,0,0),"about background not black");
    d.clear();d.text().setFont(u8g2_font_7x14_tf);d.text().setFontMode(1);
    d.text().setForegroundColor(ST7305_COLOR_WHITE);d.text().setBackgroundColor(ST7305_COLOR_BLACK);
    d.canvas().fill(0xFF);
    char expectedVersion[48]; snprintf(expectedVersion,sizeof(expectedVersion),"V%s",FirmwareInfo::kVersion);
    d.text().drawUTF8(26,129,expectedVersion);d.captureFrame(versionFrame);
    for(int y=115;y<=132;++y)for(int x=26;x<155;++x)
      check(framePixel(aboutFrame,d,x,y)==framePixel(versionFrame,d,x,y),"about firmware version differs from FirmwareInfo");
    d.clear();settings.renderDetail(0,7,1,0,d,"设备ID：123456789ABC","Flash空间：16 MB","SD卡：3.6G可用/3.8G总容量",wifi,time);save(d,"about-2");
    d.captureFrame(versionFrame);check(memcmp(aboutFrame,versionFrame,sizeof(aboutFrame)),"about pages identical");
    wifiState=WifiProvisionService::State::Connected;
    d.clear();settings.renderDetail(0,7,1,0,d,"设备ID：123456789ABC","Flash空间：16 MB","SD卡：3.6G可用/3.8G总容量",wifi,time);
    d.captureFrame(aboutFrame);check(!memcmp(aboutFrame,versionFrame,sizeof(aboutFrame)),"about displays WiFi state");
    d.clear();settings.renderDetail(0,7,1,0,d,"设备ID：123456789ABC","Flash空间：16 MB","SD卡：未插入",wifi,time);save(d,"about-no-sd");
    wifiState=WifiProvisionService::State::Idle;
    printf("PASS: about cards, current firmware version pixels, SD states and no IP\n");
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
  SdBitmapFont ui20, ui24;
  check(ui20.openUiSubset(20) && ui24.openUiSubset(24),"resident subsets open");
  for(int i=0;i<1000;++i){ui20.tick();ui24.tick();}
  check(ui20.residentBytes() && ui24.residentBytes(),"subsets not resident");
  check(ui20.state()==SdBitmapFont::State::Ready && ui24.state()==SdBitmapFont::State::Ready,"resident subsets not ready");
  SdBitmapFont large;
  check(large.open(20),"answer20");
  for(int i=0;i<1000 && large.state()==SdBitmapFont::State::Checking;++i) large.tick();
  {
    SdUiText::FixedScope fixed(d,cache);
    WifiProvisionService wifi; TimeService wifiTime;
    auto& networks=const_cast<WifiNetworkStore&>(wifi.networks());
    check(networks.save("OpenBread-Home","pw",{2026,11,13,9,30}),"seed history");
    check(networks.save(u8"工作室 WiFi","pw",{2026,11,12,18,20}),"seed unicode");
    check(networks.save("XiaoYu-Hotspot","pw",{2026,11,11,12,0}),"seed hotspot");
    TimeCalibrationPage calibration;
    clockData = {};
    clockData.valid = true; clockData.now.year = 2026; clockData.now.month = 11;
    clockData.now.day = 14; clockData.now.hour = 21; clockData.now.minute = 36;
    for (const auto state : {TimeService::SyncState::Idle, TimeService::SyncState::Syncing,
                             TimeService::SyncState::Success, TimeService::SyncState::Failed}) {
      clockData.syncState = state;
      clockData.error = state == TimeService::SyncState::Failed ? TimeService::Error::NtpTimeout : TimeService::Error::None;
      const char* name = state == TimeService::SyncState::Idle ? "time-idle" :
          state == TimeService::SyncState::Syncing ? "time-syncing" :
          state == TimeService::SyncState::Success ? "time-success" : "time-failed";
      const size_t reads = TestFs::reads;
      d.clear(); check(calibration.renderDetail(0,3,0,d,wifiTime,&ui24),"time page selection");
      save(d,name);
      uint8_t frame[8064]; d.captureFrame(frame);
      check(framePixel(frame,d,0,0) && framePixel(frame,d,383,167),"time page black background/header");
      check(TestFs::reads == reads,"time page read SD");
    }
    clockData.valid = false; clockData.syncState = TimeService::SyncState::Idle;
    clockData.error = TimeService::Error::None;
    d.clear();calibration.renderDetail(0,3,0,d,wifiTime,&ui24);save(d,"time-invalid");
    check(calibration.handleDetailInput(0,3,false,false,true,100,wifiTime,wifi) && timeRequests==1,
          "time OK did not request sync");
    clockData.syncState = TimeService::SyncState::Syncing;
    check(calibration.handleDetailInput(0,3,false,false,true,101,wifiTime,wifi) && timeCancels==1,
          "time OK did not cancel sync");
    check(!calibration.handleDetailBack(0,3,wifiTime) && timeCancels==2,
          "time Left did not cancel and return");
    clockData = {};
    printf("PASS: time calibration state frames, zero SD reads and OK/Left interactions\n");
    SettingsPage aboutPage;
    const size_t aboutReads=TestFs::reads;
    d.clear();aboutPage.renderDetail(0,7,0,0,d,"设备ID：123456789ABC","Flash空间：16 MB","SD卡：3.6G可用/3.8G总容量",wifi,wifiTime,&ui20);save(d,"about-resident20");
    uint8_t heroFrame[8064];d.captureFrame(heroFrame);
    check(!framePixel(heroFrame,d,20,52),"about hero background not white");
    check(framePixel(heroFrame,d,33,52),"about hero icon not black");
    size_t titleInk=0;
    for(int y=40;y<63;++y)for(int x=72;x<300;++x)
      if(framePixel(heroFrame,d,x,y))++titleInk;
    check(titleInk>50,"about hero title not black");
    check(TestFs::reads==aboutReads,"about cards read SD");
    WifiPage wifiPage;
    wifiState=WifiProvisionService::State::Idle;
    wifiPage.prepareEnter(wifi);wifiPage.beginFonts(ui20,ui24);
    for(int i=0;i<1000;++i)wifiPage.update(wifi);
    auto renderWifi=[&](const char* name) { d.clear();wifiPage.render(d,0,wifi,wifiTime,&external,&large);save(d,name); };
    check(wifiPage.needsFullFont20(wifi) && !wifiPage.needsFullFont14(wifi),"history full-font demand");
    renderWifi("wifi-history");
    const size_t historyReads=TestFs::reads;
    renderWifi("wifi-history");
    check(TestFs::reads==historyReads,"warm network list still reads SD");
    uint8_t historyFrame[8064];d.captureFrame(historyFrame);
    check(framePixel(historyFrame,d,0,0),"WiFi page background is not black");
    check(!framePixel(historyFrame,d,30,37),"WiFi focus capsule missing");
    wifiPage.handleInput(false,false,false,true,1000,wifi,wifiTime);renderWifi("wifi-detail");
    wifiPage.handleInput(false,false,true,false,1001,wifi,wifiTime);
    wifiPage.handleInput(false,false,false,true,1002,wifi,wifiTime);renderWifi("wifi-forget-confirm");
    check(wifiPage.needsFullFont14(wifi),"forget network needs full font");
    check(networks.count()==3,"forget before confirm");
    wifiPage.handleInput(false,false,false,true,1003,wifi,wifiTime);
    check(networks.count()==3,"default confirmation was destructive");
    wifiPage.handleInput(true,false,false,false,1004,wifi,wifiTime);
    for(int i=0;i<3;++i)wifiPage.handleInput(false,false,true,false,1005+i,wifi,wifiTime);
    renderWifi("wifi-add-selected");
    wifiPage.handleInput(false,false,false,true,1010,wifi,wifiTime);
    const size_t confirmReads=TestFs::reads;
    renderWifi("wifi-start-confirm");
    check(TestFs::reads==confirmReads,"fixed start dialog still reads SD");
    uint8_t confirmBefore[8064],confirmPartial[8064],confirmFull[8064];
    d.captureFrame(confirmBefore);
    wifiPage.handleInput(false,false,true,false,1011,wifi,wifiTime);
    check(wifiPage.renderConfirmationFocusOnly(d),"focus-only render not selected");
    check(TestFs::reads==confirmReads,"focus-only dialog reads SD");
    d.captureFrame(confirmPartial);save(d,"wifi-start-selected");
    check(memcmp(confirmBefore,confirmPartial,sizeof(confirmPartial)),"focus did not change");
    for(int16_t y=0;y<168;++y)for(int16_t x=0;x<384;++x)
      if(x<24||x>243||y<101||y>128)
        check(framePixel(confirmBefore,d,x,y)==framePixel(confirmPartial,d,x,y),"focus changed static text");
    renderWifi("wifi-start-selected-full");d.captureFrame(confirmFull);
    check(!memcmp(confirmPartial,confirmFull,sizeof(confirmFull)),"partial dialog differs from full frame");
    check(TestFs::reads==confirmReads,"repeated dialog still reads SD");
    wifiPage.handleInput(false,false,false,true,1012,wifi,wifiTime);
    check(wifiState==WifiProvisionService::State::Idle,"hotspot started before busy frame");
    check(!wifiPage.afterPresent(1012,wifi),"hotspot started without busy render");
    renderWifi("wifi-starting");
    check(wifiState==WifiProvisionService::State::Idle,"busy render enabled radio");
    check(wifiPage.afterPresent(1013,wifi)&&wifiState==WifiProvisionService::State::PortalReady,"hotspot not started after presentation");
    check(!wifiPage.afterPresent(1014,wifi),"hotspot started twice");
    printf("PASS: WiFi fixed dialog/focus SD reads=0; partial frame matches full; hotspot deferred until presentation\n");
    for(const auto& entry:std::vector<std::pair<WifiProvisionService::State,const char*>>{
        {WifiProvisionService::State::PortalReady,"wifi-portal"},
        {WifiProvisionService::State::Connecting,"wifi-connecting"},
        {WifiProvisionService::State::Failed,"wifi-failed"},
        {WifiProvisionService::State::PortalTimeout,"wifi-timeout"},
        {WifiProvisionService::State::SyncingTime,"wifi-syncing"},
        {WifiProvisionService::State::Connected,"wifi-connected"}}) {
      wifiState=entry.first;wifiPage.update(wifi);renderWifi(entry.second);
      if(entry.first==WifiProvisionService::State::PortalReady) {
        uint8_t frame[8064];d.captureFrame(frame);
        check(framePixel(frame,d,270,43),"QR top finder missing");
        check(!framePixel(frame,d,258,93),"QR quiet zone missing");
        check(framePixel(frame,d,250,93),"QR leaked outside white frame");
      }
    }
    // Dynamic network text is the only reason to request full fonts.
    wifiState=WifiProvisionService::State::Connecting;
    check(wifiPage.needsFullFont14(wifi),"connecting lacks dynamic font");
    // Empty and full-capacity lists use the same production input/render path.
    while(networks.count())check(networks.forget(0),"clear history");
    wifiState=WifiProvisionService::State::Idle;wifiPage.prepareEnter(wifi);
    check(!wifiPage.needsFullFont14(wifi) && !wifiPage.needsFullFont20(wifi),"empty WiFi requested full fonts");
    const size_t emptyReads=TestFs::reads;
    renderWifi("wifi-empty");
    wifiPage.endFonts();wifiPage.beginFonts(ui20,ui24);
    renderWifi("wifi-empty-reentry");
    check(TestFs::reads==emptyReads,"empty/reentry WiFi reads SD");
    wifiPage.handleInput(false,false,false,true,1100,wifi,wifiTime);
    wifiPage.handleInput(false,false,false,true,1101,wifi,wifiTime);
    check(networks.count()==0&&wifiState==WifiProvisionService::State::Idle,"empty add default did not cancel");
    check(networks.save(std::string(32,'W').c_str(),"pw",{}),"seed long SSID");
    for(int i=0;i<4;++i){char name[20];snprintf(name,sizeof(name),"Saved-%d",i);check(networks.save(name,"pw",{}),"fill history");}
    wifiPage.prepareEnter(wifi);
    for(int i=0;i<5;++i)wifiPage.handleInput(false,false,true,false,1110+i,wifi,wifiTime);
    renderWifi("wifi-full-add");
    wifiPage.handleInput(false,false,false,true,1120,wifi,wifiTime);
    renderWifi("wifi-replace");
    wifiPage.handleInput(false,false,false,true,1121,wifi,wifiTime);
    renderWifi("wifi-replace-confirm");
    wifiPage.handleInput(false,false,false,true,1122,wifi,wifiTime);
    check(networks.count()==5,"replacement default deleted a record");
    wifiPage.handleInput(true,false,false,false,1123,wifi,wifiTime);
    for(int i=0;i<4;++i)wifiPage.handleInput(false,true,false,false,1124+i,wifi,wifiTime);
    wifiPage.handleInput(false,false,false,true,1130,wifi,wifiTime);
    wifiPage.handleInput(false,false,true,false,1131,wifi,wifiTime);
    wifiPage.handleInput(false,false,false,true,1132,wifi,wifiTime);
    wifiPage.handleInput(false,false,true,false,1133,wifi,wifiTime);
    wifiPage.handleInput(false,false,false,true,1134,wifi,wifiTime);
    check(networks.count()==4,"confirmed forget did not remove exactly one record");
    wifiPage.endFonts();
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
    TimerPage timerDone;timerDone.handleDetailEnter(6,0);
    timerDone.handleDetailInput(6,0,false,false,false,false,true,0);
    check(timerDone.update(61000),"timer did not finish");
    d.clear();timerDone.renderDetail(6,0,0,d,61000,&large);save(d,"timer-done");
    uint8_t timerFull[8064],timerSubset[8064];d.captureFrame(timerFull);
    const size_t timerReads=TestFs::reads;
    d.clear();timerDone.renderDetail(6,0,0,d,61000,&ui24);d.captureFrame(timerSubset);
    check(!memcmp(timerFull,timerSubset,sizeof(timerFull)) && TestFs::reads==timerReads,
          "timer resident title differs or reads SD");
    reader.reset();
  }
  external.end();large.end();ui20.end();ui24.end();cache.end();
  check(!TestFs::handles && !TestPsram::used,"final leak");
  std::printf("PASS: cached glyphs, 13 home entries, portrait/USB/popups/settings; offline reads=0; peak PSRAM=%zu; errors/cancel/retry/nested scopes/cleanup\n",TestPsram::peak);
  return 0;
 } catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}

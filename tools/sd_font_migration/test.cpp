#include <FS.h>
#include <esp_heap_caps.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include "src/bsp/DisplayMonoTft.h"
#include "src/services/SdCardService.h"
#include "src/services/SdBitmapFont.h"
#include "src/ui/SdUiText.h"
#define private public
#include "src/ui/pages/AnswersBookPage.h"
#include "src/ui/pages/SystemUpdatePage.h"
#undef private
#include "src/ui/pages/AnswersBookData.h"
#include "src/ui/pages/ReaderPage.h"

OtaService::State OtaService::state() const { return State::Idle; }
OtaService::Error OtaService::error() const { return Error::None; }
const OtaService::ManifestInfo& OtaService::manifest() const {
  static ManifestInfo info;
  return info;
}
uint32_t OtaService::downloadedBytes() const { return 0; }
uint32_t OtaService::totalBytes() const { return 0; }
uint8_t OtaService::progressPercent() const { return 0; }
bool OtaService::hasPostApplyResult() const { return false; }
bool OtaService::postApplySucceeded() const { return false; }

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
void DisplayMonoTft::captureFrame(uint8_t* out) const { display_.copyFrameBufferTo(out); }
ST7305_2p9_BW_DisplayDriver& DisplayMonoTft::canvas() { return display_; }
U8G2_FOR_ST73XX& DisplayMonoTft::text() { return text_; }
SdCardService::Status SdCardService::refresh() {
  if (TestFs::handles) throw std::runtime_error("remount with open font handles");
  return Status::Ready;
}
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void settle(SdBitmapFont& font) {
  for (int i=0; i<10000 && font.state()==SdBitmapFont::State::Checking; ++i) font.tick();
  check(font.state()==SdBitmapFont::State::Ready, "font did not load");
}
void save(DisplayMonoTft& display, const std::string& name) {
  uint8_t raw[8064]; display.captureFrame(raw);
  std::ofstream out("tmp/sd-font-migration/"+name+".pgm", std::ios::binary);
  out << "P5\n384 168\n255\n";
  for (int16_t y=0;y<168;++y) for (int16_t x=0;x<384;++x) {
    int16_t px,py; display.canvas().logicalToPhysical(x,y,px,py);
    out.put(static_cast<char>((raw[(py/2)*42+px/4] & (1U << (7-((px%4)*2+py%2)))) ? 0 : 255));
  }
}
void residentChecks(DisplayMonoTft& d) {
  uint8_t a[8064], b[8064];
  d.text().setFont(u8g2_font_7x14_tf);
  const auto asciiWidth=d.text().getUTF8Width("AB 123");
  check(asciiWidth>0,"resident Latin/numbers missing");
  d.clear(); d.text().drawUTF8(50,60,"AB 123"); d.captureFrame(a);
  for (const char* value:{u8"A中文B 123",u8"A\U00010041B 123"}) {
    check(d.text().getUTF8Width(value)==asciiWidth,"unsupported scalar changed resident advance");
    d.clear(); d.text().drawUTF8(50,60,value); d.captureFrame(b);
    check(!memcmp(a,b,sizeof(a)),"unsupported scalar aliased a resident glyph");
  }
  std::puts("PASS: resident Latin/numbers available; unsupported Chinese/UTF32 omitted without wrapping");
}
void logsSettle(SystemUpdatePage& page, const SystemUpdatePage::View& view,
                DisplayMonoTft& d, SdBitmapFont* font, uint32_t now) {
  for (int i=0;i<1000;++i) {
    page.update(view,true,false,now,d.text(),font);
    if (!page.isPreparingLogs()) return;
  }
  throw std::runtime_error("log layout stuck");
}
int main(int argc,char** argv) {
 try {
  TestPsram::available=!(argc>1 && std::string(argv[1])=="--no-psram");
  DisplayMonoTft d; d.begin(); residentChecks(d);
  TestFs::root="tools/font_builder/output";
  SdBitmapFont body,large;
  check(body.open(14),"open14"); settle(body);
  const auto allocations=TestPsram::allocations;
  check(large.open(20,false),"open20"); settle(large);
  check(large.cacheBytes()==0 && TestPsram::allocations==allocations,"second PSRAM cache allocated");
  check(body.cacheBytes()==(TestPsram::available ? 125792U:0),"body cache budget changed");
  AnswersBookPage answer; answer.handleDetailEnter(9,0);
  check(answer.handleDetailInput(9,0,false,false,true,1000),"start think");
  answer.update(5999); check(answer.view_==AnswersBookPage::View::Thinking,"reveal too early");
  answer.update(6000); answer.update(6360);
  check(answer.view_==AnswersBookPage::View::Answer,"reveal timing changed");
  for (uint16_t i=0;i<AnswersBookData::kAnswerCount;++i) {
    answer.answerIndex_=i; d.clear(); answer.renderDetail(9,0,0,d,7000,&body,&large);
    check(body.state()==SdBitmapFont::State::Ready && large.state()==SdBitmapFont::State::Ready,"answer font failed");
    if (i==0 || i==30 || i==78) save(d,"answer-"+std::to_string(i));
  }
  answer.view_=AnswersBookPage::View::History; answer.historyCount_=3;
  answer.historyIndices_[0]=0; answer.historyIndices_[1]=78; answer.historyIndices_[2]=267;
  for(uint8_t selected=0;selected<3;++selected) {
    answer.historyCursor_=selected; d.clear(); answer.renderDetail(9,0,0,d,7000,&body,&large);
    save(d,"history-"+std::to_string(selected));
  }
  check(answer.handleDetailBack(9,0) && answer.view_==AnswersBookPage::View::Answer,"history back");
  answer.handleDetailInput(9,0,true,false,false,7000);
  check(answer.view_==AnswersBookPage::View::History,"history input changed");
  large.end(); check(large.open(24,false),"open24"); settle(large);
  const auto before=TestPsram::allocations;
  d.text().setFont(u8g2_font_6x12_mf);
  const auto latinWidth = d.text().getUTF8Width("ABC");
  { SdUiText::Scope scope(d,&body);
    d.clear(); d.text().setFont(u8g2_font_7x14_tf);
    const char* song=u8"龘 𠮷 音乐 é gyp";
    check(d.text().getUTF8Width(song)==SdBitmapText::measureUTF8(body,song),"scoped UTF32 measurement");
    d.text().drawUTF8(3,25,song); save(d,"external-text");
    d.text().setFont(u8g2_font_6x12_mf);
    check(d.text().getUTF8Width("ABC")==latinWidth,"scope redirected unrelated font");
  }
  d.text().setFont(u8g2_font_7x14_tf);
  check(d.text().getUTF8Width(u8"龘")==0 && d.text().getUTF8Width("ABC")>0,"scope not restored");
  check(TestPsram::allocations==before,"scope allocated");
  ReaderService reader; reader.begin(); ReaderPage readerPage; SdCardService sd;
  // The reader owns font14 and remounts on book open. Close both before that operation.
  body.end(); large.end(); TestFs::root="tmp/reader-sd-font/valid";
  readerPage.handleDetailEnter(2,0,body);
  check(readerPage.handleDetailInput(2,0,false,false,false,false,true,reader,sd,body),"reader book");
  settle(body); TestFs::root="tools/font_builder/output";
  check(large.open(24,false),"reader title24"); settle(large);
  for(int i=0;i<1000;++i) if(readerPage.updateMeaning(reader,body)) break;
  d.clear(); readerPage.renderDetail(2,0,0,d,reader,body,&large); save(d,"reader-24");
  TestFs::failedPath="font-14.bin";
  SdBitmapFont::Glyph failedGlyph;
  body.glyph(0x9F98,failedGlyph);
  check(body.state()==SdBitmapFont::State::Error,"reader I/O failure not detected");
  TestFs::failedPath.clear();
  check(readerPage.handleDetailInput(2,0,false,false,false,false,true,reader,sd,body,&large),"reader retry");
  check(large.state()==SdBitmapFont::State::Closed,"title handle kept during remount");
  settle(body);
  large.end(); readerPage.handleDetailBack(2,0,reader,body);
  check(TestFs::handles==0 && TestPsram::used==0,"reader exit leak");
  TestFs::root="tools/font_builder/output"; body.open(14); settle(body);
  SystemUpdatePage page; SystemUpdatePage::View view;
  view.state=OtaService::State::UpdateAvailable;
  view.version="0.1.3";
  view.notes=u8"优化龘与𠮷的文字显示\n改善更新日志滚动\n这是较长的中文更新说明，用于检查换行和分页。\nEnglish 123";
  page.prepareEnter(); logsSettle(page,view,d,&body,1000);
  check(page.logPageCount()>1,"SD logs pagination missing");
  for(uint8_t i=0;i<page.lineCount_;++i) {
    const auto line=page.lines_[i];
    std::ofstream("tmp/sd-font-migration/log-"+std::to_string(i)+".txt",std::ios::binary)
      .write(view.notes+line.offset,line.bytes);
  }
  std::ofstream("tmp/sd-font-migration/log-count.txt") << unsigned(page.lineCount_);
  d.clear(); page.render(d,0,view,1000,&body); save(d,"update-14");
  const auto reads=TestFs::reads;
  for(int i=0;i<100;++i) check(!page.update(view,true,false,2000,d.text(),&body),"idle relayout");
  check(TestFs::reads==reads,"idle layout reads SD");
  check(page.update(view,true,false,5000,d.text(),&body) && page.needsAnimationFrame(),"SD auto scroll");
  d.clear(); page.render(d,0,view,5275,&body); save(d,"update-slide");
  page.update(view,true,false,5550,d.text(),&body);
  check(page.handleLogInput(true,false,6000),"SD manual paging");
  page.update(view,true,true,6100,d.text(),&body); check(!page.needsAnimationFrame(),"popup kept animation");
  // Slow-card layout still yields; a closed font resets to the Latin-only face.
  page.prepareEnter(); TestFs::readLatencyMs=5;
  const auto start=TestFs::reads; page.update(view,true,false,1000,d.text(),&body);
  check(TestFs::reads-start<30,"unbounded SD layout batch"); TestFs::readLatencyMs=0;
  body.end(); logsSettle(page,view,d,&body,2000);
  check(page.logFont_==nullptr,"closed SD font kept by logs");
  d.clear(); page.render(d,0,view,2000,&body); save(d,"update-offline");
  answer.view_=AnswersBookPage::View::Answer; answer.answerIndex_=0;
  d.clear(); answer.renderDetail(9,0,0,d,8000,&body,&large); save(d,"answer-offline");
  check(body.open(14),"PSRAM reopen"); settle(body);
  check(body.open(20,false),"external -> internal switch"); settle(body);
  check(body.cacheBytes()==0 && TestPsram::used==0,"PSRAM not released on internal switch");
  body.end(); large.end();
  check(TestFs::handles==0 && TestPsram::used==0,"final resources leaked");
  std::printf("PASS: %u answers, 14/20/24 coexistence, scoped UTF32, SD logs timing/cancel/fallback, handles released; PSRAM peak=%zu\n",AnswersBookData::kAnswerCount,TestPsram::peak);
  return 0;
 } catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}

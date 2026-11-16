#include "TxtReaderPage.h"
#include "../../bsp/DisplayMonoTft.h"
#include "../../services/SdBitmapFont.h"
#include "../SdUiText.h"
#include "../DrawUtils.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

void TxtReaderPage::enter(SdCardService& sd) {
  reader_.scan(sd); view_ = View::Shelf; selection_ = menu_ = 0;
  for (auto& w : widths_) w = Width{};
}
void TxtReaderPage::leave() { reader_.end(); font_ = nullptr; }
bool TxtReaderPage::busy() const {
  return reader_.busy() && (!font_ || font_->state() == SdBitmapFont::State::Checking ||
                            font_->state() == SdBitmapFont::State::Ready);
}
int16_t TxtReaderPage::measure(void* context, uint32_t cp) {
  auto& self = *static_cast<TxtReaderPage*>(context);
  auto& cache = self.widths_[cp % 256];
  if (cache.cp == cp) return cache.advance;
  SdBitmapFont::Glyph glyph;
  int16_t width = self.font_->glyph(cp, glyph) ? glyph.advance : 20;
  if (self.font_->state() != SdBitmapFont::State::Ready) return -1;
  cache = {cp, width}; return width;
}
bool TxtReaderPage::update(SdBitmapFont& font) {
  font_ = &font;
  if (font.state() == SdBitmapFont::State::Ready) reader_.tick(measure, this);
  return reader_.consumeChanged();
}
bool TxtReaderPage::input(bool left, bool up, bool down, bool ok, SdCardService& sd) {
  using State = TxtReaderService::State;
  if (reader_.state() != State::Reading) {
    if (left) { leave(); return false; }
    if (reader_.state() != State::Shelf) {
      if (ok) { reader_.scan(sd); selection_ = 0; view_ = View::Shelf; }
      return true;
    }
    if (up && selection_) --selection_;
    if (down && selection_+1 < reader_.count()) ++selection_;
    if (ok && reader_.open(selection_)) view_ = View::Reading;
    return true;
  }
  if (view_ == View::Reading) {
    if (left) { reader_.closeBook(); view_ = View::Shelf; }
    else if (ok) { menu_ = 0; view_ = View::Menu; }
    else if (reader_.pageReady()) {
      const uint32_t page = reader_.currentPage();
      if (up && page) reader_.go(page-1);
      if (down) reader_.go(page+1);
    }
  } else if (view_ == View::Menu) {
    if (left) view_ = View::Reading;
    if (up && menu_) --menu_;
    if (down && menu_ < 3) ++menu_;
    if (ok) {
      if (menu_ == 0) { reader_.bookmark(); view_ = View::Reading; }
      if (menu_ == 1 && reader_.indexed()) { jump_ = reader_.currentPage(); view_ = View::Jump; }
      if (menu_ == 2) { reader_.toggleTheme(); view_ = View::Reading; }
      if (menu_ == 3) { reader_.closeBook(); view_ = View::Shelf; }
    }
  } else if (view_ == View::Jump) {
    if (left) view_ = View::Menu;
    if (up && jump_) --jump_;
    if (down && jump_+1 < reader_.totalPages()) ++jump_;
    if (ok) { reader_.go(jump_); view_ = View::Reading; }
  }
  return true;
}
void TxtReaderPage::render(DisplayMonoTft& d, SdBitmapFont& full14, SdBitmapFont& full20,
                           SdBitmapFont& ui24) {
  const uint16_t fg = reader_.dark() ? ST7305_COLOR_WHITE : ST7305_COLOR_BLACK;
  const uint16_t bg = reader_.dark() ? ST7305_COLOR_BLACK : ST7305_COLOR_WHITE;
  auto& canvas = d.canvas(); auto& text = d.text();
  canvas.drawFilledRectangle(0,0,383,167,bg);
  text.setFont(u8g2_font_7x14_tf); text.setFontMode(1);
  text.setForegroundColor(fg); text.setBackgroundColor(bg);
  auto small = [&](const char* s,int x,int y) { text.drawUTF8(x,y,s); };
  auto draw = [&](SdBitmapFont& f,const char* s,int x,int y,int right,uint16_t color) {
    if (f.state() != SdBitmapFont::State::Ready) {
      text.setForegroundColor(color); small(s,x,y); text.setForegroundColor(fg); return;
    }
    SdUiText::draw(d,&f,x,y,s,color,{static_cast<int16_t>(x),0,static_cast<int16_t>(right),167});
  };
  auto rule = [&](int x,int y,int w) { canvas.drawFilledRectangle(x,y,x+w-1,y,fg); };
  auto pill = [&](int x,int y,int w,int h) { DrawUtils::fillRoundRect(canvas,x,y,x+w-1,y+h-1,h/2,fg); };
  char label[192];
  const auto state = reader_.state();
  if (state == TxtReaderService::State::Shelf) {
    draw(ui24,"我的书架",16,28,260,fg); small("/Books",326,26); rule(16,39,352);
    small("继续",16,68); draw(full20,"阅读",16,94,86,fg);
    snprintf(label,sizeof(label),"%02u 本",reader_.count()); small(label,16,119);
    canvas.drawFilledRectangle(91,51,91,129,fg);
    const int start = std::max(0,std::min(static_cast<int>(selection_)-1,static_cast<int>(reader_.count())-3));
    for (int i=0;i<3 && start+i<reader_.count();++i) {
      const int index=start+i,y=48+i*29; const bool focus=index==selection_;
      if (focus) pill(104,y,264,28);
      const uint16_t color=focus?bg:fg;
      snprintf(label,sizeof(label),"%02d",index+1);
      text.setForegroundColor(color);small(label,116,y+20);
      TxtReaderService::title(reader_.book(index),label,sizeof(label));
      draw(full20,label,145,y+21,337,color);small("›",350,y+20);
    }
    text.setForegroundColor(fg);small("↑↓ 选择",16,155);
    small(reader_.book(selection_).page?"OK 继续阅读":"OK 开始阅读",270,155);return;
  }
  if (state != TxtReaderService::State::Reading) {
    small("阅读 / TXT",16,25);rule(16,37,352);
    const char* title="暂时无法打开书架";const char* a=reader_.error();const char* b="OK 重试　← 返回";
    if (state==TxtReaderService::State::Empty) { title="书架，等你来填满";a="请将 TXT 文件放入 /Books";b="文件夹已准备好　OK 刷新"; }
    if (state==TxtReaderService::State::Missing) { title="请插入 SD 卡";a="你的书，存放在 SD 卡里";b="插入后按 OK 重新检查"; }
    canvas.drawRectangle(28,63,52,94,fg);rule(33,71,14);rule(33,76,11);
    draw(full20,title,76,67,367,fg);small(a,76,94);small(b,76,116);
    small("← 返回",16,155);pill(246,134,122,26);text.setForegroundColor(bg);small("OK 检查",278,153);return;
  }
  if (view_ == View::Jump) {
    draw(ui24,"跳转进度",16,28,368,fg);rule(16,39,352);
    snprintf(label,sizeof(label),"%lu / %lu",static_cast<unsigned long>(jump_+1),static_cast<unsigned long>(reader_.totalPages()));
    small(label,145,83);rule(48,107,288);
    canvas.drawFilledRectangle(48,105,48+288*(jump_+1)/reader_.totalPages()-1,109,fg);
    small("↑↓ 调整　← 取消",16,155);small("OK 跳转",306,155);return;
  }
  TxtReaderService::title(reader_.book(selection_),label,sizeof(label));draw(full14,label,16,21,280,fg);
  snprintf(label,sizeof(label),"%u%%",reader_.progress());small(label,334,21);rule(16,30,352);
  if (reader_.pageReady()) {
    for (uint8_t i=0;i<4;++i) draw(full20,reader_.content().lines[i],18,57+i*26,365,fg);
    if(reader_.marked()) { canvas.drawFilledRectangle(356,32,363,43,fg); }
  } else {
    small(full20.state()==SdBitmapFont::State::Error?"字库不可用，请返回后重试":"正在准备正文，按 Left 可返回",30,84);
  }
  rule(16,148,352);const int progress=352*reader_.progress()/100;
  if(progress)canvas.drawFilledRectangle(16,147,16+progress-1,149,fg);
  if (view_ != View::Menu) return;
  canvas.drawFilledRectangle(149,0,383,167,bg);canvas.drawFilledRectangle(149,0,149,167,fg);
  draw(full20,"阅读选项",166,25,372,fg);
  if(reader_.indexed()) snprintf(label,sizeof(label),"当前页：%lu / %lu",static_cast<unsigned long>(reader_.currentPage()+1),static_cast<unsigned long>(reader_.totalPages()));
  else snprintf(label,sizeof(label),"当前页：%lu / 统计中",static_cast<unsigned long>(reader_.currentPage()+1));
  small(label,166,45);
  const char* items[]={reader_.marked()?"移除本页书签":"添加书签",reader_.indexed()?"跳转进度":"正在统计页码",reader_.dark()?"白底黑字":"黑底白字","返回书架"};
  for(uint8_t i=0;i<4;++i) { const int y=52+i*23;if(i==menu_)pill(160,y,212,22);text.setForegroundColor(i==menu_?bg:fg);small(items[i],173,y+17); }
  text.setForegroundColor(fg);small("← 返回　OK 确认",166,155);
}

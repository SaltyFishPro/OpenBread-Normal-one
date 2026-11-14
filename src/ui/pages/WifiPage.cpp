#include "WifiPage.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <qrcode.h>
#include "../../bsp/DisplayMonoTft.h"
#include "../../services/TimeService.h"
#include "../DrawUtils.h"
#include "../SdUiText.h"
#include "../TextUtils.h"
#include "../../log/Log.h"

namespace {
constexpr uint16_t kWhite = ST7305_COLOR_WHITE;
constexpr uint16_t kBlack = ST7305_COLOR_BLACK;
constexpr int16_t kRight = 368, kBodyY = 37;
void wifiIcon(ST7305_2p9_BW_DisplayDriver& canvas, int16_t x, int16_t y, int16_t size) {
  // Integer sample of the same three arcs in the HTML SVG; no per-frame trig.
  const int8_t points[][10] = {
      {8,23,17,17,29,13,41,17,50,23},
      {15,31,21,26,29,24,37,26,43,31},
      {23,39,26,36,29,35,32,36,35,39}};
  for (const auto& arc : points) for (uint8_t i = 0; i < 8; i += 2)
    canvas.drawLine(x + arc[i] * size / 58, y + arc[i+1] * size / 58,
                    x + arc[i+2] * size / 58, y + arc[i+3] * size / 58, kWhite);
  DrawUtils::drawClippedFilledRect(canvas,x+28*size/58,y+46*size/58,
                                   x+30*size/58,y+48*size/58,kWhite);
}

void heroIcon(ST7305_2p9_BW_DisplayDriver& c, uint8_t kind, int16_t y) {
  if (kind == 0) { wifiIcon(c,26,y+57,58); return; }
  const int16_t cy = y + 86;
  if (kind == 4) {
    // Precomputed points on the preview's dashed circle, one small arc per pair.
    constexpr int8_t ring[][2] = {{23,0},{21,9},{16,16},{9,21},{0,23},{-9,21},
      {-16,16},{-21,9},{-23,0},{-21,-9},{-16,-16},{-9,-21},{0,-23},{9,-21},{16,-16},{21,-9}};
    for (uint8_t i = 0; i < 16; i += 2)
      c.drawLine(55+ring[i][0],cy+ring[i][1],55+ring[i+1][0],cy+ring[i+1][1],kWhite);
  } else c.drawCircle(55,cy,23,kWhite);
  if (kind == 1) { c.drawLine(43,cy,51,cy+8,kWhite); c.drawLine(51,cy+8,67,cy-9,kWhite); }
  else if (kind == 2 || kind == 3) { c.drawLine(55,cy-15,55,cy+1,kWhite); c.drawLine(55,cy+1,65,cy+7,kWhite); if(kind==3)c.drawLine(36,cy-19,74,cy+19,kWhite); }
  else { c.drawLine(44,cy,66,cy,kWhite); c.drawLine(59,cy-7,66,cy,kWhite); c.drawLine(59,cy+7,66,cy,kWhite); }
}

constexpr int16_t kWifiQrMaxModules = 29;
constexpr int16_t kWifiQrRowBytes = (kWifiQrMaxModules + 7) / 8;
constexpr int16_t kWifiQrQuietZone = 4;
constexpr int16_t kWifiQrScale = 3;
constexpr int16_t kWifiQrFrameSize =
    (kWifiQrMaxModules + 2 * kWifiQrQuietZone) * kWifiQrScale;
uint8_t gWifiQrRows[kWifiQrMaxModules][kWifiQrRowBytes] = {};
char gWifiQrSsid[33] = {};
int16_t gWifiQrSize = 0;

void captureWifiQr(esp_qrcode_handle_t qr) {
  const int size = esp_qrcode_get_size(qr);
  if (size <= 0 || size > kWifiQrMaxModules) {
    gWifiQrSize = 0;
    return;
  }
  memset(gWifiQrRows, 0, sizeof(gWifiQrRows));
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      if (esp_qrcode_get_module(qr, x, y)) {
        gWifiQrRows[y][x >> 3] |= static_cast<uint8_t>(0x80U >> (x & 7));
      }
    }
  }
  gWifiQrSize = static_cast<int16_t>(size);
}

bool prepareWifiQr(const char* ssid) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }
  if (gWifiQrSize > 0 && strcmp(gWifiQrSsid, ssid) == 0) {
    return true;
  }

  gWifiQrSize = 0;
  gWifiQrSsid[0] = '\0';
  char payload[72];
  const int length = snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;;", ssid);
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(payload)) {
    return false;
  }

  esp_qrcode_config_t config = {};
  config.display_func = captureWifiQr;
  config.max_qrcode_version = 3;
  config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  if (esp_qrcode_generate(&config, payload) != ESP_OK || gWifiQrSize == 0) {
    return false;
  }

  snprintf(gWifiQrSsid, sizeof(gWifiQrSsid), "%s", ssid);
  return true;
}

bool wifiQrModule(int16_t x, int16_t y) {
  return (gWifiQrRows[y][x >> 3] & static_cast<uint8_t>(0x80U >> (x & 7))) != 0;
}

void drawWifiQr(DisplayMonoTft& display, int16_t x, int16_t y) {
  auto& canvas = display.canvas();
  canvas.drawFilledRectangle(x, y, static_cast<int16_t>(x + kWifiQrFrameSize - 1),
                             static_cast<int16_t>(y + kWifiQrFrameSize - 1),
                             ST7305_COLOR_WHITE);
  for (int16_t row = 0; row < gWifiQrSize; ++row) {
    for (int16_t col = 0; col < gWifiQrSize; ++col) {
      if (!wifiQrModule(col, row)) {
        continue;
      }
      const int16_t left = static_cast<int16_t>(x + (col + kWifiQrQuietZone) * kWifiQrScale);
      const int16_t top = static_cast<int16_t>(y + (row + kWifiQrQuietZone) * kWifiQrScale);
      canvas.drawFilledRectangle(left, top, static_cast<int16_t>(left + kWifiQrScale - 1),
                                 static_cast<int16_t>(top + kWifiQrScale - 1),
                                 ST7305_COLOR_BLACK);
    }
  }
}


}

void WifiPage::prepareEnter(const WifiProvisionService& wifi) {
  focus_ = record_ = action_ = choice_ = 0;
  confirm_ = Confirm::None;
  pendingPortal_ = renderedConfirm_ = Confirm::None;
  focusOnly_ = startingPresented_ = false;
  storageError_ = false;
  previousState_ = wifi.state();
  view_ = wifi.isRadioActive() ? View::Session : View::List;
}

void WifiPage::beginFonts(SdBitmapFont& ui20, SdBitmapFont& ui24) {
  ui20_ = &ui20; ui24_ = &ui24;
  fontsStarted_ = true;
}
void WifiPage::endFonts() {
  ui20_ = ui24_ = nullptr;
  fontsStarted_ = false;
  pendingPortal_ = renderedConfirm_ = Confirm::None;
  focusOnly_ = startingPresented_ = false;
}
bool WifiPage::fontsLoading() const {
  return (ui20_ && ui20_->state() == SdBitmapFont::State::Checking) ||
         (ui24_ && ui24_->state() == SdBitmapFont::State::Checking);
}
bool WifiPage::needsFullFont20(const WifiProvisionService& wifi) const {
  return (view_ == View::List && wifi.networks().count() != 0) ||
         view_ == View::Detail || view_ == View::Replace;
}
bool WifiPage::needsFullFont14(const WifiProvisionService& wifi) const {
  if (confirm_ != Confirm::None && confirm_ != Confirm::Start) return true;
  if (view_ != View::Session) return false;
  const auto state = wifi.state();
  return state == WifiProvisionService::State::Connecting ||
         state == WifiProvisionService::State::Failed ||
         (state == WifiProvisionService::State::Connected && wifi.isRadioActive());
}
bool WifiPage::update(const WifiProvisionService& wifi) {
  if (previousState_ == wifi.state()) return false;
  previousState_ = wifi.state();
  action_ = 0;
  return true;
}
void WifiPage::showList(const WifiProvisionService& wifi) {
  view_ = View::List; confirm_ = Confirm::None;
  focus_ = std::min(focus_,wifi.networks().count());
}
void WifiPage::addNetwork(const WifiProvisionService& wifi) {
  choice_ = 0; storageError_ = false;
  if (wifi.networks().count() == WifiNetworkStore::kCapacity) {
    view_ = View::Replace; focus_ = WifiNetworkStore::kCapacity - 1;
  } else confirm_ = Confirm::Start;
}
void WifiPage::activateRecord(const WifiProvisionService& wifi) {
  if (!wifi.networks().info(focus_)) return;
  record_ = focus_; action_ = choice_ = 0;
  if (view_ == View::Replace) confirm_ = Confirm::Replace;
  else view_ = View::Detail;
}
void WifiPage::executeConfirmation(WifiProvisionService& wifi) {
  const Confirm pending = confirm_;
  confirm_ = Confirm::None;
  if (pending == Confirm::Forget) {
    storageError_ = !wifi.forgetNetwork(record_);
    if (!storageError_) showList(wifi);
    return;
  }
  pendingPortal_ = pending;
  startingPresented_ = false;
  view_ = View::Starting;
}
bool WifiPage::afterPresent(uint32_t nowMs, WifiProvisionService& wifi) {
  if (view_ != View::Starting || !startingPresented_ || pendingPortal_ == Confirm::None) return false;
  const Confirm pending = pendingPortal_;
  pendingPortal_ = Confirm::None;
  char prefill[33] = {};
  int8_t replacement = -1;
  if (pending == Confirm::Replace) replacement = static_cast<int8_t>(record_);
  if (pending == Confirm::Reconfigure) {
    snprintf(prefill,sizeof(prefill),"%s",wifi.targetSsid());
    if (wifi.networks().count() == WifiNetworkStore::kCapacity)
      replacement = wifi.networks().find(prefill);
  }
  wifi.startPortal(nowMs,WifiProvisionService::Purpose::TimeSync,replacement,prefill);
  view_ = View::Session;
  return true;
}
bool WifiPage::handleInput(bool left, bool up, bool down, bool ok, uint32_t nowMs,
                            WifiProvisionService& wifi, TimeService& time) {
  if (confirm_ != Confirm::None) {
    if (left || ok) focusOnly_ = false;
    if (left) confirm_ = Confirm::None;
    else if (up) choice_ = 0;
    else if (down) choice_ = 1;
    else if (ok) { if (choice_) executeConfirmation(wifi); else confirm_ = Confirm::None; }
    if (up || down)
      focusOnly_ = renderedConfirm_ == confirm_;
    return true;
  }
  focusOnly_ = false;
  if (view_ == View::Starting) return true;
  if (left) {
    if (view_ == View::List) return false;
    if (view_ == View::Session && wifi.isRadioActive()) {
      const bool holding = wifi.state() == WifiProvisionService::State::Connected && wifi.isPortalActive();
      wifi.cancelProvision(time);
      if (holding) return true;
    }
    showList(wifi); return true;
  }
  const auto state = wifi.state();
  if (up || down) {
    if (view_ == View::List || view_ == View::Replace) {
      const uint8_t count = wifi.networks().count();
      const uint8_t maximum = view_ == View::Replace && count ? count - 1 : count;
      if (up && focus_) --focus_;
      if (down && focus_ < maximum) ++focus_;
    } else if (view_ == View::Detail || state == WifiProvisionService::State::Failed)
      action_ = down ? 1 : 0;
    return true;
  }
  if (!ok) return true;
  if (view_ == View::List) {
    if (focus_ == wifi.networks().count()) addNetwork(wifi);
    else activateRecord(wifi);
  } else if (view_ == View::Replace) activateRecord(wifi);
  else if (view_ == View::Detail) {
    if (action_ == 0) { if (wifi.connectSaved(record_)) view_ = View::Session; }
    else { confirm_ = Confirm::Forget; choice_ = 0; }
  } else if (state == WifiProvisionService::State::Failed) {
    if (action_ == 0 && wifi.canRetryConnect()) wifi.retryConnect();
    else if (!wifi.fromSavedNetwork() && wifi.isPortalActive()) {
      // Return the phone to its form without restarting AP or losing replacement.
      wifi.editPortal();
    } else { confirm_ = wifi.fromSavedNetwork() ? Confirm::Reconfigure : Confirm::Start; choice_ = 0; }
  } else if (state == WifiProvisionService::State::PortalTimeout) {
    const int8_t replacement = wifi.replacementIndex();
    if (replacement >= 0) record_ = static_cast<uint8_t>(replacement);
    confirm_ = replacement >= 0 ? Confirm::Replace : Confirm::Start;
    choice_ = 0;
  }
  else if (state == WifiProvisionService::State::Connected && !wifi.isRadioActive()) showList(wifi);
  return true;
}

int16_t WifiPage::textWidth(DisplayMonoTft& d, SdBitmapFont* font, const char* value) {
  d.text().setFont(u8g2_font_7x14_tf);
  return SdUiText::width(d,font,value);
}
void WifiPage::text(DisplayMonoTft& d, SdBitmapFont* font, int16_t x, int16_t baseline,
                     const char* value, int16_t width, uint16_t color) {
  d.text().setFont(u8g2_font_7x14_tf);
  d.text().setFontMode(1);
  d.text().setForegroundColor(color); d.text().setBackgroundColor(kBlack);
  char fitted[144] = {};
  const char* output = value;
  // Most labels fit: measure once rather than rereading every SD-font prefix.
  if (textWidth(d,font,value) > width || strlen(value) >= sizeof(fitted)) {
    size_t used = 0;
    const char* p = value;
    while (*p && used + 5 < sizeof(fitted)) {
      const uint8_t bytes = TextUtils::utf8CharLen(*p);
      if (strlen(p) < bytes) break;
      memcpy(fitted+used,p,bytes); fitted[used+bytes] = '\0';
      if (textWidth(d,font,fitted) > width) { fitted[used] = '\0'; break; }
      used += bytes; p += bytes;
    }
    if (*p) {
      while (used && textWidth(d,font,fitted) + textWidth(d,font,"...") > width) {
        --used; while (used && TextUtils::utf8IsContinuation(fitted[used])) --used;
        fitted[used] = '\0';
      }
      memcpy(fitted+used,"...",4);
    }
    output = fitted;
  }
  const int16_t size = SdUiText::ready(font) ? font->metrics().pixelSize : 14;
  const SdBitmapText::Clip clip = {x,static_cast<int16_t>(std::max<int>(0,baseline-size-3)),
    static_cast<int16_t>(std::min<int>(d.width()-1,x+width-1)),
    static_cast<int16_t>(std::min<int>(d.height()-1,baseline+5))};
  SdUiText::draw(d,font,x,baseline,output,color,clip);
}

void WifiPage::renderConfirmationButtons(DisplayMonoTft& d, int16_t y) {
  auto& c=d.canvas();
  DrawUtils::drawClippedFilledRect(c,24,y+101,243,y+128,kBlack);
  const char* labels[] = {"取消",confirm_ == Confirm::Forget ? "忘记网络" : "开启热点"};
  for (uint8_t i=0;i<2;++i) {
    const int16_t x=24+i*116;
    const bool selected=choice_==i;
    DrawUtils::drawRoundRect(c,x,y+101,104,28,14,selected?kWhite:kBlack,kWhite);
    text(d,nullptr,x+10,y+121,labels[i],84,selected?kBlack:kWhite);
  }
}
bool WifiPage::renderConfirmationFocusOnly(DisplayMonoTft& d) {
  if (!focusOnly_ || confirm_ == Confirm::None || renderedConfirm_ != confirm_) return false;
  focusOnly_=false;
  renderConfirmationButtons(d,0);
  return true;
}

void WifiPage::render(DisplayMonoTft& d, int16_t y, const WifiProvisionService& wifi,
                       const TimeService& time, SdBitmapFont* fullFont, SdBitmapFont* networkFont) {
  auto& c = d.canvas();
  renderedConfirm_=Confirm::None;
  focusOnly_=false;
  DrawUtils::drawClippedFilledRect(c,0,y,d.width()-1,y+d.height()-1,kBlack);
  SdBitmapFont* small = nullptr; // Outer resident 14-ui cache.
  SdBitmapFont* medium = SdUiText::ready(ui20_) ? ui20_ : nullptr;
  SdBitmapFont* large = SdUiText::ready(ui24_) ? ui24_ : medium;
  const uint8_t count = wifi.networks().count();
  const bool listing = view_ == View::List && count;
  const bool replacing = view_ == View::Replace;
  const bool detail = view_ == View::Detail;
  const auto state = wifi.state();
  wifiIcon(c,16,y+10,16);
  text(d,small,39,y+23,replacing ? "选择要替换的网络" : listing || detail ? "已保存网络" : "WiFi 配网",230,kWhite);
  if (listing || replacing || detail) {
    char right[48];
    if (detail) snprintf(right,sizeof(right),"网络详情");
    else if (replacing) snprintf(right,sizeof(right),"%u / %u",focus_+1,count);
    else snprintf(right,sizeof(right),"%u / 5 已保存",count);
    text(d,small,kRight-textWidth(d,small,right),y+23,right,120,kWhite);
  } else {
    uint8_t step = 0;
    if (view_ == View::Session) step = state == WifiProvisionService::State::PortalReady || state == WifiProvisionService::State::PortalTimeout ? 1 : state == WifiProvisionService::State::Connected && !wifi.isRadioActive() ? 3 : 2;
    int16_t x = step ? 330 : 341;
    for (uint8_t i=1;i<=3;++i) {
      if (i==step) { DrawUtils::drawRoundRect(c,x,y+16,16,5,2,kWhite,kWhite); x+=22; }
      else { if(i<step)c.drawFilledCircle(x+2,y+18,2,kWhite); else c.drawCircle(x+2,y+18,2,kWhite); x+=11; }
    }
  }
  const char* back = "返回";
  const char* action = "";
  if (!fontsStarted_ || fontsLoading() ||
      (needsFullFont20(wifi) && networkFont && networkFont->state()==SdBitmapFont::State::Checking) ||
      (needsFullFont14(wifi) && fullFont && fullFont->state()==SdBitmapFont::State::Checking)) {
    wifiIcon(c,26,y+57,58);
    text(d,small,112,y+83,"正在准备字体",256,kWhite);
    text(d,small,112,y+110,"完成后显示网络列表",256,kWhite);
    text(d,small,16,y+157,"< 返回",210,kWhite);
    return;
  }
  auto actionButton = [&](int16_t x,int16_t top,int16_t width,const char* label,bool selected) {
    if(selected) DrawUtils::drawRoundRect(c,x,y+top,width,24,12,kBlack,kWhite);
    text(d,small,x+10,y+top+18,label,width-20,kWhite);
  };
  if (confirm_ != Confirm::None) {
    const char* title = "开启配网热点？";
    char note[104] = "开启后，用手机扫码连接设备。";
    if (confirm_ == Confirm::Forget) { title="忘记这个网络？"; const auto* info=wifi.networks().info(record_); snprintf(note,sizeof(note),"%s",info?info->ssid:""); }
    else if (confirm_ == Confirm::Replace) { title="替换并添加网络？"; const auto* info=wifi.networks().info(record_); snprintf(note,sizeof(note),"新增成功后替换：%s",info?info->ssid:""); }
    else if (confirm_ == Confirm::Reconfigure) { title="重新配置网络？"; snprintf(note,sizeof(note),"成功后更新：%s",wifi.targetSsid()); }
    text(d,large,24,y+61,title,336,kWhite);
    text(d,confirm_ == Confirm::Start ? small : fullFont,24,y+84,note,336,kWhite);
    renderConfirmationButtons(d,y);
    if (y==0) renderedConfirm_=confirm_;
    back="取消"; action="确认";
  } else if (view_ == View::Starting) {
    heroIcon(c,4,y);
    text(d,large,112,y+74,"正在开启热点",256,kWhite);
    text(d,small,112,y+102,"请稍候",256,kWhite);
    if (y==0) startingPresented_=true;
    back="";
  } else if (listing || replacing) {
    const uint8_t start = static_cast<uint8_t>(std::max<int>(0,std::min<int>(focus_-1,count-3)));
    for (uint8_t row=0;row<3 && start+row<count;++row) {
      const uint8_t index=start+row;
      const int16_t top=kBodyY+row*26;
      if(focus_==index)DrawUtils::drawRoundRect(c,16,y+top,352,25,12,kBlack,kWhite);
      wifiIcon(c,26,y+top+4,17);
      text(d,SdUiText::ready(networkFont)?networkFont:medium,52,y+top+20,wifi.networks().info(index)->ssid,288,kWhite);
      text(d,small,350,y+top+18,">",12,kWhite);
    }
    if(replacing)text(d,small,26,y+135,"连接成功后才替换",310,kWhite);
    else {
      const bool selected=focus_==count;
      if(selected)DrawUtils::drawRoundRect(c,16,y+118,352,21,10,kWhite,kWhite);
      text(d,small,26,y+134,"+ 添加新网络",330,selected?kBlack:kWhite);
    }
    back=replacing?"返回":"设置"; action=focus_==count?"添加":replacing?"选择":"查看";
  } else if (detail) {
    const auto* info=wifi.networks().info(record_);
    if(info) {
      text(d,SdUiText::ready(networkFont)?networkFont:medium,16,y+58,info->ssid,352,kWhite);
      char date[72];
      const auto& used=info->lastUsed;
      if(used.year)snprintf(date,sizeof(date),"上次成功连接  %02u/%02u %02u:%02u",used.month,used.day,used.hour,used.minute);
      else snprintf(date,sizeof(date),"上次成功连接时间未记录");
      text(d,small,16,y+81,storageError_?"记录保存失败，请重试":date,352,kWhite);
      actionButton(16,88,352,action_==0?"> 连接此网络":"连接此网络",action_==0);
      actionButton(16,115,352,action_==1?"> 忘记网络":"忘记网络",action_==1);
    }
    back="列表"; action="确认";
  } else if (view_==View::Session && state==WifiProvisionService::State::PortalReady) {
    text(d,medium,16,y+58,"扫码加入设备热点",232,kWhite);
    text(d,small,16,y+82,wifi.apSsid(),232,kWhite);
    text(d,small,16,y+106,"连接后，在手机页面填写 WiFi",232,kWhite);
    text(d,small,16,y+126,"未弹出？打开 192.168.4.1",232,kWhite);
    if(prepareWifiQr(wifi.apSsid()))drawWifiQr(d,257,y+31);
    back="关闭热点";
  } else if (view_==View::Session && state==WifiProvisionService::State::Failed) {
    const bool absent=wifi.error()==WifiProvisionService::Error::ApNotFound;
    const bool hotspotFailed=wifi.error()==WifiProvisionService::Error::Unknown;
    text(d,medium,16,y+58,hotspotFailed?"热点开启失败":absent?"网络不在附近":"连接未成功",352,kWhite);
    text(d,fullFont,16,y+83,wifi.targetSsid(),352,kWhite);
    const char* reason=hotspotFailed?"请重新开启配网热点":absent?"未找到此网络，请确认距离与路由器":wifi.error()==WifiProvisionService::Error::AuthFailed?"密码可能已变更，请检查后重试":"连接超时，请检查网络后重试";
    text(d,small,16,y+103,reason,352,kWhite);
    actionButton(16,115,hotspotFailed?352:171,hotspotFailed?"重新开启":"重试连接",hotspotFailed||action_==0);
    if(!hotspotFailed)actionButton(197,115,171,wifi.fromSavedNetwork()?"重新配网":"修改网络信息",action_==1);
    back="关闭并返回"; action="确认";
  } else {
    const char* title="添加第一个网络";
    const char* first="用手机完成 WiFi 配置";
    const char* second="连接成功后自动保存";
    uint8_t kind=0; action="添加网络";
    if(view_==View::Session) {
      action="";
      switch(state) {
        case WifiProvisionService::State::Connecting: title="正在连接";first=wifi.targetSsid();second=wifi.fromSavedNetwork()?"正在使用已保存的网络信息":"请保持手机连接设备热点";kind=4;back="取消连接";break;
        case WifiProvisionService::State::SyncingTime:title="同步设备时间";first="网络连接成功";second="校时完成后自动关闭 WiFi";kind=2;break;
        case WifiProvisionService::State::PortalTimeout:title="热点已关闭";first="等待连接超时";second="原有连接记录保持不变";kind=3;action="重新开启";break;
        case WifiProvisionService::State::Connected:
          kind=1;
          if(wifi.isRadioActive()) {title="网络已连接";first=wifi.targetSsid();second=wifi.staIp();back="继续校时";}
          else {title="一切准备就绪";first="网络信息已保存 · 时间已同步";second="WiFi 已关闭";action="查看记录";
            if(time.snapshot().syncState!=TimeService::SyncState::Success)first="网络已连接 · 时间未同步";
          }
          if(wifi.saveStatus()==WifiProvisionService::SaveStatus::StorageError)first="连接成功 · 记录保存失败";
          if(wifi.saveStatus()==WifiProvisionService::SaveStatus::Full)first="连接成功 · 记录已满未保存";
          break;
        default:break;
      }
    }
    heroIcon(c,kind,y);
    text(d,large,112,y+74,title,256,kWhite);
    const bool network=view_==View::Session && (state==WifiProvisionService::State::Connecting || (state==WifiProvisionService::State::Connected && wifi.isRadioActive()));
    // Network text here is 14px in the HTML; use the full 20px source only in
    // the list/detail. The full 14px source is supplied separately below.
    text(d,network?fullFont:small,112,y+102,first,256,kWhite);
    text(d,small,112,y+125,second,256,kWhite);
  }
  if (back[0]) {
    char left[64];snprintf(left,sizeof(left),"< %s",back);
    text(d,small,16,y+157,left,210,kWhite);
  }
  if(action[0]) {
    const int16_t actionWidth=textWidth(d,small,action);
    const int16_t x=kRight-actionWidth-37;
    DrawUtils::drawRoundRect(c,x,y+140,31,18,4,kBlack,kWhite);
    text(d,small,x+5,y+154,"OK",24,kWhite);
    text(d,small,x+37,y+157,action,actionWidth+1,kWhite);
  }
}

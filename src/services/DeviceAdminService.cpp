#include "DeviceAdminService.h"
#include <WiFi.h>
#include <esp_system.h>
#include <SD_MMC.h>
#include "OtaService.h"
#include "TimeService.h"
#include "PowerDiagnosticService.h"
#include "DeviceAdminPageHtml.h"
#include "AdminFilePath.h"
#include "../bsp/SdCardSession.h"
#include "../app/FirmwareInfo.h"
#include "../log/Log.h"

namespace {

String jsonEscape(const char* s) { String out; for (;*s;++s) { if (*s=='"' || *s=='\\') out += '\\'; out += *s; } return out; }
}

bool DeviceAdminService::start(uint32_t now, DeviceSettings& settings, OtaService& ota, TimeService& time, PowerDiagnosticService& power) {
  if (active_) return true;
  if (WiFi.getMode() != WIFI_OFF) {
    LogE("ADMIN", "AP start refused: WiFi is already in use");
    return false;
  }
  settings_ = &settings; ota_ = &ota; time_ = &time; power_ = &power; failures_ = 0; blockedUntilMs_ = 0; token_[0] = 0;
  snprintf(ssid_, sizeof(ssid_), "OpenBread-%06X", static_cast<unsigned>(ESP.getEfuseMac() & 0xffffff));
  snprintf(code_, sizeof(code_), "%06u", static_cast<unsigned>(esp_random() % 1000000U));
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid_)) { LogE("ADMIN", "AP start failed"); WiFi.mode(WIFI_OFF); return false; }
  if (!dns_.start(53, "*", WiFi.softAPIP())) { LogE("ADMIN", "DNS start failed"); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF); return false; }
  if (!routesReady_) { routes(); routesReady_ = true; }
  server_.begin(); activityMs_ = now; active_ = changed_ = true;
  LogI("ADMIN", "AP started ssid=%s ip=%s", ssid_, WiFi.softAPIP().toString().c_str());
  return true;
}
void DeviceAdminService::stop() {
  if (!active_) return;
  abortUpload();
  if (localWriting_) { esp_ota_abort(localHandle_); localWriting_ = false; }
  localReady_ = false; localPartition_ = nullptr;
  ota_->cancel(); ota_->setKeepAccessPoint(false);
  server_.stop(); dns_.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
  token_[0] = code_[0] = 0; active_ = false; changed_ = true;
  LogI("ADMIN", "management stopped; radio off");
}
void DeviceAdminService::tick(uint32_t now) {
  if (!active_) return;
  dns_.processNextRequest(); server_.handleClient();
  now = millis();
  if (stopPending_) { stopPending_ = false; stop(); return; }
  if (restartPending_ && static_cast<int32_t>(now - restartAtMs_) >= 0) ESP.restart();
  const uint32_t timeout = settings_->values().adminTimeoutSeconds * 1000U;
  if (ota_->isBusy() || localWriting_ || uploadStarted_) activityMs_ = now;
  if (timeout && now - activityMs_ >= timeout) { LogI("ADMIN", "idle timeout"); stop(); }
}
bool DeviceAdminService::authorized(bool activity) {
  if (!token_[0] || server_.header("X-Admin-Token") != token_) { server_.send(403,"text/plain","请重新登录"); return false; }
  if (activity) activityMs_ = millis(); return true;
}
const char* DeviceAdminService::statusText() const {
  if (ota_ && ota_->isBusy()) {
    if (ota_->isConnecting()) return "正在联网";
    if (ota_->state() == OtaService::State::Downloading) return "正在下载";
    if (ota_->state() == OtaService::State::Verifying) return "正在校验";
    if (ota_->state() == OtaService::State::Applying) return "正在更新";
    return "检查更新中";
  }
  return SD_MMC.cardType() == CARD_NONE ? "SD 卡未插入" : "热点已开启";
}
void DeviceAdminService::routes() {
  const char* headers[] = {"X-Admin-Token"}; server_.collectHeaders(headers, 1);
  server_.on("/", HTTP_GET, [this](){ server_.sendHeader("Cache-Control","no-store"); server_.send_P(200,"text/html; charset=utf-8",kDeviceAdminPage); });
  server_.on("/api/login", HTTP_POST, [this](){
    uint32_t now=millis();
    if (blockedUntilMs_ && static_cast<int32_t>(now-blockedUntilMs_) < 0) { server_.send(429,"text/plain","尝试次数过多，请稍后重试"); return; }
    if (server_.arg("code") != code_) { if (++failures_ >= 5) { failures_=0; blockedUntilMs_=now+30000; } server_.send(403,"text/plain","验证码不正确"); return; }
    failures_=0; blockedUntilMs_=0; for (int i=0;i<4;++i) snprintf(token_+i*8,9,"%08x",static_cast<unsigned>(esp_random()));
    activityMs_=now; server_.send(200,"application/json",String("{\"token\":\"")+token_+"\"}");
  });
  server_.on("/api/settings", HTTP_POST, [this](){ if(!authorized()) return; const auto& v=settings_->values(); String body=String("{\"name\":\"")+jsonEscape(v.name)+"\",\"sleep\":"+v.sleepSeconds+",\"timeout\":"+v.adminTimeoutSeconds+"}"; server_.send(200,"application/json",body); });
  server_.on("/api/close", HTTP_POST, [this](){ if(!authorized()) return; if(ota_->isBusy() || localWriting_) {server_.send(409,"text/plain","请先取消更新");return;} server_.send(200,"application/json","{}"); stopPending_=true; });
  server_.on("/api/save", HTTP_POST, [this](){ if(!authorized()) return; auto v=settings_->values(); String name=server_.arg("name"); if(name.length()>=sizeof(v.name)){ server_.send(400,"text/plain","设备名称过长"); return; } name.toCharArray(v.name,sizeof(v.name)); String s=server_.arg("sleep"), t=server_.arg("timeout"); if ((s!="0"&&s!="20"&&s!="60"&&s!="300") || (t!="0"&&t!="600"&&t!="1800")) {server_.send(400,"text/plain","设置无效");return;} v.sleepSeconds=s.toInt(); v.adminTimeoutSeconds=t.toInt(); if(!settings_->save(v)){server_.send(500,"text/plain","保存失败");return;} server_.send(200,"application/json","{}"); });
  registerFileRoutes();
  registerUpdateRoutes();
  server_.onNotFound([this](){ server_.sendHeader("Location","http://192.168.4.1/",true); server_.send(302,"text/plain",""); });
}

bool DeviceAdminService::filePath(const String& value, bool write) {
  if (!AdminFilePath::valid(value.c_str()) || value.startsWith("/.admin-")) return false;
  if (write && (value == "/" || AdminFilePath::isFontResource(value.c_str()))) return false;
  return true;
}
void DeviceAdminService::abortUpload() {
  if (uploadFile_) uploadFile_.close();
  if (uploadStarted_) SD_MMC.remove("/.admin-upload.tmp");
  uploadStarted_ = uploadOk_ = false;
  uploadTarget_[0] = 0;
}
void DeviceAdminService::registerFileRoutes() {
  server_.on("/api/overview", HTTP_POST, [this]() {
    if (!authorized()) return;
    String body = String("{\"name\":\"")+jsonEscape(settings_->values().name)+"\",\"version\":\""+FirmwareInfo::kVersion+"\",\"sd\":";
    bool mounted = SD_MMC.cardType() != CARD_NONE;
    bool batteryValid = power_->refresh();
    uint32_t percent = power_->reading().stateOfChargeX100 / 100U;
    if (percent > 100) percent = 100;
    const auto& clock = time_->snapshot();
    char timeText[12] = {};
    if (clock.valid) snprintf(timeText,sizeof(timeText),"%02u:%02u",clock.now.hour,clock.now.minute);
    body += mounted ? "true" : "false";
    body += ",\"total\":"; body += String(static_cast<unsigned long long>(mounted ? SD_MMC.cardSize() : 0));
    body += ",\"used\":"; body += String(static_cast<unsigned long long>(mounted ? SD_MMC.usedBytes() : 0));
    body += ",\"battery\":"; body += batteryValid ? String(percent) : "null";
    body += ",\"time\":\""; body += timeText; body += "\"}";
    server_.send(200,"application/json",body);
  });
  server_.on("/api/files", HTTP_POST, [this]() {
    if (!authorized()) return;
    String path=server_.arg("path");
    if (!filePath(path,false)) {server_.send(400,"text/plain","路径无效");return;}
    File dir=SD_MMC.open(path.c_str());
    if (!dir || !dir.isDirectory()) {server_.send(404,"text/plain","文件夹不可用");return;}
    String json="{\"files\":["; unsigned count=0;
    for (File f=dir.openNextFile(); f && count<100; f=dir.openNextFile()) {
      String name=f.name(); int slash=name.lastIndexOf('/'); if (slash>=0) name=name.substring(slash+1);
      if (!name.startsWith(".admin-")) {
        if(count++) json+=',';
        json+=String("{\"name\":\"")+jsonEscape(name.c_str())+"\",\"dir\":"+(f.isDirectory()?"true":"false")+",\"size\":"+String(static_cast<unsigned long>(f.size()))+"}";
      }
      f.close();
    }
    json += "]}"; server_.send(200,"application/json",json);
  });
  server_.on("/api/download", HTTP_POST, [this]() {
    if(!authorized()) return;
    String path=server_.arg("path");
    if(!filePath(path,false)) {server_.send(400,"text/plain","路径无效");return;}
    File f=SD_MMC.open(path.c_str(),FILE_READ);
    if(!f || f.isDirectory()) {server_.send(404,"text/plain","文件不可用");return;}
    server_.streamFile(f,"application/octet-stream"); f.close();
  });
  server_.on("/api/file-action", HTTP_POST, [this]() {
    if(!authorized()) return;
    String path=server_.arg("path"), op=server_.arg("op"), dest=server_.arg("dest");
    if(!filePath(path,true)) {server_.send(400,"text/plain","路径无效或受保护");return;}
    bool ok=false;
    if(op=="mkdir") ok=!SD_MMC.exists(path.c_str()) && SD_MMC.mkdir(path.c_str());
    else if(op=="rename") ok=filePath(dest,true) && !SD_MMC.exists(dest.c_str()) && SD_MMC.rename(path.c_str(),dest.c_str());
    else if(op=="delete") { File f=SD_MMC.open(path.c_str()); bool dir=f && f.isDirectory(); f.close(); ok=dir?SD_MMC.rmdir(path.c_str()):SD_MMC.remove(path.c_str()); }
    if(!ok) { server_.send(400,"text/plain","操作失败；文件可能已存在或文件夹非空");return; }
    SdCardSession::invalidate(); server_.send(200,"application/json","{}");
  });
  server_.on("/api/upload", HTTP_POST, [this]() {
    if(!authorized()) {abortUpload();return;}
    bool ok=uploadStarted_ && uploadOk_; uploadStarted_=uploadOk_=false; uploadTarget_[0]=0;
    if(!ok) {server_.send(400,"text/plain","上传失败，文件已存在或路径受保护");return;}
    server_.send(200,"application/json","{}");
  }, [this]() {
    HTTPUpload& u=server_.upload();
    if(u.status==UPLOAD_FILE_START) {
      abortUpload();
      if(!token_[0] || server_.header("X-Admin-Token")!=token_) return;
      String path=server_.arg("path");
      if(!filePath(path,true) || SD_MMC.exists(path.c_str())) return;
      snprintf(uploadTarget_,sizeof(uploadTarget_),"%s",path.c_str());
      SD_MMC.remove("/.admin-upload.tmp");
      uploadFile_=SD_MMC.open("/.admin-upload.tmp",FILE_WRITE);
      uploadStarted_=static_cast<bool>(uploadFile_); uploadOk_=uploadStarted_;
      activityMs_=millis();
    } else if(u.status==UPLOAD_FILE_WRITE && uploadStarted_) {
      if(uploadOk_ && uploadFile_.write(u.buf,u.currentSize)!=u.currentSize) uploadOk_=false;
      activityMs_=millis();
    } else if(u.status==UPLOAD_FILE_END && uploadStarted_) {
      uploadFile_.close();
      if(uploadOk_ && !SD_MMC.exists(uploadTarget_)) uploadOk_=SD_MMC.rename("/.admin-upload.tmp",uploadTarget_);
      else uploadOk_=false;
      if(!uploadOk_) SD_MMC.remove("/.admin-upload.tmp");
      else SdCardSession::invalidate();
    } else if(u.status==UPLOAD_FILE_ABORTED) abortUpload();
  });
}
void DeviceAdminService::registerUpdateRoutes() {
  server_.on("/api/online-status", HTTP_POST, [this]() {
    if(!authorized(false)) return;
    const auto state=ota_->state();
    String body=String("{\"state\":")+static_cast<unsigned>(state)+",\"error\":"+static_cast<unsigned>(ota_->error())+",\"progress\":"+ota_->progressPercent()+",\"version\":\""+jsonEscape(ota_->manifest().version)+"\"}";
    server_.send(200,"application/json",body);
  });
  server_.on("/api/online", HTTP_POST, [this]() {
    if(!authorized()) return;
    if(localWriting_ || localReady_) {server_.send(409,"text/plain","请先取消本地固件");return;}
    String action=server_.arg("action"); bool ok=false;
    ota_->setKeepAccessPoint(true);
    if(action=="check") ok=ota_->requestCheck(millis());
    else if(action=="download") ok=ota_->requestDownload(millis());
    else if(action=="cancel") { ota_->cancel(); ok=true; }
    else if(action=="apply" && ota_->state()==OtaService::State::ReadyToApply) {
      server_.send(200,"application/json","{}"); ota_->requestApply(); return;
    }
    if(!ok) {server_.send(409,"text/plain","无法执行操作，请检查已保存的 WiFi 与更新状态");return;}
    server_.send(200,"application/json","{}");
  });
  server_.on("/api/local-cancel", HTTP_POST, [this]() {
    if(!authorized()) return;
    if(localWriting_) {esp_ota_abort(localHandle_);localWriting_=false;}
    localReady_=false;localPartition_=nullptr;server_.send(200,"application/json","{}");
  });
  server_.on("/api/local-apply", HTTP_POST, [this]() {
    if(!authorized()) return;
    if(!localReady_ || !localPartition_ || esp_ota_set_boot_partition(localPartition_)!=ESP_OK) {
      server_.send(400,"text/plain","固件未就绪或无法启动"); return;
    }
    localReady_=false; restartPending_=true; changed_=true; restartAtMs_=millis()+500;
    LogI("ADMIN", "local firmware confirmed; restarting");
    server_.send(200,"application/json","{}");
  });
  server_.on("/api/local-upload", HTTP_POST, [this]() {
    if(!authorized()) return;
    if(!localReady_) {server_.send(400,"text/plain","固件上传失败或格式无效");return;}
    server_.send(200,"application/json","{}");
  }, [this]() {
    HTTPUpload& u=server_.upload();
    if(u.status==UPLOAD_FILE_START) {
      if(localWriting_) esp_ota_abort(localHandle_);
      localWriting_=localReady_=false; localPartition_=nullptr;
      if(!token_[0] || server_.header("X-Admin-Token")!=token_ || ota_->isBusy() || ota_->state()==OtaService::State::ReadyToApply) return;
      if(!u.filename.endsWith(".bin") && !u.filename.endsWith(".BIN")) return;
      localPartition_=esp_ota_get_next_update_partition(nullptr);
      if(!localPartition_) return;
      esp_err_t err=esp_ota_begin(localPartition_, OTA_SIZE_UNKNOWN, &localHandle_);
      localWriting_=err==ESP_OK;
      LogI("ADMIN", "local firmware upload start result=%d", static_cast<int>(err));
      activityMs_=millis();
    } else if(u.status==UPLOAD_FILE_WRITE && localWriting_) {
      esp_err_t err=esp_ota_write(localHandle_,u.buf,u.currentSize);
      if(err!=ESP_OK) {LogE("ADMIN", "local firmware write failed err=%d",static_cast<int>(err));esp_ota_abort(localHandle_);localWriting_=false;}
      activityMs_=millis();
    } else if(u.status==UPLOAD_FILE_END && localWriting_) {
      esp_err_t err=esp_ota_end(localHandle_); localWriting_=false; localReady_=err==ESP_OK;
      LogI("ADMIN", "local firmware validation result=%d", static_cast<int>(err));
    } else if(u.status==UPLOAD_FILE_ABORTED && localWriting_) {
      esp_ota_abort(localHandle_);localWriting_=false;localReady_=false;
    }
  });
}

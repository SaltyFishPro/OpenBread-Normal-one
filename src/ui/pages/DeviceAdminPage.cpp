#include "DeviceAdminPage.h"
#include "../../services/DeviceAdminService.h"

bool DeviceAdminPage::input(bool left, bool ok, DeviceAdminService& service) {
  if (left) {
    if (service.active() && !confirm_) { confirm_=true; return true; }
    if (service.active()) { confirm_=false; return true; }
    pending_=starting_=false; return false;
  }
  if (ok && confirm_) { service.stop(); confirm_=false; return true; }
  if (ok && !service.active() && !starting_) { pending_=starting_=true; failed_=false; }
  return true;
}
bool DeviceAdminPage::afterPresent(uint32_t now, DeviceAdminService& service, DeviceSettings& settings, OtaService& ota, TimeService& time, PowerDiagnosticService& power) {
  if (!pending_) return false;
  pending_=false; failed_=!service.start(now,settings,ota,time,power); starting_=false;
  if (!failed_) opened_ = true;
  return true;
}
void DeviceAdminPage::render(DisplayMonoTft& display, int16_t offset, const DeviceAdminService& service,
                             SdBitmapFont* ui20, SdBitmapFont* ui24) const {
  View view = {service.active(), starting_, failed_, confirm_, service.ssid(), service.code()};
  view.closed = opened_ && !service.active() && !starting_ && !failed_;
  view.updating = service.restarting();
  view.status = service.statusText();
  renderView(display, offset, view, ui20, ui24);
}

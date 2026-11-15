#pragma once
#include <stdint.h>
class DisplayMonoTft;
class DeviceAdminService;
class DeviceAdminPage {
public:
  struct View {
    bool active = false, starting = false, failed = false, confirm = false;
    const char* ssid = "";
    const char* code = "";
    bool closed = false, updating = false;
    const char* status = "热点已开启";
  };
  static void renderView(DisplayMonoTft& display, int16_t offset, const View& view,
                          class SdBitmapFont* ui20, class SdBitmapFont* ui24);
  void prepare() { pending_ = false; starting_ = false; failed_ = false; confirm_ = false; opened_ = false; }
  bool input(bool left, bool ok, DeviceAdminService& service);
  bool afterPresent(uint32_t now, DeviceAdminService& service, class DeviceSettings& settings,
                    class OtaService& ota, class TimeService& time, class PowerDiagnosticService& power);
  void render(DisplayMonoTft& display, int16_t offset, const DeviceAdminService& service,
              class SdBitmapFont* ui20, class SdBitmapFont* ui24) const;
private:
  bool pending_ = false, starting_ = false, failed_ = false, confirm_ = false;
  bool opened_ = false;
};

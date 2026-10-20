#pragma once

// Hardware route only. The caller must release SD_MMC before enabling the bridge.
class SdUsbSwitch {
public:
  void begin();
  void setEnabled(bool enabled);
  bool enabled() const { return enabled_; }

private:
  bool enabled_ = false;
};

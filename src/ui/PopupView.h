#pragma once

#include <stdint.h>

class DisplayMonoTft;

// 统一弹窗外观与"两选项确认"按键语义，所有弹窗共用同一套实现与动画：
// 进场 = 从屏幕上方滑入 + 由 85% 放大到 100%，退场反向；默认 100ms。
// 左侧是安全项（默认选中），右侧是执行项；Up/Down/Right 切换，Left 取消，OK 确认。
namespace PopupView {

// 弹窗动画时长；改这一个常量即可调整所有弹窗的快慢。
constexpr uint16_t kAnimMs = 100;

struct Anim {
  bool visible = false;
  bool running = false;
  bool closing = false;
  uint32_t startMs = 0;
};

struct ConfirmState {
  bool primarySelected = true;
  Anim anim;
  // 退场动画结束后要返回的结果（Confirmed 表示选中了右侧执行项）。
  bool pendingConfirmed = false;
};

struct InfoState {
  Anim anim;
};

enum class Result : uint8_t {
  None,
  Confirmed,
  Cancelled,
};

// 0..1000 的显示程度：进场由 0 到满、退场由满到 0，使用缓出曲线。
uint16_t shownFixed(const Anim& anim, uint32_t nowMs);
bool isAnimating(const Anim& anim);
inline bool isVisible(const Anim& anim) { return anim.visible; }

void open(ConfirmState& state, uint32_t nowMs);
// 启动退场；动作在退场结束、update() 返回结果时再执行。
void requestClose(ConfirmState& state, bool confirmed, uint32_t nowMs);
bool handleInput(ConfirmState& state, bool leftEdge, bool rightEdge, bool upEdge, bool downEdge,
                 bool okEdge, uint32_t nowMs);
// 每帧调用；退场结束时返回 Confirmed / Cancelled，其余情况返回 None。
Result update(ConfirmState& state, uint32_t nowMs);

void open(InfoState& state, uint32_t nowMs);
void requestClose(InfoState& state, uint32_t nowMs);
// 每帧调用；退场结束后弹窗自动消失。
void update(InfoState& state, uint32_t nowMs);

// primaryLabel 在左（安全项），dangerLabel 在右（执行项）。
void drawConfirm(DisplayMonoTft& display, int16_t yOffset, const ConfirmState& state,
                 const char* title, const char* primaryLabel, const char* dangerLabel,
                 uint32_t nowMs);

// 提示型弹窗：标题 + 一行数值（音量这类不需要用户确认的信息）。
void drawInfo(DisplayMonoTft& display, int16_t yOffset, const InfoState& state, const char* title,
              const char* value, uint32_t nowMs);

}  // namespace PopupView

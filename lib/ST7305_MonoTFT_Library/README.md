# ST7305_MonoTFT_Library

 

### Default pins
These macros live in `DisplayConfig.h` (override as needed):
```c
#define PIN_DC    4
#define PIN_CS    3
#define PIN_SCLK  2
#define PIN_SDIN  1
#define PIN_RST   0
```
Use the convenience constructors that read from these defaults:
```cpp
ST7305_2p9_BW_DisplayDriver lcd(spi);   // 2.9"
ST7305_4p2_BW_DisplayDriver lcd(spi);   // 4.2"
```

### 屏幕旋转

支持0°, 90°, 180°, 270°旋转。在初始化后调用 `setRotation()` 方法：

```cpp
display.initialize();
display.setRotation(1);  // 90度顺时针旋转
```

**旋转参数值：**
- `0` - 无旋转（0°）
- `1` - 顺时针90°
- `2` - 180°
- `3` - 逆时针90°（或顺时针270°）

**逻辑屏幕尺寸变化：**
- 原始 (2.9"): 168 × 384
  - 旋转0°/180°: 168 × 384
  - 旋转90°/270°: 384 × 168

- 原始 (4.2"): 300 × 400
  - 旋转0°/180°: 300 × 400
  - 旋转90°/270°: 400 × 300

**获取当前旋转状态：**
```cpp
uint8_t rotation = display.getRotation();
int16_t width = display.getDisplayWidth();
int16_t height = display.getDisplayHeight();
```

**使用示例：**
```cpp
display.initialize();
display.setRotation(1);  // 设置90°旋转

// 绘制一个矩形，坐标基于旋转后的逻辑尺寸
display.drawFilledRectangle(10, 20, 100, 100, 0x01);
display.display();
```

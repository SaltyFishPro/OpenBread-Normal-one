#ifndef ST7305_2P9_BW_DISPLAY_DRIVER_H
#define ST7305_2P9_BW_DISPLAY_DRIVER_H

#include <Arduino.h>
#include <SPI.h>
#include <ST73XX_UI.h>
#include <ST73xxPins.h>

#define ST7305_COLOR_WHITE (0x00)
#define ST7305_COLOR_BLACK (0x01)

class ST7305_2p9_BW_DisplayDriver : public ST73XX_UI{
public:
    ST7305_2p9_BW_DisplayDriver(int dcPin, int resPin, int csPin, int sclkPin, int sdinPin, SPIClass& spi);
    ST7305_2p9_BW_DisplayDriver(const ST73xxPins& pins, SPIClass& spi);
    // Convenience: uses default pins from DisplayConfig.h
    ST7305_2p9_BW_DisplayDriver(SPIClass& spi);
    ~ST7305_2p9_BW_DisplayDriver();

    void initialize();
    void fill(uint8_t data);

    void clearDisplay();

    void writePoint(uint x, uint y, bool enabled) override;
    void writePoint(uint x, uint y, uint16_t data) override;

    // 按物理字节批量写入线段，替代基类逐像素调用 writePoint 的实现
    void drawFastHLine(int16_t x, int16_t y, int16_t len, uint16_t color) override;
    void drawFastVLine(int16_t x, int16_t y, int16_t len, uint16_t color) override;

    void display();
    // 只刷新面板物理坐标区域 (x1,y1)-(x2,y2)；x 为 0..167，y 为 0..383
    void displayRegion(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

    // 帧缓冲快照/回填，供上层缓存静态图层使用
    size_t frameBufferLength() const { return static_cast<size_t>(DISPLAY_BUFFER_LENGTH); }
    void copyFrameBufferTo(uint8_t* dst) const;
    void copyFrameBufferFrom(const uint8_t* src);

    void Initial_ST7305();
    void Low_Power_Mode();
    void High_Power_Mode();
    void display_on(bool enabled);
    void display_Inversion(bool enabled);

private:
    const int DC_PIN;
    const int RES_PIN;
    const int CS_PIN;
    const int SCLK_PIN;
    const int SDIN_PIN;
    const int LCD_WIDTH;
    const int LCD_HIGH;
    const int LCD_DATA_WIDTH;
    const int LCD_DATA_HIGH;
    const int DISPLAY_BUFFER_LENGTH;
    uint8_t* display_buffer;
    SPIClass& spiRef;

    void address();
    void drawLogicalRun(int16_t lx, int16_t ly, int16_t len, bool horizontal, uint16_t color);
    void writePhysicalHLine(uint32_t py, int32_t px0, int32_t px1, uint16_t color);
    void writePhysicalVLine(uint32_t px, int32_t py0, int32_t py1, uint16_t color);
    void Write_Register(uint8_t idat);
    void Write_Parameter(uint8_t ddat);
};

#endif

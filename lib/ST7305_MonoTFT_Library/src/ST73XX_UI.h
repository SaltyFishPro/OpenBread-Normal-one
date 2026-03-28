#ifndef ST73XX_UI_H
#define ST73XX_UI_H

#include <Arduino.h>
#include <SPI.h>

class ST73XX_UI {
public:
    ST73XX_UI(int16_t w, int16_t h);
    ~ST73XX_UI();

    virtual void writePoint(uint x, uint y, bool enabled);
    virtual void writePoint(uint x, uint y, uint16_t color);

    void drawFastHLine(int16_t x, int16_t y, int16_t len, uint16_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t len, uint16_t color);

    // 画直线
    void drawLine(uint x1, uint y1, uint x2, uint y2, uint16_t color);

    // 画三角形
    void drawTriangle(uint x1, uint y1, uint x2, uint y2, uint x3, uint y3, uint16_t color);

    // 画实心三角形
    void drawFilledTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);

    // 画矩形
    void drawRectangle(uint x1, uint y1, uint x2, uint y2, uint16_t color);

    // 画实心矩形
    void drawFilledRectangle(uint x1, uint y1, uint x2, uint y2, uint16_t color);

    // 画圆形
    void drawCircle(uint xc, uint yc, uint r, uint16_t color);

    // 画实心圆形
    void drawFilledCircle(int centerX, int centerY, int radius, uint16_t color);

    // 画多边形
    void drawPolygon(uint* points, int n, uint16_t color);

    // 画实心多边形
    void drawFilledPolygon(uint* points, int n, uint16_t color);

    // 旋转控制函数
    // rotation: 0=0度, 1=90度顺时针, 2=180度, 3=270度顺时针
    void setRotation(uint8_t rotation);
    uint8_t getRotation() const { return _rotation; }
    int16_t getDisplayWidth() const { return _displayWidth; }
    int16_t getDisplayHeight() const { return _displayHeight; }

protected:
    int16_t _displayWidth;
    int16_t _displayHeight;
    uint8_t _rotation;

private:
    const int WIDTH;
    const int HEIGHT;
    
    // 坐标转换辅助函数
    void rotateCoordinates(uint& x, uint& y) const;
};

#endif

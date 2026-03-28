#include <ST73XX_UI.h>
#include <stdlib.h>
#include <new>

#define ABS_DIFF(x, y) (((x) > (y))? ((x) - (y)) : ((y) - (x)))

static inline int32_t clampI32(int32_t value, int32_t low, int32_t high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

ST73XX_UI::ST73XX_UI(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h), _displayWidth(w), _displayHeight(h), _rotation(0)
{
}

ST73XX_UI::~ST73XX_UI() {
}

void ST73XX_UI::setRotation(uint8_t rotation) {
    _rotation = rotation % 4;
    
    // 根据旋转角度交换宽高
    if (_rotation == 0 || _rotation == 2) {
        _displayWidth = WIDTH;
        _displayHeight = HEIGHT;
    } else {
        _displayWidth = HEIGHT;
        _displayHeight = WIDTH;
    }
}

void ST73XX_UI::rotateCoordinates(uint& x, uint& y) const {
    uint tempX = x;
    uint tempY = y;
    
    switch (_rotation) {
        case 0:
            // 无旋�?
            x = tempX;
            y = tempY;
            break;
        case 1:
            // 顺时�?0度旋�?- 使用逻辑高而不是物理高
            x = _displayHeight - 1 - tempY;
            y = tempX;
            break;
        case 2:
            // 180度旋�?- 使用逻辑宽和�?
            x = _displayWidth - 1 - tempX;
            y = _displayHeight - 1 - tempY;
            break;
        case 3:
            // 逆时�?0度旋转（或顺时针270度） - 使用逻辑�?
            x = tempY;
            y = _displayWidth - 1 - tempX;
            break;
    }
}

void ST73XX_UI::writePoint(uint x, uint y, bool enabled) {
    
}

void ST73XX_UI::writePoint(uint x, uint y, uint16_t color) {
    
}

// 函数功能�?

// 这个函数名为u8g2_draw_hv_line，它的作用是在使用u8g2库的图形上下文中绘制水平或垂直的线段�?

// 参数解释�?

//     u8g2_font_t *u8g2：这是一个指向u8g2库的字体结构的指针，通过这个指针可以访问到u8g2库的图形绘制函数�?
//     int16_t x和int16_t y：表示线段起点的坐标�?
//     int16_t len：线段的长度�?
//     uint8_t dir：指定线段的方向，有以下几种取值：
//         0：表示绘制水平线段，从左到右，起点坐标为(x, y)，长度为len�?
//         1：表示绘制垂直线段，从上到下，起点坐标为(x, y)，长度为len�?
//         2：表示绘制水平线段，从右到左，起点坐标为(x - len + 1, y)，长度为len�?
//         3：表示绘制垂直线段，从下到上，起点坐标为(x, y - len + 1)，长度为len�?
//     uint16_t color：指定线段的颜色�?


// 函数执行过程�?

//     根据传入的dir参数的值，选择不同的绘制方式�?
//         如果dir�?，调用u8g2->gfx->drawFastHLine(x,y,len,color)绘制从左到右的水平线段�?
//         如果dir�?，调用u8g2->gfx->drawFastVLine(x,y,len,color)绘制从上到下的垂直线段�?
//         如果dir�?，调用u8g2->gfx->drawFastHLine(x - len + 1,y,len,color)绘制从右到左的水平线段�?
//         如果dir�?，调用u8g2->gfx->drawFastVLine(x,y - len + 1,len,color)绘制从下到上的垂直线段�?


void ST73XX_UI::drawFastHLine(int16_t x, int16_t y, int16_t len, uint16_t color) {
    if (len <= 0 || _displayWidth <= 0 || _displayHeight <= 0) {
        return;
    }

    int32_t yPos = y;
    if (yPos < 0 || yPos >= _displayHeight) {
        return;
    }

    int32_t startX = x;
    int32_t endX = x + len - 1;
    if (startX > endX) {
        int32_t t = startX;
        startX = endX;
        endX = t;
    }
    if (endX < 0 || startX >= _displayWidth) {
        return;
    }

    startX = clampI32(startX, 0, _displayWidth - 1);
    endX = clampI32(endX, 0, _displayWidth - 1);

    for (int32_t i = startX; i <= endX; ++i) {
        uint px = (uint)i;
        uint py = (uint)yPos;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
    }
}

void ST73XX_UI::drawFastVLine(int16_t x, int16_t y, int16_t len, uint16_t color) {
    if (len <= 0 || _displayWidth <= 0 || _displayHeight <= 0) {
        return;
    }

    int32_t xPos = x;
    if (xPos < 0 || xPos >= _displayWidth) {
        return;
    }

    int32_t startY = y;
    int32_t endY = y + len - 1;
    if (startY > endY) {
        int32_t t = startY;
        startY = endY;
        endY = t;
    }
    if (endY < 0 || startY >= _displayHeight) {
        return;
    }

    startY = clampI32(startY, 0, _displayHeight - 1);
    endY = clampI32(endY, 0, _displayHeight - 1);

    for (int32_t i = startY; i <= endY; ++i) {
        uint px = (uint)xPos;
        uint py = (uint)i;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
    }
}

// 画直�?
void ST73XX_UI::drawLine(uint x1, uint y1, uint x2, uint y2, uint16_t color) {
    if (_displayWidth <= 0 || _displayHeight <= 0) {
        return;
    }

    int32_t ix1 = (int32_t)x1;
    int32_t iy1 = (int32_t)y1;
    int32_t ix2 = (int32_t)x2;
    int32_t iy2 = (int32_t)y2;
    const int32_t maxX = _displayWidth - 1;
    const int32_t maxY = _displayHeight - 1;

    if ((ix1 < 0 && ix2 < 0) || (ix1 > maxX && ix2 > maxX) ||
        (iy1 < 0 && iy2 < 0) || (iy1 > maxY && iy2 > maxY)) {
        return;
    }

    ix1 = clampI32(ix1, 0, maxX);
    iy1 = clampI32(iy1, 0, maxY);
    ix2 = clampI32(ix2, 0, maxX);
    iy2 = clampI32(iy2, 0, maxY);

    x1 = (uint)ix1;
    y1 = (uint)iy1;
    x2 = (uint)ix2;
    y2 = (uint)iy2;

    int dx = ABS_DIFF(x2, x1);
    int dy = ABS_DIFF(y2, y1);
    int sx = (x1 < x2)? 1 : -1;
    int sy = (y1 < y2)? 1 : -1;
    int err = dx - dy;

    uint32_t guard = (uint32_t)(_displayWidth + _displayHeight) * 4U + 16U;
    while (guard--) {
        uint px = x1;
        uint py = y1;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

// 画三角形
void ST73XX_UI::drawTriangle(uint x1, uint y1, uint x2, uint y2, uint x3, uint y3, uint16_t color) {
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x3, y3, color);
    drawLine(x3, y3, x1, y1, color);
}

// 绘制实心三角�?
void ST73XX_UI::drawFilledTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    if (_displayWidth <= 0 || _displayHeight <= 0) {
        return;
    }

    uint polygonPoints[] = {
        (uint)clampI32(x0, 0, _displayWidth - 1),
        (uint)clampI32(y0, 0, _displayHeight - 1),
        (uint)clampI32(x1, 0, _displayWidth - 1),
        (uint)clampI32(y1, 0, _displayHeight - 1),
        (uint)clampI32(x2, 0, _displayWidth - 1),
        (uint)clampI32(y2, 0, _displayHeight - 1)
    };
    drawFilledPolygon(polygonPoints, 3, color);
}

// 画矩�?
void ST73XX_UI::drawRectangle(uint x1, uint y1, uint x2, uint y2, uint16_t color) {
    drawLine(x1, y1, x2, y1, color);
    drawLine(x2, y1, x2, y2, color);
    drawLine(x2, y2, x1, y2, color);
    drawLine(x1, y2, x1, y1, color);
}

// 画实心矩�?
void ST73XX_UI::drawFilledRectangle(uint x1, uint y1, uint x2, uint y2, uint16_t color) {
    if (_displayWidth <= 0 || _displayHeight <= 0) {
        return;
    }

    int32_t left = (int32_t)x1;
    int32_t top = (int32_t)y1;
    int32_t right = (int32_t)x2;
    int32_t bottom = (int32_t)y2;

    if (left > right) {
        int32_t t = left;
        left = right;
        right = t;
    }
    if (top > bottom) {
        int32_t t = top;
        top = bottom;
        bottom = t;
    }

    if (right < 0 || left >= _displayWidth || bottom < 0 || top >= _displayHeight) {
        return;
    }

    left = clampI32(left, 0, _displayWidth - 1);
    right = clampI32(right, 0, _displayWidth - 1);
    top = clampI32(top, 0, _displayHeight - 1);
    bottom = clampI32(bottom, 0, _displayHeight - 1);

    for (int32_t y = top; y <= bottom; ++y) {
        for (int32_t x = left; x <= right; ++x) {
            uint px = (uint)x;
            uint py = (uint)y;
            rotateCoordinates(px, py);
            writePoint(px, py, color);
        }
    }
}

// 画圆形（简单的 Bresenham 算法�?
void ST73XX_UI::drawCircle(uint xc, uint yc, uint r, uint16_t color) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    while (x <= y) {
        uint px, py;
        
        px = xc + x; py = yc + y;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
        
        px = xc - x; py = yc + y;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
        
        px = xc + x; py = yc - y;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
        
        px = xc - x; py = yc - y;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
        
        px = xc + y; py = yc + x;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
        
        px = xc - y; py = yc + x;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
        
        px = xc + y; py = yc - x;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
        
        px = xc - y; py = yc - x;
        rotateCoordinates(px, py);
        writePoint(px, py, color);
        
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

// 绘制实心圆形并调�?writePoint 函数
void ST73XX_UI::drawFilledCircle(int centerX, int centerY, int radius, uint16_t color) {
    int x = radius;
    int y = 0;
    int decisionOver2 = 1 - x;

    while (x >= y) {
        // 绘制水平线段填充圆形的八分之一
        for (int i = centerX - x; i <= centerX + x; i++) {
            uint px = i;
            uint py = centerY + y;
            rotateCoordinates(px, py);
            writePoint(px, py, color);
            
            px = i;
            py = centerY - y;
            rotateCoordinates(px, py);
            writePoint(px, py, color);
        }
        for (int i = centerX - y; i <= centerX + y; i++) {
            uint px = i;
            uint py = centerY + x;
            rotateCoordinates(px, py);
            writePoint(px, py, color);
            
            px = i;
            py = centerY - x;
            rotateCoordinates(px, py);
            writePoint(px, py, color);
        }

        y++;
        if (decisionOver2 <= 0) {
            decisionOver2 += 2 * y + 1;
        } else {
            x--;
            decisionOver2 += 2 * (y - x) + 1;
        }
    }
}

// 画多边形（假设顶点数组为 points，n 为顶点数量）
void ST73XX_UI::drawPolygon(uint* points, int n, uint16_t color) {
    if (points == nullptr || n < 2) {
        return;
    }

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        drawLine(points[2 * i], points[2 * i + 1], points[2 * j], points[2 * j + 1], color);
    }
}

// 画实心多边形（扫描线算法，假设顶点数组为 points，n 为顶点数量）
void ST73XX_UI::drawFilledPolygon(uint* points, int n, uint16_t color) {
    if (points == nullptr || n < 3 || _displayWidth <= 0 || _displayHeight <= 0) {
        return;
    }

    int* rotatedPoints = new (std::nothrow) int[2 * n];
    if (rotatedPoints == nullptr) {
        return;
    }

    for (int i = 0; i < n; ++i) {
        int32_t x = clampI32((int32_t)points[2 * i], 0, _displayWidth - 1);
        int32_t y = clampI32((int32_t)points[2 * i + 1], 0, _displayHeight - 1);
        uint px = (uint)x;
        uint py = (uint)y;
        rotateCoordinates(px, py);
        rotatedPoints[2 * i] = (int)px;
        rotatedPoints[2 * i + 1] = (int)py;
    }

    int minY = rotatedPoints[1];
    int maxY = rotatedPoints[1];
    for (int i = 0; i < n; ++i) {
        int y = rotatedPoints[2 * i + 1];
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    minY = (int)clampI32(minY, 0, _displayHeight - 1);
    maxY = (int)clampI32(maxY, 0, _displayHeight - 1);
    if (minY > maxY) {
        delete[] rotatedPoints;
        return;
    }

    int* intersections = new (std::nothrow) int[2 * n];
    if (intersections == nullptr) {
        delete[] rotatedPoints;
        return;
    }

    int intersectionCount = 0;
    for (int y = minY; y <= maxY; ++y) {
        intersectionCount = 0;
        for (int i = 0; i < n; ++i) {
            int nextIndex = (i + 1) % n;
            int x1 = rotatedPoints[2 * i];
            int y1 = rotatedPoints[2 * i + 1];
            int x2 = rotatedPoints[2 * nextIndex];
            int y2 = rotatedPoints[2 * nextIndex + 1];

            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                int x = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
                if (intersectionCount < (2 * n)) {
                    intersections[intersectionCount++] = x;
                }
            }
        }

        for (int i = 0; i < intersectionCount - 1; ++i) {
            for (int j = i + 1; j < intersectionCount; ++j) {
                if (intersections[i] > intersections[j]) {
                    int temp = intersections[i];
                    intersections[i] = intersections[j];
                    intersections[j] = temp;
                }
            }
        }

        for (int i = 0; (i + 1) < intersectionCount; i += 2) {
            int startX = intersections[i];
            int endX = intersections[i + 1];
            if (startX > endX) {
                int t = startX;
                startX = endX;
                endX = t;
            }

            startX = (int)clampI32(startX, 0, _displayWidth - 1);
            endX = (int)clampI32(endX, 0, _displayWidth - 1);
            for (int x = startX; x <= endX; ++x) {
                writePoint((uint)x, (uint)y, color);
            }
        }
    }

    delete[] intersections;
    delete[] rotatedPoints;
}

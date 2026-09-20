#include <ST73XX_UI.h>
#include <ST7305_2p9_BW_DisplayDriver.h>
#include <stdlib.h>

#define ABS_DIFF(x, y) (((x) > (y))? ((x) - (y)) : ((y) - (x)))

ST7305_2p9_BW_DisplayDriver::ST7305_2p9_BW_DisplayDriver(int dcPin, int resPin, int csPin, int sclkPin, int sdinPin, SPIClass& spi) : 
    DC_PIN(dcPin), RES_PIN(resPin), CS_PIN(csPin), SCLK_PIN(sclkPin), SDIN_PIN(sdinPin),
    LCD_WIDTH(168), LCD_HIGH(384),
    ST73XX_UI(168, 384),
    // 168/4=42 一行共42个byte的数据，上下两行共用一行的数据，所以总行数需要除2
    // 384/2=192 所以共192行，一行42个byte数据，共192*42=8064byte
    LCD_DATA_WIDTH(42), LCD_DATA_HIGH(192), DISPLAY_BUFFER_LENGTH(8064),
    spiRef(spi)
{
    display_buffer = new uint8_t[DISPLAY_BUFFER_LENGTH];
    // delete[] display_buffer;

    // 像素数据结构为：
    // P1 P3 P5 P7
    // P2 P5 P6 P8

    // P0 P2 P4 P6
    // P1 P3 P5 P7

    // 对应一个byte数据的：
    // BIT7 BIT5 BIT3 BIT1
    // BIT6 BIT4 BIT2 BIT0
}

ST7305_2p9_BW_DisplayDriver::~ST7305_2p9_BW_DisplayDriver() {
    delete[] display_buffer;
}

void ST7305_2p9_BW_DisplayDriver::initialize() {
    pinMode(DC_PIN, OUTPUT);
    pinMode(RES_PIN, OUTPUT);
    pinMode(CS_PIN, OUTPUT);

    digitalWrite(RES_PIN, HIGH);

    spiRef.setFrequency(40000000);
    spiRef.begin(SCLK_PIN, -1, SDIN_PIN, -1);

    Initial_ST7305();

    fill(0x00);
}

void ST7305_2p9_BW_DisplayDriver::fill(uint8_t data) {
    memset(display_buffer, data, DISPLAY_BUFFER_LENGTH);
}

void ST7305_2p9_BW_DisplayDriver::clearDisplay() {
    memset(display_buffer, 0x00, DISPLAY_BUFFER_LENGTH);
}

void ST7305_2p9_BW_DisplayDriver::writePoint(uint x, uint y, bool enabled) {
    if(x>=LCD_WIDTH || y>=LCD_HIGH){
        return;
    }
    else{
        // 找到是哪一行的数据
        uint real_x = x/4; // 0->0, 3->0, 4->1, 7->1
        uint real_y = y/2; // 0->0, 1->0, 2->1, 3->1
        uint write_byte_index = real_y*LCD_DATA_WIDTH+real_x;
        uint one_two = (y % 2 == 0)?0:1; // 0 1
        uint line_bit_4 = x % 4; // 0 1 2 3
        uint8_t write_bit = 7-(line_bit_4*2+one_two);

        if (enabled) {
            // 将指定位置的 bit 置为 1
            display_buffer[write_byte_index] |= (1 << write_bit);
        } else {
            // 将指定位置的 bit 置为 0
            display_buffer[write_byte_index] &= ~(1 << write_bit);
        }
    }
}

void ST7305_2p9_BW_DisplayDriver::writePoint(uint x, uint y, uint16_t data) {
    if(x>=LCD_WIDTH || y>=LCD_HIGH){
        return;
    }
    else{
        // 找到是哪一行的数据
        uint real_x = x/4; // 0->0, 3->0, 4->1, 7->1
        uint real_y = y/2; // 0->0, 1->0, 2->1, 3->1
        uint write_byte_index = real_y*LCD_DATA_WIDTH+real_x;
        uint one_two = (y % 2 == 0)?0:1; // 0 1
        uint line_bit_4 = x % 4; // 0 1 2 3
        uint8_t write_bit = 7-(line_bit_4*2+one_two);

        if (data != 0) {
            // 将指定位置的 bit 置为 1
            display_buffer[write_byte_index] |= (1 << write_bit);
        } else {
            // 将指定位置的 bit 置为 0
            display_buffer[write_byte_index] &= ~(1 << write_bit);
        }
    }
}

void ST7305_2p9_BW_DisplayDriver::display() {
    address();
    digitalWrite(DC_PIN, HIGH);
    digitalWrite(CS_PIN, LOW);
    spiRef.writeBytes(display_buffer, DISPLAY_BUFFER_LENGTH);
    digitalWrite(CS_PIN, HIGH);
}

// 物理水平线段：py 固定、px 连续。同一字节容纳 4 个连续 px，位组由 py 奇偶决定。
void ST7305_2p9_BW_DisplayDriver::writePhysicalHLine(uint32_t py, int32_t px0, int32_t px1,
                                                     uint16_t color) {
    if (py >= static_cast<uint32_t>(LCD_HIGH)) {
        return;
    }
    if (px0 > px1) {
        const int32_t t = px0;
        px0 = px1;
        px1 = t;
    }
    if (px0 >= LCD_WIDTH || px1 < 0) {
        return;
    }
    if (px0 < 0) {
        px0 = 0;
    }
    if (px1 >= LCD_WIDTH) {
        px1 = LCD_WIDTH - 1;
    }

    const uint32_t row = py / 2U;
    const uint32_t group = py % 2U;
    int32_t x = px0;
    while (x <= px1) {
        const uint32_t byteCol = static_cast<uint32_t>(x) / 4U;
        const uint32_t lane = static_cast<uint32_t>(x) % 4U;
        int32_t count = static_cast<int32_t>(4U - lane);
        if (count > (px1 - x + 1)) {
            count = px1 - x + 1;
        }
        uint8_t mask = 0;
        for (int32_t k = 0; k < count; ++k) {
            const uint32_t shift = 7U - ((lane + static_cast<uint32_t>(k)) * 2U + group);
            mask = static_cast<uint8_t>(mask | (1U << shift));
        }
        uint8_t& target = display_buffer[row * static_cast<uint32_t>(LCD_DATA_WIDTH) + byteCol];
        if (color != 0) {
            target = static_cast<uint8_t>(target | mask);
        } else {
            target = static_cast<uint8_t>(target & static_cast<uint8_t>(~mask));
        }
        x += count;
    }
}

// 物理竖直线段：px 固定、py 连续。同一字节容纳 py 的偶/奇两行，故每次最多写 2 个像素。
void ST7305_2p9_BW_DisplayDriver::writePhysicalVLine(uint32_t px, int32_t py0, int32_t py1,
                                                     uint16_t color) {
    if (px >= static_cast<uint32_t>(LCD_WIDTH)) {
        return;
    }
    if (py0 > py1) {
        const int32_t t = py0;
        py0 = py1;
        py1 = t;
    }
    if (py0 >= LCD_HIGH || py1 < 0) {
        return;
    }
    if (py0 < 0) {
        py0 = 0;
    }
    if (py1 >= LCD_HIGH) {
        py1 = LCD_HIGH - 1;
    }

    const uint32_t byteCol = px / 4U;
    const uint32_t lane = px % 4U;
    int32_t y = py0;
    while (y <= py1) {
        const uint32_t row = static_cast<uint32_t>(y) / 2U;
        const uint32_t group = static_cast<uint32_t>(y) % 2U;
        int32_t count = (group == 0U) ? 2 : 1;
        if (count > (py1 - y + 1)) {
            count = py1 - y + 1;
        }
        uint8_t mask = 0;
        for (int32_t k = 0; k < count; ++k) {
            const uint32_t shift = 7U - (lane * 2U + group + static_cast<uint32_t>(k));
            mask = static_cast<uint8_t>(mask | (1U << shift));
        }
        uint8_t& target = display_buffer[row * static_cast<uint32_t>(LCD_DATA_WIDTH) + byteCol];
        if (color != 0) {
            target = static_cast<uint8_t>(target | mask);
        } else {
            target = static_cast<uint8_t>(target & static_cast<uint8_t>(~mask));
        }
        y += count;
    }
}

void ST7305_2p9_BW_DisplayDriver::drawLogicalRun(int16_t lx, int16_t ly, int16_t len,
                                                 bool horizontal, uint16_t color) {
    if (len <= 0) {
        return;
    }

    const int32_t alongLimit = horizontal ? getDisplayWidth() : getDisplayHeight();
    const int32_t fixedLimit = horizontal ? getDisplayHeight() : getDisplayWidth();
    const int32_t fixed = horizontal ? ly : lx;
    if (fixed < 0 || fixed >= fixedLimit) {
        return;
    }

    int32_t along0 = horizontal ? lx : ly;
    int32_t along1 = along0 + len - 1;
    if (along0 > along1) {
        const int32_t t = along0;
        along0 = along1;
        along1 = t;
    }
    if (along1 < 0 || along0 >= alongLimit) {
        return;
    }
    if (along0 < 0) {
        along0 = 0;
    }
    if (along1 >= alongLimit) {
        along1 = alongLimit - 1;
    }

    const int16_t lx0 = static_cast<int16_t>(horizontal ? along0 : fixed);
    const int16_t ly0 = static_cast<int16_t>(horizontal ? fixed : along0);
    const int16_t lx1 = static_cast<int16_t>(horizontal ? along1 : fixed);
    const int16_t ly1 = static_cast<int16_t>(horizontal ? fixed : along1);

    int16_t px0 = 0;
    int16_t py0 = 0;
    int16_t px1 = 0;
    int16_t py1 = 0;
    logicalToPhysical(lx0, ly0, px0, py0);
    logicalToPhysical(lx1, ly1, px1, py1);

    if (py0 == py1) {
        writePhysicalHLine(static_cast<uint32_t>(py0), px0, px1, color);
    } else {
        writePhysicalVLine(static_cast<uint32_t>(px0), py0, py1, color);
    }
}

void ST7305_2p9_BW_DisplayDriver::drawFastHLine(int16_t x, int16_t y, int16_t len,
                                                uint16_t color) {
    drawLogicalRun(x, y, len, true, color);
}

void ST7305_2p9_BW_DisplayDriver::drawFastVLine(int16_t x, int16_t y, int16_t len,
                                                uint16_t color) {
    drawLogicalRun(x, y, len, false, color);
}

void ST7305_2p9_BW_DisplayDriver::displayRegion(uint16_t x1, uint16_t y1, uint16_t x2,
                                                uint16_t y2) {
    if (x1 > x2) {
        const uint16_t t = x1;
        x1 = x2;
        x2 = t;
    }
    if (y1 > y2) {
        const uint16_t t = y1;
        y1 = y2;
        y2 = t;
    }
    if (x1 >= static_cast<uint16_t>(LCD_WIDTH) || y1 >= static_cast<uint16_t>(LCD_HIGH)) {
        return;
    }
    if (x2 >= static_cast<uint16_t>(LCD_WIDTH)) {
        x2 = static_cast<uint16_t>(LCD_WIDTH - 1);
    }
    if (y2 >= static_cast<uint16_t>(LCD_HIGH)) {
        y2 = static_cast<uint16_t>(LCD_HIGH - 1);
    }

    // 列地址单元 = 3 字节 = 12 像素；行地址单元 = 1 数据行 = 2 像素行。
    // 窗口会对齐到地址单元，因此实际刷新区域可能比请求区域略大。
    const uint16_t colStartUnit = static_cast<uint16_t>((x1 / 4U) / 3U);
    const uint16_t colEndUnit = static_cast<uint16_t>((x2 / 4U) / 3U);
    const uint16_t byteStart = static_cast<uint16_t>(colStartUnit * 3U);
    const uint16_t byteEnd = static_cast<uint16_t>(colEndUnit * 3U + 2U);
    const uint16_t rowStart = static_cast<uint16_t>(y1 / 2U);
    const uint16_t rowEnd = static_cast<uint16_t>(y2 / 2U);
    const size_t bytesPerRow = static_cast<size_t>(byteEnd - byteStart + 1U);

    Write_Register(0x2A);
    Write_Parameter(static_cast<uint8_t>(0x17U + colStartUnit));
    Write_Parameter(static_cast<uint8_t>(0x17U + colEndUnit));

    Write_Register(0x2B);
    Write_Parameter(static_cast<uint8_t>(rowStart));
    Write_Parameter(static_cast<uint8_t>(rowEnd));

    Write_Register(0x2C);

    digitalWrite(DC_PIN, HIGH);
    digitalWrite(CS_PIN, LOW);
    for (uint16_t row = rowStart; row <= rowEnd; ++row) {
        spiRef.writeBytes(
            &display_buffer[static_cast<size_t>(row) * static_cast<size_t>(LCD_DATA_WIDTH) +
                            byteStart],
            bytesPerRow);
    }
    digitalWrite(CS_PIN, HIGH);
}

void ST7305_2p9_BW_DisplayDriver::copyFrameBufferTo(uint8_t* dst) const {
    if (dst == nullptr) {
        return;
    }
    memcpy(dst, display_buffer, DISPLAY_BUFFER_LENGTH);
}

void ST7305_2p9_BW_DisplayDriver::copyFrameBufferFrom(const uint8_t* src) {
    if (src == nullptr) {
        return;
    }
    memcpy(display_buffer, src, DISPLAY_BUFFER_LENGTH);
}

void ST7305_2p9_BW_DisplayDriver::Initial_ST7305() {
    digitalWrite(RES_PIN, HIGH);	
    delay(10);
    digitalWrite(RES_PIN, LOW);
    delay(10);	
    digitalWrite(RES_PIN, HIGH);	
    delay(10);

    // 7305
    Write_Register(0xD6); //NVM Load Control 
    Write_Parameter(0X13); 
    Write_Parameter(0X02);

    Write_Register(0xD1); //Booster Enable 
    Write_Parameter(0X01); 

    Write_Register(0xC0); //Gate Voltage Setting 
    Write_Parameter(0X12); //VGH 00:8V  04:10V  08:12V   0E:15V   12:17V
    Write_Parameter(0X0A); //VGL 00:-5V   04:-7V   0A:-10V

    // VLC=3.6V (12/-5)(delta Vp=0.6V)		
    Write_Register(0xC1); //VSHP Setting (4.8V)	
    Write_Parameter(0X3C); //VSHP1 	
    Write_Parameter(0X3E); //VSHP2 	
    Write_Parameter(0X3C); //VSHP3 	
    Write_Parameter(0X3C); //VSHP4	

    Write_Register(0xC2); //VSLP Setting (0.98V)	
    Write_Parameter(0X23); //VSLP1 	
    Write_Parameter(0X21); //VSLP2 	
    Write_Parameter(0X23); //VSLP3 	
    Write_Parameter(0X23); //VSLP4 	

    Write_Register(0xC4); //VSHN Setting (-3.6V)	
    Write_Parameter(0X5A); //VSHN1	
    Write_Parameter(0X5C); //VSHN2 	
    Write_Parameter(0X5A); //VSHN3 	
    Write_Parameter(0X5A); //VSHN4 	

    Write_Register(0xC5); //VSLN Setting (0.22V)	
    Write_Parameter(0X37); //VSLN1 	
    Write_Parameter(0X35); //VSLN2 	
    Write_Parameter(0X37); //VSLN3 	
    Write_Parameter(0X37); //VSLN4

    Write_Register(0xD8); //OSC Setting                                                                                                                                                                              
    Write_Parameter(0XA6); //Enable OSC, HPM Frame Rate Max = 32hZ
    Write_Parameter(0XE9); 

    /*-- HPM=32hz ; LPM=> 0x15=8Hz 0x14=4Hz 0x13=2Hz 0x12=1Hz 0x11=0.5Hz 0x10=0.25Hz---*/
    Write_Register(0xB2); //Frame Rate Control 
    Write_Parameter(0X12); //HPM=32hz ; LPM=1hz 

    Write_Register(0xB3); //Update Period Gate EQ Control in HPM 
    Write_Parameter(0XE5); 
    Write_Parameter(0XF6); 
    Write_Parameter(0X17);
    Write_Parameter(0X77); 
    Write_Parameter(0X77); 
    Write_Parameter(0X77); 
    Write_Parameter(0X77); 
    Write_Parameter(0X77); 
    Write_Parameter(0X77); 
    Write_Parameter(0X71); 

    Write_Register(0xB4); //Update Period Gate EQ Control in LPM 
    Write_Parameter(0X05); //LPM EQ Control 
    Write_Parameter(0X46); 
    Write_Parameter(0X77); 
    Write_Parameter(0X77); 
    Write_Parameter(0X77); 
    Write_Parameter(0X77); 
    Write_Parameter(0X76); 
    Write_Parameter(0X45); 

    Write_Register(0x62); //Gate Timing Control
    Write_Parameter(0X32);
    Write_Parameter(0X03);
    Write_Parameter(0X1F);

    Write_Register(0xB7); //Source EQ Enable 
    Write_Parameter(0X13); 

    Write_Register(0xB0); //Gate Line Setting 
    Write_Parameter(0X60); //384 line 

    Write_Register(0x11); //Sleep out 
    delay(10); 

    Write_Register(0xC9); //Source Voltage Select  
    Write_Parameter(0X00); //VSHP1; VSLP1 ; VSHN1 ; VSLN1

    Write_Register(0x36); //Memory Data Access Control
    // Write_Parameter(0X00); //Memory Data Access Control: MX=0 ; DO=0 
    Write_Parameter(0X48); //MX=1 ; DO=1 
    // Write_Parameter(0X4c); //MX=1 ; DO=1 GS=1

    Write_Register(0x3A); //Data Format Select 
    Write_Parameter(0X11); //10:4write for 24bit ; 11: 3write for 24bit

    Write_Register(0xB9); //Gamma Mode Setting 
    Write_Parameter(0X20); //20: Mono 00:4GS  

    Write_Register(0xB8); //Panel Setting 
    Write_Parameter(0x29); // Panel Setting: 0x29: 1-Dot inversion, Frame inversion, One Line Interlace

    //WRITE RAM 168*384
    Write_Register(0x2A); //Column Address Setting 
    Write_Parameter(0X17); 
    Write_Parameter(0X24); // 0X24-0X17=14 // 14*12=168

    Write_Register(0x2B); //Row Address Setting 
    Write_Parameter(0X00); 
    Write_Parameter(0XBF); // 192*2=384
    /*
    Write_Register(0x72); //de-stress off 
    Write_Parameter(0X13);
    */
    Write_Register(0x35); //TE
    Write_Parameter(0X00); //

    Write_Register(0xD0); //Auto power dowb OFF
    // Write_Parameter(0X7F); //Auto power dowb OFF
    Write_Parameter(0XFF); //Auto power dowb ON


    Write_Register(0x39); //LPM:Low Power Mode ON
    // Write_Register(0x38); //HPM:high Power Mode ON


    Write_Register(0x29); //DISPLAY ON  
    // Write_Register(0x28); //DISPLAY OFF  

    // Write_Register(0x21); //Display Inversion On 
    Write_Register(0x20); //Display Inversion Off 

    Write_Register(0xBB); // Enable Clear RAM
    Write_Parameter(0x4F); // CLR=0 ; Enable Clear RAM,clear RAM to 0
}

void ST7305_2p9_BW_DisplayDriver::Low_Power_Mode(){
    Write_Register(0x39); //LPM:Low Power Mode ON
}

void ST7305_2p9_BW_DisplayDriver::High_Power_Mode(){
    Write_Register(0x38); //HPM:high Power Mode ON
}

void ST7305_2p9_BW_DisplayDriver::display_on(bool enabled){
    if(enabled){
        Write_Register(0x29); //DISPLAY ON  
    }else{
        Write_Register(0x28); //DISPLAY OFF  
    }
}

void ST7305_2p9_BW_DisplayDriver::display_Inversion(bool enabled){
    if(enabled){
        Write_Register(0x21); //Display Inversion On 
    }else{
        Write_Register(0x20); //Display Inversion Off 
    }
}

void ST7305_2p9_BW_DisplayDriver::address() {
    Write_Register(0x2A);//Column Address Setting S61~S182
    Write_Parameter(0x17);
    Write_Parameter(0x24); // 0X24-0X17=14 // 14*4*3=168

    Write_Register(0x2B);//Row Address Setting G1~G250
    Write_Parameter(0x00);
    Write_Parameter(0xBF); // 192*2=384

    Write_Register(0x2C);   //write image data
}

void ST7305_2p9_BW_DisplayDriver::Write_Register(uint8_t idat) {
    digitalWrite(DC_PIN, LOW);
    digitalWrite(CS_PIN, LOW);
    spiRef.write(idat);
    digitalWrite(CS_PIN, HIGH);
}

void ST7305_2p9_BW_DisplayDriver::Write_Parameter(uint8_t ddat) {
    digitalWrite(DC_PIN, HIGH);
    digitalWrite(CS_PIN, LOW);
    spiRef.write(ddat);
    digitalWrite(CS_PIN, HIGH);
}


ST7305_2p9_BW_DisplayDriver::ST7305_2p9_BW_DisplayDriver(SPIClass& spi)
    : ST7305_2p9_BW_DisplayDriver(4, 0, 3, 2, 1, spi) {
    // Uses default pins defined in DisplayConfig.h
}



// Named-pin overload to avoid argument order mistakes
ST7305_2p9_BW_DisplayDriver::ST7305_2p9_BW_DisplayDriver(const ST73xxPins& pins, SPIClass& spi)
    : ST7305_2p9_BW_DisplayDriver(pins.dc, pins.rst, pins.cs, pins.sclk, pins.sdin, spi) {
}

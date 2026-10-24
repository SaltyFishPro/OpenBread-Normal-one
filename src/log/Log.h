#pragma once

#include <Arduino.h>

// 统一串口日志入口。固件所有串口日志必须经 LogE/LogW/LogI/LogD 输出，
// 禁止在 src/log/Log.h 之外直接调用 Serial.print/printf/println/vprintf。
// 运行时开关复用 UsbSerial（USB CDC 关闭时 if(Serial) 为假，自动静音）。
// 编译期等级：-DOB_LOG_LEVEL=0..4
//   0=全部关闭 1=仅错误 2=错误+警告 3=错误+警告+信息 4=再加调试。

#ifndef OB_LOG_LEVEL
#define OB_LOG_LEVEL 3
#endif

#if OB_LOG_LEVEL >= 1
#define LogE(tag, fmt, ...) \
  do { \
    if (Serial) Serial.printf("[E][" tag "] " fmt "\r\n", ##__VA_ARGS__); \
  } while (0)
#else
#define LogE(tag, fmt, ...) ((void)0)
#endif

#if OB_LOG_LEVEL >= 2
#define LogW(tag, fmt, ...) \
  do { \
    if (Serial) Serial.printf("[W][" tag "] " fmt "\r\n", ##__VA_ARGS__); \
  } while (0)
#else
#define LogW(tag, fmt, ...) ((void)0)
#endif

#if OB_LOG_LEVEL >= 3
#define LogI(tag, fmt, ...) \
  do { \
    if (Serial) Serial.printf("[I][" tag "] " fmt "\r\n", ##__VA_ARGS__); \
  } while (0)
#else
#define LogI(tag, fmt, ...) ((void)0)
#endif

#if OB_LOG_LEVEL >= 4
#define LogD(tag, fmt, ...) \
  do { \
    if (Serial) Serial.printf("[D][" tag "] " fmt "\r\n", ##__VA_ARGS__); \
  } while (0)
#else
#define LogD(tag, fmt, ...) ((void)0)
#endif

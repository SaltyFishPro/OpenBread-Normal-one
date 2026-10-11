#pragma once

#include <Arduino.h>

// USB CDC protocol used by the computer status dashboard.
// The service is active only while the corresponding detail page is open.
class ComputerStatusProtocol {
public:
  static constexpr uint8_t kProtocolMajor = 1;
  static constexpr uint8_t kProtocolMinor = 0;
  static constexpr uint16_t kCanvasWidth = 384;
  static constexpr uint16_t kCanvasHeight = 168;
  // HWCDC has a 256-byte RX queue. One stop-and-wait packet must fit in it,
  // including the 13-byte header and 2-byte CRC.
  static constexpr uint16_t kMaxChunkPayload = 240;

  void beginSession();
  void endSession();
  void tick();
  bool isActive() const;
  bool isHandshakeComplete() const;
  bool hasCompletedFrame() const;
  const uint8_t* completedFrame() const;
  uint16_t completedFrameBytes() const;
  void markFrameDisplayed();

private:
  static constexpr uint32_t kMagic = 0x314F4E52U;
  static constexpr uint8_t kHelloCommand = 0x01;
  static constexpr uint8_t kHelloAckCommand = 0x02;
  static constexpr uint8_t kFrameBeginCommand = 0x10;
  static constexpr uint8_t kFrameChunkCommand = 0x11;
  static constexpr uint8_t kFrameEndCommand = 0x12;
  static constexpr uint8_t kAckCommand = 0x20;
  static constexpr uint8_t kNakCommand = 0x21;
  static constexpr uint16_t kFrameBytes = kCanvasWidth * kCanvasHeight / 8U;
  static constexpr uint16_t kProtocolMaxChunkPayload = 512;
  static constexpr uint8_t kFrameBeginPayloadBytes = 12;
  static constexpr uint8_t kFrameEndPayloadBytes = 8;
  static constexpr uint8_t kAckPayloadBytes = 6;
  static constexpr uint16_t kHeaderBytes = 13;
  static constexpr uint16_t kCrcBytes = 2;
  static constexpr uint16_t kMaxPayloadBytes = kMaxChunkPayload;
  static constexpr uint16_t kBufferBytes = kHeaderBytes + kMaxPayloadBytes + kCrcBytes;
  static_assert(kBufferBytes <= 256U, "USB packet exceeds HWCDC RX capacity");
  static constexpr uint8_t kPollBudget = 128;
  static constexpr uint32_t kPartialPacketTimeoutMs = 250;

  void consumeByte(uint8_t value);
  void processBuffer();
  void handleFrame(const uint8_t* frame, uint16_t frameBytes);
  void handleFrameBegin(uint32_t sequence, const uint8_t* payload, uint16_t payloadLength);
  void handleFrameChunk(uint32_t sequence, const uint8_t* payload, uint16_t payloadLength);
  void handleFrameEnd(uint32_t sequence, const uint8_t* payload, uint16_t payloadLength);
  void sendHelloAck(uint32_t sequence);
  void sendAck(uint32_t sequence, uint8_t acknowledgedCommand, uint32_t value);
  void sendNak(uint32_t sequence, uint16_t reason);
  void resetBuffer();
  static uint16_t crc16(const uint8_t* data, uint16_t length);
  static uint32_t readU32(const uint8_t* data);
  static uint16_t readU16(const uint8_t* data);
  static void writeU32(uint8_t* data, uint32_t value);
  static void writeU16(uint8_t* data, uint16_t value);

  bool active_ = false;
  bool handshakeComplete_ = false;
  bool frameReceiving_ = false;
  bool frameReady_ = false;
  bool frameValidated_ = false;
  uint32_t frameSequence_ = 0;
  uint32_t frameExpectedOffset_ = 0;
  uint32_t frameTotalBytes_ = 0;
  uint16_t frameChunkBytes_ = 0;
  uint16_t frameExpectedChunks_ = 0;
  uint16_t frameReceivedChunks_ = 0;
  uint16_t negotiatedChunkPayload_ = kMaxChunkPayload;
  uint16_t bufferLength_ = 0;
  uint32_t lastByteMs_ = 0;
  uint8_t buffer_[kBufferBytes] = {0};
  uint8_t frameBuffer_[kFrameBytes] = {0};
};

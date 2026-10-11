#include "ComputerStatusProtocol.h"
#include <cstdarg>

namespace {
#ifndef OB_COMPUTER_STATUS_LOG_ENABLED
#define OB_COMPUTER_STATUS_LOG_ENABLED 1
#endif

constexpr uint32_t kDeviceFlags = 0x00000001U;  // full-frame transport supported
constexpr uint32_t kFirmwareBuild = 0x00010000U;
constexpr uint16_t kNakInvalidHello = 1;
constexpr uint16_t kNakUnsupportedVersion = 2;
constexpr uint16_t kNakInvalidFrame = 3;
constexpr uint16_t kNakInvalidState = 4;
constexpr uint16_t kNakInvalidFrameSize = 5;
constexpr uint16_t kNakInvalidChunk = 6;
constexpr uint16_t kNakFrameCrc = 7;

void protocolLog(const char* format, ...) {
#if OB_COMPUTER_STATUS_LOG_ENABLED
  if (!Serial) return;
  Serial.print("[USB] ");
  va_list args;
  va_start(args, format);
  Serial.vprintf(format, args);
  va_end(args);
  Serial.println();
#else
  (void)format;
#endif
}
}

void ComputerStatusProtocol::beginSession() {
  active_ = true;
  handshakeComplete_ = false;
  frameReceiving_ = false;
  frameReady_ = false;
  frameValidated_ = false;
  negotiatedChunkPayload_ = kMaxChunkPayload;
  resetBuffer();
  protocolLog("computer status session begin");
}

void ComputerStatusProtocol::endSession() {
  active_ = false;
  handshakeComplete_ = false;
  frameReceiving_ = false;
  frameReady_ = false;
  frameValidated_ = false;
  resetBuffer();
  protocolLog("computer status session end");
}

void ComputerStatusProtocol::tick() {
  if (!active_ || !Serial) return;

  if (bufferLength_ > 0U && millis() - lastByteMs_ >= kPartialPacketTimeoutMs) {
    protocolLog("partial packet timeout bytes=%u", static_cast<unsigned>(bufferLength_));
    resetBuffer();
  }

  uint8_t consumed = 0;
  while (Serial.available() > 0 && consumed < kPollBudget) {
    const int value = Serial.read();
    if (value < 0) break;
    consumeByte(static_cast<uint8_t>(value));
    ++consumed;
  }
}

bool ComputerStatusProtocol::isActive() const { return active_; }

bool ComputerStatusProtocol::isHandshakeComplete() const { return handshakeComplete_; }

bool ComputerStatusProtocol::hasCompletedFrame() const { return frameReady_; }

const uint8_t* ComputerStatusProtocol::completedFrame() const {
  return frameReady_ ? frameBuffer_ : nullptr;
}

uint16_t ComputerStatusProtocol::completedFrameBytes() const {
  return frameReady_ ? frameTotalBytes_ : 0U;
}

void ComputerStatusProtocol::markFrameDisplayed() { frameReady_ = false; }

void ComputerStatusProtocol::consumeByte(uint8_t value) {
  lastByteMs_ = millis();
  if (bufferLength_ >= kBufferBytes) {
    resetBuffer();
  }
  buffer_[bufferLength_++] = value;
  processBuffer();
}

void ComputerStatusProtocol::processBuffer() {
  while (bufferLength_ >= 4U) {
    if (readU32(buffer_) != kMagic) {
      // Preserve the last three bytes: the next magic can straddle reads.
      memmove(buffer_, buffer_ + 1U, static_cast<size_t>(bufferLength_ - 1U));
      --bufferLength_;
      continue;
    }

    if (bufferLength_ < kHeaderBytes) return;
    const uint32_t payloadLength = readU32(buffer_ + 9U);
    if (payloadLength > kMaxPayloadBytes) {
      memmove(buffer_, buffer_ + 1U, static_cast<size_t>(bufferLength_ - 1U));
      --bufferLength_;
      continue;
    }

    const uint16_t frameBytes = static_cast<uint16_t>(kHeaderBytes + payloadLength + kCrcBytes);
    if (bufferLength_ < frameBytes) return;

    const uint16_t receivedCrc = readU16(buffer_ + kHeaderBytes + payloadLength);
    const uint16_t calculatedCrc = crc16(buffer_ + 4U, static_cast<uint16_t>(9U + payloadLength));
    if (receivedCrc != calculatedCrc) {
      memmove(buffer_, buffer_ + 1U, static_cast<size_t>(bufferLength_ - 1U));
      --bufferLength_;
      continue;
    }

    handleFrame(buffer_, frameBytes);
    const uint16_t remaining = static_cast<uint16_t>(bufferLength_ - frameBytes);
    if (remaining > 0U) memmove(buffer_, buffer_ + frameBytes, remaining);
    bufferLength_ = remaining;
  }
}

void ComputerStatusProtocol::handleFrame(const uint8_t* frame, uint16_t frameBytes) {
  if (frameBytes < kHeaderBytes + kCrcBytes) return;
  const uint8_t command = frame[4];
  const uint32_t sequence = readU32(frame + 5U);
  const uint16_t payloadLength = static_cast<uint16_t>(readU32(frame + 9U));
  const uint8_t* payload = frame + kHeaderBytes;

  if (command == kHelloCommand) {
    if (payloadLength != 12U) {
      sendNak(sequence, kNakInvalidHello);
      return;
    }

    const uint8_t major = payload[0];
    const uint16_t width = readU16(payload + 2U);
    const uint16_t height = readU16(payload + 4U);
    const uint16_t maxChunk = readU16(payload + 6U);
    if (major != kProtocolMajor || width != kCanvasWidth || height != kCanvasHeight ||
        maxChunk < kFrameBeginPayloadBytes || maxChunk > kProtocolMaxChunkPayload) {
      sendNak(sequence, kNakUnsupportedVersion);
      return;
    }

    negotiatedChunkPayload_ = maxChunk < kMaxChunkPayload ? maxChunk : kMaxChunkPayload;
    sendHelloAck(sequence);
    handshakeComplete_ = true;
    frameReceiving_ = false;
    frameReady_ = false;
    frameValidated_ = false;
    protocolLog("HELLO accepted seq=%lu payload=%u", static_cast<unsigned long>(sequence),
                static_cast<unsigned>(negotiatedChunkPayload_));
    return;
  }

  if (!handshakeComplete_) {
    sendNak(sequence, kNakInvalidState);
    return;
  }

  switch (command) {
    case kFrameBeginCommand:
      handleFrameBegin(sequence, payload, payloadLength);
      break;
    case kFrameChunkCommand:
      handleFrameChunk(sequence, payload, payloadLength);
      break;
    case kFrameEndCommand:
      handleFrameEnd(sequence, payload, payloadLength);
      break;
    default:
      sendNak(sequence, kNakInvalidFrame);
      break;
  }
}

void ComputerStatusProtocol::handleFrameBegin(uint32_t sequence, const uint8_t* payload,
                                               uint16_t payloadLength) {
  if (payloadLength != kFrameBeginPayloadBytes) {
    sendNak(sequence, kNakInvalidFrameSize);
    return;
  }

  const uint16_t width = readU16(payload);
  const uint16_t height = readU16(payload + 2U);
  const uint32_t totalBytes = readU32(payload + 4U);
  const uint16_t chunkBytes = readU16(payload + 8U);
  const uint16_t chunkCount = readU16(payload + 10U);
  const uint16_t expectedChunks = chunkBytes == 0U
                                      ? 0U
                                      : static_cast<uint16_t>(
                                            (totalBytes + static_cast<uint32_t>(chunkBytes) - 1U) /
                                            chunkBytes);
  if (width != kCanvasWidth || height != kCanvasHeight || totalBytes != kFrameBytes ||
      chunkBytes == 0U || chunkBytes > negotiatedChunkPayload_ - 4U || chunkCount == 0U ||
      chunkCount != expectedChunks) {
    sendNak(sequence, kNakInvalidFrameSize);
    return;
  }

  frameReceiving_ = true;
  frameReady_ = false;
  frameValidated_ = false;
  frameSequence_ = sequence;
  frameExpectedOffset_ = 0U;
  frameTotalBytes_ = totalBytes;
  frameChunkBytes_ = chunkBytes;
  frameExpectedChunks_ = chunkCount;
  frameReceivedChunks_ = 0U;
  memset(frameBuffer_, 0, sizeof(frameBuffer_));
  sendAck(sequence, kFrameBeginCommand, 0U);
}

void ComputerStatusProtocol::handleFrameChunk(uint32_t sequence, const uint8_t* payload,
                                               uint16_t payloadLength) {
  if (!frameReceiving_ || sequence != frameSequence_ || payloadLength <= 4U ||
      payloadLength > negotiatedChunkPayload_) {
    sendNak(sequence, kNakInvalidState);
    return;
  }

  const uint32_t offset = readU32(payload);
  const uint16_t dataBytes = static_cast<uint16_t>(payloadLength - 4U);
  if (dataBytes > frameChunkBytes_ || offset > frameTotalBytes_ ||
      offset + dataBytes > frameTotalBytes_) {
    sendNak(sequence, kNakInvalidChunk);
    return;
  }

  if (offset == frameExpectedOffset_) {
    memcpy(frameBuffer_ + offset, payload + 4U, dataBytes);
    frameExpectedOffset_ += dataBytes;
    ++frameReceivedChunks_;
    sendAck(sequence, kFrameChunkCommand, frameExpectedOffset_);
    return;
  }

  // A repeated chunk can arrive when its ACK was lost. It is safe to ACK it
  // again because chunks are accepted strictly in order.
  if (offset < frameExpectedOffset_ && offset + dataBytes <= frameExpectedOffset_) {
    sendAck(sequence, kFrameChunkCommand, offset + dataBytes);
    return;
  }

  sendNak(sequence, kNakInvalidChunk);
}

void ComputerStatusProtocol::handleFrameEnd(uint32_t sequence, const uint8_t* payload,
                                             uint16_t payloadLength) {
  if ((!frameReceiving_ && !frameValidated_) || sequence != frameSequence_ ||
      payloadLength != kFrameEndPayloadBytes) {
    sendNak(sequence, kNakInvalidState);
    return;
  }

  const uint32_t totalBytes = readU32(payload);
  const uint16_t expectedCrc = readU16(payload + 4U);
  const uint16_t chunkCount = readU16(payload + 6U);
  if (totalBytes != frameTotalBytes_ || frameExpectedOffset_ != frameTotalBytes_ ||
      frameReceivedChunks_ != frameExpectedChunks_ || chunkCount != frameExpectedChunks_) {
    sendNak(sequence, kNakInvalidChunk);
    return;
  }

  if (crc16(frameBuffer_, static_cast<uint16_t>(frameTotalBytes_)) != expectedCrc) {
    protocolLog("frame CRC reject seq=%lu", static_cast<unsigned long>(sequence));
    sendNak(sequence, kNakFrameCrc);
    return;
  }

  if (frameValidated_) {
    sendAck(sequence, kFrameEndCommand, frameTotalBytes_);
    return;
  }
  frameReceiving_ = false;
  frameReady_ = true;
  frameValidated_ = true;
  sendAck(sequence, kFrameEndCommand, frameTotalBytes_);
  protocolLog("frame ready seq=%lu bytes=%lu", static_cast<unsigned long>(sequence),
              static_cast<unsigned long>(frameTotalBytes_));
}

void ComputerStatusProtocol::sendHelloAck(uint32_t sequence) {
  uint8_t packet[kHeaderBytes + 16U + kCrcBytes] = {0};
  writeU32(packet, kMagic);
  packet[4] = kHelloAckCommand;
  writeU32(packet + 5U, sequence);
  writeU32(packet + 9U, 16U);
  packet[13] = kProtocolMajor;
  packet[14] = kProtocolMinor;
  writeU16(packet + 15U, kCanvasWidth);
  writeU16(packet + 17U, kCanvasHeight);
  writeU16(packet + 19U, negotiatedChunkPayload_);
  writeU32(packet + 21U, kDeviceFlags);
  writeU32(packet + 25U, kFirmwareBuild);
  writeU16(packet + sizeof(packet) - kCrcBytes,
           crc16(packet + 4U, sizeof(packet) - 4U - kCrcBytes));
  Serial.write(packet, sizeof(packet));
}

void ComputerStatusProtocol::sendAck(uint32_t sequence, uint8_t acknowledgedCommand,
                                     uint32_t value) {
  uint8_t packet[kHeaderBytes + kAckPayloadBytes + kCrcBytes] = {0};
  writeU32(packet, kMagic);
  packet[4] = kAckCommand;
  writeU32(packet + 5U, sequence);
  writeU32(packet + 9U, kAckPayloadBytes);
  packet[13] = acknowledgedCommand;
  packet[14] = 0U;
  writeU32(packet + 15U, value);
  writeU16(packet + sizeof(packet) - kCrcBytes,
           crc16(packet + 4U, sizeof(packet) - 4U - kCrcBytes));
  Serial.write(packet, sizeof(packet));
}

void ComputerStatusProtocol::sendNak(uint32_t sequence, uint16_t reason) {
  uint8_t packet[kHeaderBytes + 2U + kCrcBytes] = {0};
  writeU32(packet, kMagic);
  packet[4] = kNakCommand;
  writeU32(packet + 5U, sequence);
  writeU32(packet + 9U, 2U);
  writeU16(packet + 13U, reason);
  writeU16(packet + sizeof(packet) - kCrcBytes,
           crc16(packet + 4U, sizeof(packet) - 4U - kCrcBytes));
  Serial.write(packet, sizeof(packet));
}

void ComputerStatusProtocol::resetBuffer() { bufferLength_ = 0; }

uint16_t ComputerStatusProtocol::crc16(const uint8_t* data, uint16_t length) {
  uint16_t crc = 0xFFFFU;
  for (uint16_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8U; ++bit) {
      crc = (crc & 1U) != 0U ? static_cast<uint16_t>((crc >> 1U) ^ 0xA001U)
                              : static_cast<uint16_t>(crc >> 1U);
    }
  }
  return crc;
}

uint32_t ComputerStatusProtocol::readU32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8U) |
         (static_cast<uint32_t>(data[2]) << 16U) | (static_cast<uint32_t>(data[3]) << 24U);
}

uint16_t ComputerStatusProtocol::readU16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) | static_cast<uint16_t>(data[1] << 8U);
}

void ComputerStatusProtocol::writeU32(uint8_t* data, uint32_t value) {
  data[0] = static_cast<uint8_t>(value & 0xFFU);
  data[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  data[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
  data[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

void ComputerStatusProtocol::writeU16(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value & 0xFFU);
  data[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

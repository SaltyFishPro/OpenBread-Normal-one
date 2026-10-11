#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include "src/services/ComputerStatusProtocol.h"

TestSerial Serial;
uint32_t TestMillis = 0;

#define CHECK(expression) \
  do { if (!(expression)) throw std::runtime_error(#expression); } while (false)

static uint16_t crc16(const uint8_t* data, size_t size) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : crc >> 1;
  }
  return crc;
}

static void append16(std::vector<uint8_t>& dst, uint16_t value) {
  dst.push_back(value & 0xFF);
  dst.push_back(value >> 8);
}

static void append32(std::vector<uint8_t>& dst, uint32_t value) {
  append16(dst, value & 0xFFFF);
  append16(dst, value >> 16);
}

static uint32_t read32(const uint8_t* data) {
  return data[0] | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

static std::vector<uint8_t> encode(uint8_t command, uint32_t sequence,
                                 const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> packet;
  append32(packet, 0x314F4E52);
  packet.push_back(command);
  append32(packet, sequence);
  append32(packet, payload.size());
  packet.insert(packet.end(), payload.begin(), payload.end());
  append16(packet, crc16(packet.data() + 4, packet.size() - 4));
  return packet;
}

static std::vector<uint8_t> exchange(ComputerStatusProtocol& protocol, uint8_t command,
                                   uint32_t sequence, const std::vector<uint8_t>& payload) {
  const auto packet = encode(command, sequence, payload);
  Serial.output.clear();
  // Deliver a complete USB burst before tick() drains the 256-byte RX queue.
  Serial.receive(packet.data(), packet.size());
  CHECK(Serial.droppedBytes == 0);
  while (!Serial.input.empty()) protocol.tick();
  const auto response = Serial.output;
  CHECK(response.size() >= 15);
  CHECK(read32(response.data()) == 0x314F4E52);
  CHECK(read32(response.data() + 5) == sequence);
  CHECK(response.size() == 15 + read32(response.data() + 9));
  const uint16_t receivedCrc = response[response.size() - 2] |
                              static_cast<uint16_t>(response.back() << 8);
  CHECK(receivedCrc == crc16(response.data() + 4, response.size() - 6));
  return response;
}

static void expectAck(const std::vector<uint8_t>& packet, uint8_t command, uint32_t offset) {
  CHECK(packet.size() == 21);
  CHECK(packet[4] == 0x20);
  CHECK(packet[13] == command);
  CHECK(packet[14] == 0);
  CHECK(read32(packet.data() + 15) == offset);
}

int main() {
  try {
    const uint8_t reference[] = "123456789";
    CHECK(crc16(reference, 9) == 0x4B37);
    ComputerStatusProtocol protocol;
    protocol.beginSession();
    std::vector<uint8_t> hello = {1, 0};
    append16(hello, 384);
    append16(hello, 168);
    append16(hello, 512);
    append32(hello, 0);
    auto helloAck = exchange(protocol, 0x01, 1, hello);
    CHECK(helloAck[4] == 0x02);
    CHECK(read32(helloAck.data() + 21) == 1U);
    const uint16_t negotiatedPayload = helloAck[19] | (helloAck[20] << 8);
    CHECK(negotiatedPayload + 15U <= TestSerial::kRxCapacity);

    // Recover magic at every alignment, including after a corrupt packet.
    for (size_t noiseBytes = 1; noiseBytes <= 3; ++noiseBytes) {
      const uint8_t noise[] = {0x5A, 0x66, 0x77};
      Serial.receive(noise, noiseBytes);
      CHECK(exchange(protocol, 0x01, 1, hello)[4] == 0x02);
    }
    auto corruptHello = encode(0x01, 1, hello);
    corruptHello.back() ^= 1;
    Serial.receive(corruptHello.data(), corruptHello.size());
    CHECK(exchange(protocol, 0x01, 1, hello)[4] == 0x02);

    // A truncated packet must not keep a later reconnect waiting for bytes.
    const auto partialHello = encode(0x01, 1, hello);
    Serial.receive(partialHello.data(), 14);
    while (!Serial.input.empty()) protocol.tick();
    TestMillis += 251;
    CHECK(exchange(protocol, 0x01, 1, hello)[4] == 0x02);

    // Honor hosts that support smaller packets, then restore the normal size.
    hello[6] = 64;
    hello[7] = 0;
    helloAck = exchange(protocol, 0x01, 1, hello);
    CHECK(helloAck[19] == 64 && helloAck[20] == 0);
    hello[6] = 0;
    hello[7] = 2;
    CHECK(exchange(protocol, 0x01, 1, hello)[4] == 0x02);

    std::vector<uint8_t> pixels(8064);
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<uint8_t>(i * 31);
    std::vector<uint8_t> begin;
    append16(begin, 384);
    append16(begin, 168);
    append32(begin, pixels.size());
    const uint16_t chunkBytes = negotiatedPayload - 4U;
    const uint16_t chunkCount = (pixels.size() + chunkBytes - 1U) / chunkBytes;
    append16(begin, chunkBytes);
    append16(begin, chunkCount);

    // Receive two frames, exercising a lost chunk ACK and a lost end ACK.
    for (uint32_t sequence = 2; sequence <= 3; ++sequence) {
      expectAck(exchange(protocol, 0x10, sequence, begin), 0x10, 0);
      for (size_t offset = 0; offset < pixels.size(); offset += chunkBytes) {
        const size_t count = std::min<size_t>(chunkBytes, pixels.size() - offset);
        std::vector<uint8_t> chunk;
        append32(chunk, offset);
        chunk.insert(chunk.end(), pixels.begin() + offset, pixels.begin() + offset + count);
        if (offset == 0) {
          const auto partialChunk = encode(0x11, sequence, chunk);
          Serial.receive(partialChunk.data(), 32);
          while (!Serial.input.empty()) protocol.tick();
          TestMillis += 251;
        }
        expectAck(exchange(protocol, 0x11, sequence, chunk), 0x11, offset + count);
        if (offset == 0) expectAck(exchange(protocol, 0x11, sequence, chunk), 0x11, count);
      }
      CHECK(!protocol.hasCompletedFrame());
      std::vector<uint8_t> end;
      append32(end, pixels.size());
      const uint16_t frameCrc = crc16(pixels.data(), pixels.size());
      append16(end, frameCrc ^ 1);
      append16(end, chunkCount);
      CHECK(exchange(protocol, 0x12, sequence, end)[4] == 0x21);
      CHECK(!protocol.hasCompletedFrame());
      end[4] = frameCrc & 0xFF;
      end[5] = frameCrc >> 8;
      expectAck(exchange(protocol, 0x12, sequence, end), 0x12, 8064);
      CHECK(protocol.hasCompletedFrame());
      CHECK(std::equal(pixels.begin(), pixels.end(), protocol.completedFrame()));
      protocol.markFrameDisplayed();
      expectAck(exchange(protocol, 0x12, sequence, end), 0x12, 8064);
      CHECK(!protocol.hasCompletedFrame());
    }
    protocol.endSession();
    CHECK(!protocol.isHandshakeComplete());
    std::puts("PASS: 256-byte USB bursts, negotiation, receive recovery, ACK/NAK CRC, full frame, duplicate chunk/end, frame CRC rejection");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}

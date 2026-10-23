#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "src/bsp/BoardConfig.h"
#include "src/ui/IconBitmap.h"
#include "large_assets.h"

struct Placement {
  int16_t x, y, w, h, top, bottom;
  bool invert;
};

static std::string output;
static bool requireLinear = true;
static unsigned comparisons = 0;
static uint64_t totalReads = 0;

static std::vector<uint8_t> decodeReference(const IconBitmap::Anim& anim, uint16_t index) {
  std::vector<uint8_t> result(((anim.frameWidth + 7U) / 8U) * anim.frameHeight, 0);
  size_t bytes = anim.frameBytes & IconBitmap::kFrameBytesMask;
  const uint8_t* source = anim.frames + bytes * index;
  if (anim.frameBytes == IconBitmap::kPackedRleFrameBytes) {
    const auto offset = [&](uint16_t i) {
      return anim.frames[i * 2U] | (anim.frames[i * 2U + 1U] << 8U);
    };
    source = anim.frames + (anim.frameCount + 1U) * 2U + offset(index);
    bytes = offset(index + 1U) - offset(index);
  }
  size_t decoded = 0;
  for (size_t i = 0; i + 1 < bytes && decoded < result.size(); i += 2) {
    if (!source[i]) break;
    for (unsigned n = 0; n < source[i] && decoded < result.size(); ++n) {
      result[decoded++] = source[i + 1];
    }
  }
  return result;
}

static void verify(ST7305_2p9_BW_DisplayDriver& canvas, const char* name,
                    const IconBitmap::Anim& anim) {
  const int16_t w = anim.frameWidth, h = anim.frameHeight;
  const Placement cases[] = {
      {0, 0, w, h, 0, 167, false}, {226, 9, 150, 150, 0, 167, false},
      {-17, -83, w, h, 10, 120, true}, {170, -19, 210, 230, 20, 160, false},
      {350, 130, 37, 41, 0, 167, true}, {6, -140, 150, 150, 0, 167, false},
      {150, 0, 150, 150, 145, 149, false}, {384, 4, 150, 150, 0, 167, false},
      {0, 0, 150, 150, 200, 210, false}, {20, 15, 300, 3, 0, 167, false},
      {12, 8, 300, 300, 0, 167, true}};
  uint64_t fullReads = 0;
  for (uint16_t index = 0; index < anim.frameCount; ++index) {
    auto decoded = decodeReference(anim, index);
    const IconBitmap::Anim raw = {decoded.data(), static_cast<uint16_t>(decoded.size()),
        anim.frameWidth, anim.frameHeight, anim.frameDelayMs, 1};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
      const auto& p = cases[i];
      uint8_t expected[8064], actual[8064];
      canvas.clearDisplay();
      IconBitmap::drawFrame(canvas, raw, 0, p.x, p.y, p.w, p.h, p.invert, p.top, p.bottom);
      canvas.copyFrameBufferTo(expected);
      canvas.clearDisplay();
      iconByteReads = 0;
      countIconReads = true;
      // frameCount + index also verifies modulo wrapping.
      IconBitmap::drawFrame(canvas, anim, anim.frameCount + index,
                            p.x, p.y, p.w, p.h, p.invert, p.top, p.bottom);
      countIconReads = false;
      if (i == 0) fullReads = iconByteReads;
      totalReads += iconByteReads;
      const bool packed = anim.frameBytes == IconBitmap::kPackedRleFrameBytes;
      size_t budget = anim.frameBytes & IconBitmap::kFrameBytesMask;
      if (packed) {
        const auto* offset = anim.frames + index * 2U;
        const size_t start = offset[0] | (offset[1] << 8U);
        const size_t end = offset[2] | (offset[3] << 8U);
        budget = end - start + 4;  // Two offsets, then each encoded byte at most once.
      }
      if (requireLinear) assert(iconByteReads <= budget);
      if (i == 8) assert(iconByteReads == (packed ? 4U : 0U));
      canvas.copyFrameBufferTo(actual);
      if (std::memcmp(expected, actual, sizeof(actual))) {
        std::fprintf(stderr, "Pixel mismatch: %s frame %u placement %zu\n", name, index, i);
        std::abort();
      }
      const std::string file = output + "/" + name + "-" + std::to_string(index) + "-" + std::to_string(i);
      std::ofstream buffer(file + ".bin", std::ios::binary);
      buffer.write(reinterpret_cast<const char*>(actual), sizeof(actual));
      assert(buffer.good());
      if (i == 0 || i == 3) {
        std::ofstream image(file + ".pgm", std::ios::binary);
        image << "P5\n384 168\n255\n";
        for (int16_t y = 0; y < 168; ++y) for (int16_t x = 0; x < 384; ++x) {
          int16_t px, py;
          canvas.logicalToPhysical(x, y, px, py);
          const size_t pos = size_t(py / 2) * 42U + px / 4;
          const uint8_t mask = 1U << (7 - ((px % 4) * 2 + py % 2));
          image.put(static_cast<char>((actual[pos] & mask) ? 0 : 255));
        }
        assert(image.good());
      }
      ++comparisons;
    }
  }
  std::printf("%s: full render %llu encoded byte reads\n", name,
              static_cast<unsigned long long>(fullReads));
}

int main(int argc, char** argv) {
  assert(argc == 3);
  output = argv[1];
  requireLinear = std::string(argv[2]) == "linear";
  std::filesystem::create_directories(output);
  ST7305_2p9_BW_DisplayDriver canvas(SPI);
  canvas.setRotation(BoardConfig::kDisplayRotation);
  for (const auto& asset : assets) verify(canvas, asset.name, asset.anim);

  // Long runs straddle rows; an early terminator or dangling byte must zero-fill.
  const uint8_t terminated[] = {255, 255, 13, 0xAA, 0, 99, 1, 255};
  const uint8_t truncated[] = {255, 255, 13, 0xAA, 1};
  const uint8_t empty[] = {0, 255};
  const uint8_t* inputs[] = {terminated, truncated, empty};
  const uint16_t sizes[] = {sizeof(terminated), sizeof(truncated), sizeof(empty)};
  const char* names[] = {"terminated", "truncated", "empty"};
  for (unsigned i = 0; i < 3; ++i) {
    const IconBitmap::Anim anim = {inputs[i], static_cast<uint16_t>(0x8000U | sizes[i]),
        150, 150, 42, 1};
    verify(canvas, names[i], anim);
  }

  // Two large packed frames: row-spanning runs followed by one-byte runs.
  std::vector<uint8_t> frames[2];
  frames[0] = {255, 0xAA, 255, 0x55};
  for (unsigned n = 0; n < 9; ++n) frames[0].insert(frames[0].end(), {255, 0});
  frames[0].insert(frames[0].end(), {45, 0});
  for (unsigned n = 0; n < 2850; ++n) {
    frames[1].push_back(1);
    frames[1].push_back(static_cast<uint8_t>(n));
  }
  std::vector<uint8_t> packedData;
  for (size_t offset : {size_t(0), frames[0].size(), frames[0].size() + frames[1].size()}) {
    packedData.push_back(offset & 0xFFU);
    packedData.push_back(offset >> 8U);
  }
  for (const auto& frame : frames) packedData.insert(packedData.end(), frame.begin(), frame.end());
  const IconBitmap::Anim packed = {packedData.data(), IconBitmap::kPackedRleFrameBytes,
      150, 150, 42, 2};
  verify(canvas, "packed", packed);
  std::printf("PASS: %u ST7305 framebuffer comparisons; %zu real large assets; "
              "%llu total encoded byte reads; linear bound %s\n", comparisons,
              sizeof(assets) / sizeof(assets[0]), static_cast<unsigned long long>(totalReads),
              requireLinear ? "enforced" : "measurement only");
}

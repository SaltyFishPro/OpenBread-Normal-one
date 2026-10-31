#include "RacingCarMesh.h"

#include <cmath>
#include <cstring>

#include "../../bsp/DisplayMonoTft.h"
#include "racing_assets/CarMesh.h"
#include "racing_assets/CarTexture.h"

namespace {
constexpr int kWidth = 384;
constexpr int kHeight = 168;
constexpr size_t kFrameBytes = 8064;
constexpr uint8_t kBayer[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

struct Vertex {
  float x;
  float y;
  float z;
  float u;
  float v;
};

Vertex projected[428];
uint16_t order[312];
float depth[312];
uint8_t frame[kFrameBytes];

void putPixel(int x, int y, bool black, uint8_t rotation) {
  if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
  int px = 0;
  int py = 0;
  switch (rotation) {
    case 0: px = x; py = y; break;
    case 1: px = kHeight - 1 - y; py = x; break;
    case 2: px = kWidth - 1 - x; py = kHeight - 1 - y; break;
    default: px = y; py = kWidth - 1 - x; break;
  }
  if (px < 0 || px >= 168 || py < 0 || py >= 384) return;
  const size_t index = static_cast<size_t>(py / 2) * 42U + px / 4;
  const uint8_t mask = static_cast<uint8_t>(1U << (7 - (px % 4) * 2 - py % 2));
  if (black) frame[index] |= mask;
  else frame[index] &= static_cast<uint8_t>(~mask);
}

float edge(float ax, float ay, float bx, float by, float px, float py) {
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void drawTriangle(const Vertex& a, const Vertex& b, const Vertex& c,
                  float light, uint8_t rotation) {
  const float area = edge(a.x, a.y, b.x, b.y, c.x, c.y);
  if (area <= 0.01f) return;
  int minX = static_cast<int>(fminf(a.x, fminf(b.x, c.x)));
  int maxX = static_cast<int>(fmaxf(a.x, fmaxf(b.x, c.x))) + 1;
  int minY = static_cast<int>(fminf(a.y, fminf(b.y, c.y)));
  int maxY = static_cast<int>(fmaxf(a.y, fmaxf(b.y, c.y))) + 1;
  if (maxX < 0 || minX >= kWidth || maxY < 0 || minY >= kHeight) return;
  if (minX < 0) minX = 0;
  if (maxX >= kWidth) maxX = kWidth - 1;
  if (minY < 0) minY = 0;
  if (maxY >= kHeight) maxY = kHeight - 1;
  const float invArea = 1.0f / area;
  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      const float px = x + 0.5f;
      const float py = y + 0.5f;
      const float w0 = edge(b.x, b.y, c.x, c.y, px, py);
      const float w1 = edge(c.x, c.y, a.x, a.y, px, py);
      const float w2 = edge(a.x, a.y, b.x, b.y, px, py);
      if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
      const float u = (w0 * a.u + w1 * b.u + w2 * c.u) * invArea;
      const float v = (w0 * a.v + w1 * b.v + w2 * c.v) * invArea;
      int tx = static_cast<int>(u * (CAR2_TEX_W - 1));
      int ty = static_cast<int>((1.0f - v) * (CAR2_TEX_H - 1));
      if (tx < 0) tx = 0;
      if (tx >= CAR2_TEX_W) tx = CAR2_TEX_W - 1;
      if (ty < 0) ty = 0;
      if (ty >= CAR2_TEX_H) ty = CAR2_TEX_H - 1;
      const uint16_t texel = pgm_read_word(&car2_texture[ty * CAR2_TEX_W + tx]);
      const int r = ((texel >> 11) & 31) * 255 / 31;
      const int g = ((texel >> 5) & 63) * 255 / 63;
      const int blue = (texel & 31) * 255 / 31;
      const int sourceLuminance = (r * 54 + g * 183 + blue * 19) / 256;
      int luminance = static_cast<int>((sourceLuminance - 12) * 2.5f * light + 35.0f);
      if (r > g * 3 / 2 && r > blue * 3 / 2 && r > 60) luminance = 240;
      if (luminance < 0) luminance = 0;
      if (luminance > 255) luminance = 255;
      putPixel(x, y, luminance < kBayer[y & 3][x & 3] * 16, rotation);
    }
  }
}

float faceLight(uint16_t i0, uint16_t i1, uint16_t i2) {
  const Car2Vertex& a = car2_verts[i0];
  const Car2Vertex& b = car2_verts[i1];
  const Car2Vertex& c = car2_verts[i2];
  const float ux = b.x - a.x;
  const float uy = b.y - a.y;
  const float uz = b.z - a.z;
  const float vx = c.x - a.x;
  const float vy = c.y - a.y;
  const float vz = c.z - a.z;
  const float nx = uy * vz - uz * vy;
  const float ny = uz * vx - ux * vz;
  const float nz = ux * vy - uy * vx;
  const float length = sqrtf(nx * nx + ny * ny + nz * nz);
  if (length < 0.0001f) return 0.5f;
  float light = 0.35f + 0.65f * (ny * 0.7f - nz * 0.7f) / length;
  if (light < 0.35f) light = 0.35f;
  if (light > 1.0f) light = 1.0f;
  return light;
}
}  // namespace

void RacingCarMesh::draw(DisplayMonoTft& display, int16_t centerX, int16_t centerY,
                         float yaw, float pitch) {
  display.captureFrame(frame);
  const uint8_t rotation = display.rotation();
  const float cosineYaw = cosf(yaw);
  const float sineYaw = sinf(yaw);
  const float cosinePitch = cosf(pitch);
  const float sinePitch = sinf(pitch);
  for (uint16_t i = 0; i < car2_vert_count; ++i) {
    const Car2Vertex& source = car2_verts[i];
    const float rx = source.x * cosineYaw - source.z * sineYaw;
    const float ry = -(source.y - 0.5f);
    const float rz = source.x * sineYaw + source.z * cosineYaw;
    const float fy = ry * cosinePitch - rz * sinePitch;
    const float fz = ry * sinePitch + rz * cosinePitch + 6.5f;
    Vertex& target = projected[i];
    target.z = fz;
    target.u = source.u;
    target.v = source.v;
    target.x = fz > 0.01f ? centerX + rx * 250.0f / fz : -10000.0f;
    target.y = fz > 0.01f ? centerY + fy * 110.0f / fz : -10000.0f;
  }

  for (int dy = -4; dy <= 4; ++dy) {
    const int half = static_cast<int>(52.0f * sqrtf(1.0f - dy * dy / 25.0f));
    for (int x = centerX - half; x <= centerX + half; ++x) {
      if (((x + dy) & 3) != 0) putPixel(x, centerY + 14 + dy, true, rotation);
    }
  }

  for (uint16_t i = 0; i < car2_tri_count; ++i) {
    const uint16_t i0 = car2_indices[i * 3U];
    const uint16_t i1 = car2_indices[i * 3U + 1U];
    const uint16_t i2 = car2_indices[i * 3U + 2U];
    depth[i] = (projected[i0].z + projected[i1].z + projected[i2].z) / 3.0f;
    order[i] = i;
  }
  for (uint16_t i = 1; i < car2_tri_count; ++i) {
    const uint16_t item = order[i];
    uint16_t j = i;
    while (j > 0 && depth[order[j - 1U]] < depth[item]) {
      order[j] = order[j - 1U];
      --j;
    }
    order[j] = item;
  }
  for (uint16_t n = 0; n < car2_tri_count; ++n) {
    const uint16_t i = order[n];
    const uint16_t i0 = car2_indices[i * 3U];
    const uint16_t i1 = car2_indices[i * 3U + 1U];
    const uint16_t i2 = car2_indices[i * 3U + 2U];
    const Vertex& a = projected[i0];
    const Vertex& b = projected[i1];
    const Vertex& c = projected[i2];
    if (a.z <= 0.01f || b.z <= 0.01f || c.z <= 0.01f) continue;
    drawTriangle(a, b, c, faceLight(i0, i1, i2), rotation);
  }
  display.restoreFrame(frame);
}

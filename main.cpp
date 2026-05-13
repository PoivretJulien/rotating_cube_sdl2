#include "MathTypes.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <print>
#include <string>
#include <vector>

// Constants for the cube definition and window size
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const float CUBE_SIZE = 100.0f;
const float AA_RADIUS_SQ = 1.69f; // 1.3^2 — precomputed to avoid sqrt/pow

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------
struct SDLPoint {
  int x, y;
};

// ---------------------------------------------------------------------------
// Point3D
// ---------------------------------------------------------------------------
struct Point3D {
  float x, y, z;
  inline Point3D to_opengl() { return {x, z, y}; }
  inline void scale(float factor) {
    x *= factor;
    y *= factor;
    z *= factor;
  }
  Point3D operator*(float const factor) {
    return {x * factor, y * factor, z * factor};
  }
};

// ---------------------------------------------------------------------------
// Coordinate conversion  ([-1,1] → [0, W/H])
// ---------------------------------------------------------------------------
inline SDLPoint convert(const Vector3 &v) {
  return {(int)((v.x + 1.0f) * 0.5f * WINDOW_WIDTH),
          (int)((v.y + 1.0f) * 0.5f * WINDOW_HEIGHT)};
}

// ---------------------------------------------------------------------------
// Text rendering helpers
// ---------------------------------------------------------------------------
static void renderText(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                       int x, int y) {
  if (!font || !text)
    return;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Surface *surf = TTF_RenderText_Blended(font, text, white);
  if (!surf)
    return;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
  if (tex) {
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
  }
  SDL_FreeSurface(surf);
}

// Overload for std::string (keeps existing API)
static void renderText(SDL_Renderer *renderer, TTF_Font *font,
                       const std::string &text, int x, int y) {
  renderText(renderer, font, text.c_str(), x, y);
}

static void drawAALineOnPixels(uint8_t *pixels, int pitch, int width,
                               int height, int x1, int y1, int x2, int y2,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  float dx = float(x2 - x1);
  float dy = float(y2 - y1);
  float len2 = dx * dx + dy * dy;

  if (len2 < 1.0f) {
    if (x1 >= 0 && x1 < width && y1 >= 0 && y1 < height) {
      uint32_t *row = (uint32_t *)(pixels + y1 * pitch);
      row[x1] = (r << 24) | (g << 16) | (b << 8) | a;
      /*
      uint8_t *p = pixels + y1 * pitch + x1 * 4;
      p[0] = r;
      p[1] = g;
      p[2] = b;
      p[3] = a;
      */
    }
    return;
  }

  int minX = std::max(0, std::min(x1, x2) - 2);
  int maxX = std::min(width - 1, std::max(x1, x2) + 2);
  int minY = std::max(0, std::min(y1, y2) - 2);
  int maxY = std::min(height - 1, std::max(y1, y2) + 2);

  for (int y = minY; y <= maxY; ++y) {
    uint32_t *row = (uint32_t *)(pixels + y * pitch);

    for (int x = minX; x <= maxX; ++x) {

      float px = x + 0.5f;
      float py = y + 0.5f;

      float t = ((px - x1) * dx + (py - y1) * dy) / len2;
      t = std::clamp(t, 0.0f, 1.0f);

      float cx = x1 + dx * t;
      float cy = y1 + dy * t;

      float dist2 = (px - cx) * (px - cx) + (py - cy) * (py - cy);

      if (dist2 < AA_RADIUS_SQ) {

        float alpha = std::exp(-3.0f * dist2 / AA_RADIUS_SQ) * (a / 255.0f);

        if (alpha > 0.01f) {

          // Read existing pixel
          uint32_t dst = row[x];
          uint8_t dr = (dst >> 16) & 0xFF;
          uint8_t dg = (dst >> 8) & 0xFF;
          uint8_t db = (dst) & 0xFF;
          uint8_t da = (dst >> 24) & 0xFF;

          // Alpha blend
          float inv = 1.0f - alpha;

          uint8_t rr = uint8_t(r * alpha + dr * inv);
          uint8_t gg = uint8_t(g * alpha + dg * inv);
          uint8_t bb = uint8_t(b * alpha + db * inv);
          uint8_t aa = uint8_t(a * alpha + da * inv);

          // FIXED: correct ARGB packing
          // row[x] = SDL_MapRGBA(SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888),rr,
          // gg, bb, aa);
          row[x] = (rr << 24) | (gg << 16) | (bb << 8) | aa;
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// ③ Draw cube edges directly onto a pixel buffer (no per-edge SDL calls)
// ---------------------------------------------------------------------------
static void drawCubeEdgesOnPixels(uint8_t *pixels, int pitch, int width,
                                  int height,
                                  const std::vector<Point3D> &vertices,
                                  const Matrix4x4 &viewMatrix,
                                  const Matrix4x4 &projectionMatrix) {
  std::vector<SDLPoint> screenPoints(vertices.size());

  Matrix4x4 totalTransform = multiply_unrolled(projectionMatrix, viewMatrix);

  for (size_t i = 0; i < vertices.size(); ++i) {
    float x_model = vertices[i].x;
    float y_model = vertices[i].y;
    float z_model = vertices[i].z;

    float x_clip = totalTransform.m[0] * x_model +
                   totalTransform.m[4] * y_model +
                   totalTransform.m[8] * z_model + totalTransform.m[12];
    float y_clip = totalTransform.m[1] * x_model +
                   totalTransform.m[5] * y_model +
                   totalTransform.m[9] * z_model + totalTransform.m[13];

    float w_clip = totalTransform.m[3] * x_model +
                   totalTransform.m[7] * y_model +
                   totalTransform.m[11] * z_model + totalTransform.m[15];

    screenPoints[i] = convert({x_clip / w_clip, y_clip / w_clip, 0});
  }

  static const int edge_indices[][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0}, // back face
      {4, 5}, {5, 6}, {6, 7}, {7, 4}, // front face
      {0, 4}, {1, 5}, {2, 6}, {3, 7}, // connecting
  };

  for (const auto &e : edge_indices)
    drawAALineOnPixels(pixels, pitch, width, height, screenPoints[e[0]].x,
                       screenPoints[e[0]].y, screenPoints[e[1]].x,
                       screenPoints[e[1]].y, 52, 180, 235, 255);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int, char **) {
  TTF_Init();
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL init failed: " << SDL_GetError() << '\n';
    TTF_Quit();
    return 1;
  }

  // ---- Font -----------------------------------------------------------
  TTF_Font *font = TTF_OpenFont("../0xProtoNerdFont-Bold.ttf", 12);
  if (!font)
    std::cerr << "Warning: font not loaded — text rendering disabled.\n";

  // ---- Window & Renderer ----------------------------------------------
  SDL_Window *window =
      SDL_CreateWindow("View Matrix Resolution", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (!window) {
    std::cerr << "Window failed: " << SDL_GetError() << '\n';
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::cerr << "Renderer failed: " << SDL_GetError() << '\n';
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  // ④ Streaming texture — lock once, draw pixels, unlock, blit (1 SDL call)
  SDL_Texture *cubeTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           WINDOW_WIDTH, WINDOW_HEIGHT);

  // ---- Geometry -------------------------------------------------------------
  // z is up there (cpp zero cost abstraction is use later to conform to openGL)
  std::vector<Point3D> my_geometry{
      {1, -1, 0},       {1, 1, 0},       {-1, 1, 0},       {-1, -1, 0},
      {.01, -.01, 2.5}, {.01, .01, 2.5}, {-.01, .01, 2.5}, {-.01, -.01, 2.5},
  };
  for (auto &v : my_geometry)
    v = v.to_opengl() *
        (CUBE_SIZE * 0.25f); // yz swap optimized at compile time.

  // ---- Camera state ---------------------------------------------------
  float angle = 0.0f;
  size_t count = 0;
  int mode = 0;
  Vector3 target = {0, 0, 0};
  Vector3 world_up{0, 1, 0};
  float limit = 360;
  Camera cam = {}; // zero-initialize

  // One-time debug output (moved out of the loop)
  {
    Matrix4x4 m = Matrix4x4::identity();
    m.m[12] = 2;
    m.m[13] = 2;
    m.m[14] = 2;
    Vector3 v{1, 1, 1};
    std::println("Simple vertex translation:");
    std::println("{}", m.to_string_column_major());
    std::println("{}", transform_vertex(m, v).to_string());
  }

  Matrix4x4 projectionMatrix = createPerspectiveProjectionMatrix(
      35.0f, (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 500.0f);

  // Animation mode labels
  const char *modeLabels[] = {
      "animation mode :'spiral up'",
      "animation mode :'target lock'",
      "animation mode :'orbit arc ball (from pivot)'",
      "animation mode :'orbit arc ball (from pivot)'",
  };

  // ---- Main loop ------------------------------------------------------
  bool quit = false;
  bool mouseInside = true;
  bool windowFocused = true;
  SDL_Event e;
  while (!quit) {
    // SDL_WaitEventTimeout(&e, 1)
    while (SDL_WaitEventTimeout(&e, 1)) {
      if (e.type == SDL_QUIT) {
        quit = true;
        break;
      }
      if (e.type == SDL_WINDOWEVENT) {
        switch (e.window.event) {
        case SDL_WINDOWEVENT_ENTER:
          mouseInside = true;
          break;

        case SDL_WINDOWEVENT_LEAVE:
          mouseInside = false;
          break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
          windowFocused = true;
          break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
          windowFocused = false;
          break;
        }
      }
    }
    // Render only when allowed
    if (mouseInside && windowFocused) {
      // ---- View matrix --------------------------------------------------
      Matrix4x4 viewMatrix;
      switch (mode) {
      case 0:
        target = {0, 0, 0};
        viewMatrix = look_at_view(circular_orbit(200, angle, angle * 130),
                                  target, world_up);
        break;
      case 1:
        target = {0, 0, 0};
        viewMatrix = look_at_view(circular_orbit_transversal(400, angle),
                                  target, world_up);
        break;
      case 2:
        target.y = 2.5f * (CUBE_SIZE * 0.25f);
        camera_init_orbit(cam, target, 350);
        camera_orbit(cam, target, angle, 0);
        viewMatrix = camera_view_matrix(cam);
        break;
      case 3:
        target.y = 0;
        camera_init_orbit(cam, target, 500);
        camera_orbit(cam, target, 0, angle);
        viewMatrix = camera_view_matrix(cam);
        break;
      }

      // ---- Clear & draw -------------------------------------------------
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      // ---- Draw cube FIRST (black bg would hide text if drawn after) ----
      void *pixels;
      int pitch;
      SDL_LockTexture(cubeTex, nullptr, &pixels, &pitch);
      std::memset(pixels, 0, pitch * WINDOW_HEIGHT); // clear
      drawCubeEdgesOnPixels((uint8_t *)pixels, pitch, WINDOW_WIDTH,
                            WINDOW_HEIGHT, my_geometry, viewMatrix,
                            projectionMatrix);
      SDL_UnlockTexture(cubeTex);
      SDL_RenderCopy(renderer, cubeTex, nullptr, nullptr); // single blit

      if (mode == 2 || mode == 3)
        // Mode label (rendered directly — cached version fails when font is
        // null)
        renderText(renderer, font, modeLabels[mode], 240, 500);
      else
        renderText(renderer, font, modeLabels[mode], 320, 500);

      auto txt = viewMatrix.to_string_column_major();
      auto a = txt.substr(0, 41);
      auto b = txt.substr(42, 41);
      auto c = txt.substr(84, 41);
      auto d = txt.substr(126, 41);
      renderText(renderer, font, "View Matrix: (Column Major OpenGl Style)", 50,
                 26);
      renderText(renderer, font, "     R         U         F         T", 50,
                 38);
      renderText(renderer, font, a, 50, 50);
      renderText(renderer, font, b, 50, 62);
      renderText(renderer, font, c, 50, 74);
      renderText(renderer, font, d, 50, 86);
      // Zero-allocation formatting into a static buffer (avoids per-frame
      // heap alloc)
      static char rotBuf[64];
      auto rotResult =
          std::format_to_n(rotBuf, sizeof(rotBuf), "rotation: {:5.1f} deg",
                           (angle * 360) / (M_PI * 2));
      if (rotResult.size < sizeof(rotBuf))
        *rotResult.out = '\0';
      renderText(renderer, font, rotBuf, 50, 98);

      SDL_RenderPresent(renderer);

      // ⑧ Remove SDL_Delay — SDL_RenderPresent handles vsync
      // SDL_Delay(16);   // ← removed

      angle += (2 * M_PI / 720.0f);

      count++;
      if (count >= limit) {
        count = 0;
        angle = 0.0f;
        mode++;
        if (mode == 4) {
          limit = 360;
          mode = 0;
          SDL_RenderClear(renderer);
        }
      }
    }
  }

  // ---- Cleanup ----------------------------------------------------------
  SDL_DestroyTexture(cubeTex);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  if (font)
    TTF_CloseFont(font);
  TTF_Quit();
  SDL_Quit();
  return 0;
}

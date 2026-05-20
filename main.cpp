#include "MathTypes.h"
#include "SDL_events.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <vector>

/*
  Todo:
  as it is the reaactivity of the line is break by sdl it self (when i wait for event...)
  i need to more testing, but so far... the slow dwown is not 
  related neither to the text drawing or the cube edge drawing that is not cpp issue
  so how could i get a better reactivity ON event with sdl is the question ? 
*/

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

constexpr float HALF_WIDTH = WINDOW_WIDTH * 0.5f;
constexpr float HALF_HEIGHT = WINDOW_HEIGHT * 0.5f;

// ---------------------------------------------------------------------------
// Converts a 3D point in Normalized Device Coordinates (NDC) to an SDL
// screen-space pixel. The input vector `v` is expected to have x and y in the
// range [-1, 1], where (0,0) is the center of the screen, +X goes right, and +Y
// goes upward (standard NDC). SDL's coordinate system has its origin at the
// top-left corner with +Y downward, so the function flips the Y axis and scales
// both axes to the window dimensions. The result is clamped to valid pixel
// coordinates and returned as an SDLPoint.
// ---------------------------------------------------------------------------
inline SDLPoint convert(const Vec3 &v) {
  int px = static_cast<int>(std::floor((v.x + 1.0f) * HALF_WIDTH));
  int py = static_cast<int>(std::floor((1.0f - v.y) * HALF_HEIGHT));

  px = std::clamp(px, 0, WINDOW_WIDTH - 1);
  py = std::clamp(py, 0, WINDOW_HEIGHT - 1);

  return {px, py};
}

// ---------------------------------------------------------------------------
// Text rendering helpers
// ---------------------------------------------------------------------------
static void renderText(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                       int x, int y) {
  if (!font || !text) {
    std::println("Error font not found");
    return;
  }
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
static void renderText(SDL_Renderer *renderer, TTF_Font *font, char &text,
                       int x, int y) {
  renderText(renderer, font, text, x, y);
}

/**
 * @brief Draws an anti-aliased line with alpha blending onto a pixel buffer.
 *
 * Renders a line from (x1, y1) to (x2, y2) using Gaussian-based
 * anti-aliasing within AA_RADIUS_SQ pixels of the line segment.
 * Each pixel is alpha-blended with the existing framebuffer value.
 *
 * ---
 * How it works:
 *
 * 1. **Degenerate line check** — Computes the squared length of the line.
 *    If it is below 1e-6, the function returns early (no visible line).
 *
 * 2. **Bounding box** — Computes a tight bounding box around the line segment,
 *    expanded by ±2 pixels on each side. The box is clamped to the framebuffer
 *    boundaries so no out-of-bounds memory access occurs.
 *
 * 3. **Per-pixel evaluation** — Iterates over every pixel inside the bounding
 * box. For each pixel:
 *
 *    a. **Closest point on segment** — Projects the pixel center (x+0.5, y+0.5)
 *       onto the line segment using parametric form:
 *         t = ((px-x1)·dx + (py-y1)·dy) / |d|²
 *       The parameter t is clamped to [0, 1] so the closest point stays on the
 *       segment (not on the infinite line), correctly handling end caps.
 *
 *    b. **Distance check** — Computes the squared Euclidean distance from the
 *       pixel center to the closest point. If it exceeds AA_RADIUS_SQ, the
 *       pixel is skipped entirely.
 *
 *    c. **Gaussian alpha** — Computes a per-pixel alpha value using a Gaussian
 *       falloff:
 *         α = exp(-3 · dist² / AA_RADIUS_SQ) · (a / 255)
 *       The factor of 3 ensures the alpha drops to ~5% at the radius boundary.
 *
 *    d. **Alpha-blend** — Blends the new color (r, g, b, a) over the existing
 *       pixel using over-compositing:
 *         new = color · α + dst · (1 - α)
 *       The result is written back as an RGBA8888 uint32 in little-endian
 *       byte order (R in the highest byte).
 *
 * @param pixels  Pointer to the top-left pixel of the framebuffer (RGBA8888, 4
 * bytes per pixel).
 * @param pitch   Number of bytes per row (stride).
 * @param width   Framebuffer width in pixels.
 * @param height  Framebuffer height in pixels.
 * @param x1      X coordinate of the line start point.
 * @param y1      Y coordinate of the line start point.
 * @param x2      X coordinate of the line end point.
 * @param y2      Y coordinate of the line end point.
 * @param r       Red component (0-255).
 * @param g       Green component (0-255).
 * @param b       Blue component (0-255).
 * @param a       Alpha component (0-255).
 */
static void drawAALineCore(uint8_t *pixels, int pitch, int width, int height,
                           float x1, float y1, float x2, float y2, uint8_t r,
                           uint8_t g, uint8_t b, uint8_t a) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  float len2 = dx * dx + dy * dy;

  if (len2 < 1e-6f)
    return;

  float minXf = std::min(x1, x2) - 2.0f;
  float maxXf = std::max(x1, x2) + 2.0f;
  float minYf = std::min(y1, y2) - 2.0f;
  float maxYf = std::max(y1, y2) + 2.0f;

  int minX = std::max(0, int(std::floor(minXf)));
  int maxX = std::min(width - 1, int(std::ceil(maxXf)));
  int minY = std::max(0, int(std::floor(minYf)));
  int maxY = std::min(height - 1, int(std::ceil(maxYf)));

  for (int y = minY; y <= maxY; ++y) {
    uint8_t *row = pixels + y * pitch;

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
        if (alpha <= 0.01f)
          continue;

        uint32_t *p = (uint32_t *)(row + x * 4);
        uint32_t dst = *p;

        uint8_t dr = (dst >> 24) & 0xFF;
        uint8_t dg = (dst >> 16) & 0xFF;
        uint8_t db = (dst >> 8) & 0xFF;
        uint8_t da = (dst) & 0xFF;

        float inv = 1.0f - alpha;

        uint8_t rr = uint8_t(r * alpha + dr * inv);
        uint8_t gg = uint8_t(g * alpha + dg * inv);
        uint8_t bb = uint8_t(b * alpha + db * inv);
        uint8_t aa = uint8_t(a * alpha + da * inv);

        *p = (rr << 24) | (gg << 16) | (bb << 8) | aa;
      }
    }
  }
}

static inline void drawAALineOnPixels(uint8_t *pixels, int pitch, int width,
                                      int height, int x1, int y1, int x2,
                                      int y2, uint8_t r, uint8_t g, uint8_t b,
                                      uint8_t a) {
  drawAALineCore(pixels, pitch, width, height, float(x1), float(y1), float(x2),
                 float(y2), r, g, b, a);
}

static inline void drawAALineOnPixels(uint8_t *pixels, int pitch, int width,
                                      int height, float x1f, float y1f,
                                      float x2f, float y2f, uint8_t r,
                                      uint8_t g, uint8_t b, uint8_t a) {
  drawAALineCore(pixels, pitch, width, height, x1f, y1f, x2f, y2f, r, g, b, a);
}

// ---------------------------------------------------------------------------
// drawCubeEdgesOnPixels — render a wireframe cube directly into a pixel buffer
// -- ( this for now is limited to 8 vertices. )
// Transforms model-space vertices through the combined projection × view
// matrix, clips each edge against all six frustum planes (near, far, left,
// right, top, bottom) using parametric Liang-Barsky / Cohen-Sutherland line
// clipping in clip space, then draws the visible segments onto the supplied
// pixel buffer.
//
// Why clip in clip space (not screen space)?
//   Perspective division (x/w, y/w) is non-linear. Interpolating screen-space
//   coordinates across a clipped edge gives geometrically incorrect positions.
//   By interpolating in clip space and dividing only at the final step we get
//   correct perspective-accurate intersection points.
//
// Algorithm (per edge):
//   1. Transform each vertex to clip space (x_clip, y_clip, z_clip, w_clip).
//   2. Compute the visible t-interval [t_min, t_max] along the edge by clipping
//      against each frustum plane in sequence:
//        Near:   z_clip >= 0
//        Far:    w_clip - z_clip >= 0
//        Left:   x_clip + w_clip >= 0
//        Right:  w_clip - x_clip >= 0
//        Bottom: y_clip + w_clip >= 0
//        Top:    w_clip - y_clip >= 0
//      For each plane, if one endpoint is inside and one is outside, compute
//      the intersection parameter t = v_in / (v_in - v_out) and tighten the
//      interval (t_min = max(t_min, t) for entering, t_max = min(t_max, t)
//      for exiting). If both endpoints are outside the same plane, skip the
//      edge entirely.
//   3. If t_min >= t_max after all planes, the edge has no visible portion.
//   4. Interpolate clip-space x, y, w at t_min and t_max, perspective-divide
//      to get NDC → convert to screen-space pixels.
//   5. Draw the resulting line segment with anti-aliased pixel writing.
//
// Parameters:
//   pixels        — raw pixel buffer (RGBA8888, 4 bytes per pixel)
//   pitch         — byte stride between rows in the pixel buffer
//   width/height  — viewport dimensions in pixels
//   vertices      — model-space cube vertices (8 Point3D)
//   screenPoints  — output array for pre-perspective-divided NDC positions
//   viewMatrix    — camera view matrix (column-major)
//   projectionMatrix — perspective projection matrix (column-major)
//
// Returns: nothing. Edges are written directly into the pixel buffer.
// ---------------------------------------------------------------------------
static void drawCubeEdgesOnPixels(uint8_t *pixels, int pitch, int width,
                                  int height,
                                  const std::vector<Point3D> &vertices,
                                  std::vector<SDLPoint> &screenPoints,
                                  const Matrix4x4 &viewMatrix,
                                  const Matrix4x4 &projectionMatrix) {

  Matrix4x4 totalTransform = multiply_unrolled(projectionMatrix, viewMatrix);

  // Clip-space coords per vertex — stack array avoids heap allocation every
  // frame.
  const size_t N = vertices.size();
  float clip_x[8], clip_y[8], clip_z[8], clip_w[8];

  for (size_t i = 0; i < N; ++i) {
    float x_model = vertices[i].x;
    float y_model = vertices[i].y;
    float z_model = vertices[i].z;

    clip_x[i] = totalTransform.m[0] * x_model + totalTransform.m[4] * y_model +
                totalTransform.m[8] * z_model + totalTransform.m[12];
    clip_y[i] = totalTransform.m[1] * x_model + totalTransform.m[5] * y_model +
                totalTransform.m[9] * z_model + totalTransform.m[13];
    clip_z[i] = totalTransform.m[2] * x_model + totalTransform.m[6] * y_model +
                totalTransform.m[10] * z_model + totalTransform.m[14];
    clip_w[i] = totalTransform.m[3] * x_model + totalTransform.m[7] * y_model +
                totalTransform.m[11] * z_model + totalTransform.m[15];

    // Guard against vertices on or behind the camera eye point.
    if (clip_w[i] <= 0.0f) {
      screenPoints[i] = {-1, -1};
      continue;
    }

    screenPoints[i] =
        convert({clip_x[i] / clip_w[i], clip_y[i] / clip_w[i], 0});
  }

  static const int edge_indices[][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0}, // back face
      {4, 5}, {5, 6}, {6, 7}, {7, 4}, // front face
      {0, 4}, {1, 5}, {2, 6}, {3, 7}, // connecting
  };

  // Clip each edge against all 6 frustum planes using parametric line clipping.
  // The edge is parameterized as P(t) = A + t*(B-A), t ∈ [0, 1].
  // For each plane the "inside" value is:
  //   Near:    z_clip >= 0
  //   Far:     w_clip - z_clip >= 0
  //   Left:    x_clip + w_clip >= 0
  //   Right:   w_clip - x_clip >= 0
  //   Bottom:  y_clip + w_clip >= 0
  //   Top:     w_clip - y_clip >= 0
  // We maintain the visible t-interval [t_min, t_max] and clip it against
  // each plane in sequence (Cohen-Sutherland / Liang-Barsky style).

  float half_width = width * 0.5f;
  float half_height = height * 0.5f;

  for (const auto &e : edge_indices) {
    int a = e[0], b = e[1];
    float t_min = 0.0f, t_max = 1.0f;

    // Early-exit: if either vertex is on/behind the camera, skip the edge.
    if (clip_w[a] <= 0.0f || clip_w[b] <= 0.0f)
      continue;

    // Helper: clip an edge against one frustum plane.
    // Returns true if the edge still has a visible portion after clipping.
    auto clip_against = [&](float va, float vb) -> bool {
      if (va < 0 && vb < 0)
        return false; // both out → skip
      if (va >= 0 && vb >= 0)
        return true; // both in — no change
      float t = va / (va - vb);
      if (va < 0) {
        t_min = std::max(t_min, t); // entering → raise lower bound
      } else {
        t_max = std::min(t_max, t); // exiting → lower upper bound
      }
      return t_min < t_max; // still visible?
    };

    // Near plane
    if (!clip_against(clip_z[a], clip_z[b]))
      continue;
    // Far plane
    if (!clip_against(clip_w[a] - clip_z[a], clip_w[b] - clip_z[b]))
      continue;
    // Left plane
    if (!clip_against(clip_x[a] + clip_w[a], clip_x[b] + clip_w[b]))
      continue;
    // Right plane
    if (!clip_against(clip_w[a] - clip_x[a], clip_w[b] - clip_x[b]))
      continue;
    // Bottom plane
    if (!clip_against(clip_y[a] + clip_w[a], clip_y[b] + clip_w[b]))
      continue;
    // Top plane
    if (!clip_against(clip_w[a] - clip_y[a], clip_w[b] - clip_y[b]))
      continue;

    // Clamp t to [0, 1] for safety (handles edge cases like w_clip ≈ 0).
    t_min = std::max(0.0f, std::min(1.0f, t_min));
    t_max = std::max(0.0f, std::min(1.0f, t_max));
    if (t_min >= t_max)
      continue;

    // Helper: clip-space → screen-space at parameter t.
    // No need to re-clamp here — convert() already clamps to [0, W/H-1].
    auto clipToScreen = [&](float t) -> std::pair<int, int> {
      float ix = clip_x[a] + t * (clip_x[b] - clip_x[a]);
      float iy = clip_y[a] + t * (clip_y[b] - clip_y[a]);
      float iw = clip_w[a] + t * (clip_w[b] - clip_w[a]);
      float ndc_x = ix / iw;
      float ndc_y = iy / iw;
      return {static_cast<int>(std::floor((ndc_x + 1.0f) * half_width)),
              static_cast<int>(std::floor((1.0f - ndc_y) * half_height))};
    };

    auto [sx0, sy0] = clipToScreen(t_min);
    auto [sx1, sy1] = clipToScreen(t_max);

    drawAALineOnPixels(pixels, pitch, width, height, sx0, sy0, sx1, sy1, 52,
                       180, 235, 255);
  }
}

Uint32 WAKE_EVENT = SDL_RegisterEvents(1);
inline void WakeEventLoop(void) {
  SDL_Event ev;
  // Remove old wake events
  while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, WAKE_EVENT, WAKE_EVENT) > 0) {
  }
  // Push a fresh wake event
  SDL_Event wake;
  SDL_zero(wake);
  wake.type = WAKE_EVENT;
  SDL_PushEvent(&wake);
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
      SDL_CreateWindow("View Matrix Study", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (!window) {
    std::cerr << "Window failed: " << SDL_GetError() << '\n';
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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
  //( z is up there, cxx zero cost abstraction is use later to conform to openGL)
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
  Vec3 target = {0, 0, 0};
  Vec3 world_up{0, 1, 0};
  float limit = 360;
  Camera cam = {}; // zero-initialize

  // One-time debug output (moved out of the loop)
  {
    Matrix4x4 m = Matrix4x4::identity();
    m.m[12] = 2;
    m.m[13] = 2;
    m.m[14] = 2;
    Vec3 v{1, 1, 1};
    std::println("Simple vertex translation:");
    std::println("{}", m.to_string_column_major());
    std::println("{}", transform_vertex(m, v).to_string());
  }

  Matrix4x4 projectionMatrix = createPerspectiveProjectionMatrix(
      35.0f, (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 10000.0f);

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
  int mouseX{0};
  int mouseY{0};

  static char rotBuf[21] = {0};
  static char rotBufB[64] = {0};

  // Avoid further inloop reallocation for screenspace points.
  std::vector<SDLPoint> screenPoints(my_geometry.size());

  SDL_Event e;
  bool cursor_in_zone = false;
  while (!quit) {
    // SDL_WaitEventTimeout(&e, 1)
    while (SDL_WaitEvent(&e)) {
      if (e.type == SDL_QUIT) {
        quit = true;
        break;
      } else if (e.type == SDL_WINDOWEVENT) {
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
      } else if (e.type == SDL_MOUSEMOTION) {
            SDL_Event flush;
            while (SDL_PeepEvents(&flush, 1, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION) > 0) {
                mouseX = flush.motion.x;
                mouseY = flush.motion.y;
            }
            if (((mouseX >= 164) && (mouseX <= 617)) &&
            ((mouseY >= 135) && (mouseY <= 494))) {
              cursor_in_zone = true;
            } else {
              cursor_in_zone = false;
            } 
      }

      /*
      else if (e.type == WAKE_EVENT) {
        std::println("animation trig");
      }
      */

      // Render only when allowed.
      if (mouseInside && windowFocused) {
        // ---- View matrix --------------------------------------------------
        Matrix4x4 viewMatrix;
        switch (mode) {
        case 0:
          target = {0, 0, 0};
          viewMatrix = look_at_view(circular_orbit(200, angle, angle * 130),
                                    target, world_up);
          // viewMatrix = look_at_view(Vector3{300,200,-500},
          //                       target, world_up);
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
                              WINDOW_HEIGHT, my_geometry, screenPoints,
                              viewMatrix, projectionMatrix);


        SDL_UnlockTexture(cubeTex);
        SDL_RenderCopy(renderer, cubeTex, nullptr, nullptr); // single blit

        if (mode == 2 || mode == 3)
          renderText(renderer, font, modeLabels[mode], 240, 500);
        else
          renderText(renderer, font, modeLabels[mode], 320, 500);

        auto txt = viewMatrix.to_string_column_major();
        auto a = txt.substr(0, 41);
        auto b = txt.substr(42, 41);
        auto c = txt.substr(84, 41);
        auto d = txt.substr(126, 41);
        renderText(renderer, font, "View Matrix: (Column Major OpenGl Style)",
                   50, 26);
        renderText(renderer, font, "     R         U         F         T", 50,
                   38);
        renderText(renderer, font, a, 50, 50);
        renderText(renderer, font, b, 50, 62);
        renderText(renderer, font, c, 50, 74);
        renderText(renderer, font, d, 50, 86);

        // Zero-allocation formatting into a static buffer (avoids per-frame
        // heap alloc)
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
        if (cursor_in_zone) {
          WakeEventLoop();
        }

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

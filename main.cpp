#include "MathTypes.h"
#include <SDL2/SDL.h>
#include <algorithm> // For std::max, std::min
#include <cmath>
#include <iostream>
#include <vector>

// Constants for the cube definition and window size
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const float CUBE_SIZE = 100.0f;

/**
 * @brief Helper structure to hold a screen point.
 */
struct SDLPoint {
  int x, y;
};

// Structure to hold a 3D point in world space
struct Point3D {
  float x, y, z;
};

// --- Coordinate Conversion Helper ---
/**
 * @brief Converts a 3D vector to an SDL Screen Point using standard perspective
 * projection (assumes Z=0 is the plane closest to the camera for screen
 * coordinates).
 */
SDLPoint convert(const Vector3 &v) {
  // Simple orthographic mapping scaled to window dimensions, assuming Z
  // determines depth/scale. Since we are calculating final_x and final_y from
  // the projection matrix already, this function should just map those
  // normalized values [-1, 1] to pixel space [0, W/H].

  float x = (v.x + 1.0f) * 0.5f * WINDOW_WIDTH;
  float y = (v.y + 1.0f) * 0.5f * WINDOW_HEIGHT;

  return {(int)x, (int)y};
}

// ------------------- RENDERING FUNCTION ----------------------

void drawCubeEdges(SDL_Renderer *renderer, const std::vector<Point3D> &vertices,
                   const Matrix4x4 &viewMatrix,
                   const Matrix4x4 &projectionMatrix) {

  std::vector<SDLPoint> screenPoints;

  for (size_t i = 0; i < vertices.size(); ++i) {
    // Model transformation M = Identity since the cube is stationary in world
    // space. The total transform T = P * V * I = P * V
    Matrix4x4 totalTransform = multiply(projectionMatrix, viewMatrix);

    // 3. TRANSFORM POINT: Apply the final transformation (P * V * I) to get
    // Clip Space coordinates
    float x_model = vertices[i].x;
    float y_model = vertices[i].y;
    float z_model = vertices[i].z;

    // Applying the full 4x4 View-Projection transformation (Homogeneous
    // Coordinates):
    float x_clip = totalTransform.m[0] * x_model +
                   totalTransform.m[4] * y_model +
                   totalTransform.m[8] * z_model + totalTransform.m[12];
    float y_clip = totalTransform.m[1] * x_model +
                   totalTransform.m[5] * y_model +
                   totalTransform.m[9] * z_model + totalTransform.m[13];
    // float z_clip = ...

    float w_clip = totalTransform.m[3] * x_model +
                   totalTransform.m[7] * y_model +
                   totalTransform.m[11] * z_model + totalTransform.m[15];

    // 4. PERSPECTIVE DIVISION & SCREEN PROJECTION: Convert Clip Space to
    // View/Screen Space
    float final_x = x_clip / w_clip;
    float final_y = y_clip / w_clip;

    // Use the defined coordinate conversion helper
    screenPoints.push_back(convert({final_x, final_y, 0}));
  }

  // Define the 12 edges by indices into the vertices array (0-7)
  const int edge_indices[][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Back face
      {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Front face
      {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connecting vertical edges
  };

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White lines

  for (const auto &edge : edge_indices) {
    int start_idx = edge[0];
    int end_idx = edge[1];

    // Draw line between the projected points
    SDL_RenderDrawLine(renderer, screenPoints[start_idx].x,
                       screenPoints[start_idx].y, screenPoints[end_idx].x,
                       screenPoints[end_idx].y);
  }
}

int main(int argc, char *argv[]) {
  // --- SDL Initialization ---
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL could not initialize! SDL Error: " << SDL_GetError()
              << std::endl;
    return 1;
  }

  SDL_Window *window =
      SDL_CreateWindow("Animated Camera Cube (App3)", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (!window) {
    std::cerr << "Window could not be created! SDL Error: " << SDL_GetError()
              << std::endl;
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::cerr << "Renderer could not be created! SDL Error: " << SDL_GetError()
              << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // --- Cube Setup (Centered, Stationary) ---
  std::vector<Point3D> cubeVertices = {
      {-CUBE_SIZE / 2, -CUBE_SIZE / 2, CUBE_SIZE / 2},  // 0: Back bottom left
      {CUBE_SIZE / 2, -CUBE_SIZE / 2, CUBE_SIZE / 2},   // 1: Back bottom right
      {CUBE_SIZE / 2, CUBE_SIZE / 2, CUBE_SIZE / 2},    // 2: Back top right
      {-CUBE_SIZE / 2, CUBE_SIZE / 2, CUBE_SIZE / 2},   // 3: Back top left
      {-CUBE_SIZE / 2, -CUBE_SIZE / 2, -CUBE_SIZE / 2}, // 4: Front bottom left
      {CUBE_SIZE / 2, -CUBE_SIZE / 2, -CUBE_SIZE / 2},  // 5: Front bottom right
      {CUBE_SIZE / 2, CUBE_SIZE / 2, -CUBE_SIZE / 2},   // 6: Front top right
      {-CUBE_SIZE / 2, CUBE_SIZE / 2, -CUBE_SIZE / 2}   // 7: Front top left
  };

  // --- Camera Control Variables ---
  float cameraAngleY = 0.0f; // Yaw (rotation around Y-axis)
  // float cameraAngleX = -M_PI / 4.0f; // Pitch (looking slightly down
  // initially)
  float cameraAngleX = 0.f; // Pitch
  const float CAMERA_DISTANCE = 700.0f;

  // incremental counter.
  size_t count = 0;

  // --- Main Loop ---
  bool quit = false;
  SDL_Event e;

  while (!quit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        quit = true;
      }
      // Allow camera movement on mouse drag
      if (e.type == SDL_MOUSEMOTION && e.button.button == SDL_BUTTON_LEFT) {
        // Simple increment for demonstration, ideally needs state tracking
        cameraAngleY += 0.001f;
        cameraAngleX = std::max(
            -static_cast<float>(M_PI) / 2.0f,
            std::min(static_cast<float>(M_PI) / 2.0f, cameraAngleX + 0.001f));
      }
      // Allow manual reset or exit on key press
      if (e.type == SDL_KEYDOWN &&
          (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)) {
        quit = true;
      }
    }

    // 1. Camera Logic: Calculate view basis vectors and View Matrix
    Vector3 origin, target;
    float cameraYOffset = -50.0f; // Keep camera viewing height constant

    // Use spherical coordinates for smooth rotation around the center point (0,
    // 0, 0)
    float x_cam =
        CAMERA_DISTANCE * std::cos(cameraAngleY) * std::cos(cameraAngleX);
    float y_cam = cameraYOffset +
                  CAMERA_DISTANCE *
                      std::sin(cameraAngleX); // Y component adjusted by offset
    float z_cam =
        CAMERA_DISTANCE * std::sin(cameraAngleY) * std::cos(cameraAngleX);

    origin = {x_cam, y_cam, z_cam}; // Camera Position
    target = {0.0f, 0.0f, 0.0f};    // Focus point (at the origin of the world)
    
    // Build the View Matrix
    Matrix4x4 viewMatrix = createViewMatrix(origin, target);

    // 2. Projection Setup (Remains constant)
    Matrix4x4 projectionMatrix = createPerspectiveProjectionMatrix(
        35.0f, (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 500.0f);

    // 3. Drawing Steps
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black screen
    SDL_RenderClear(renderer);

    drawCubeEdges(renderer, cubeVertices, viewMatrix, projectionMatrix);

    // Update the screen with rendering commands
    SDL_RenderPresent(renderer);

    // Control frame rate (approximate)
    SDL_Delay(16); // ~60 FPS

    // Small animation.
    cameraAngleY += (M_PI / 360.0f);
    count++;
    if (count >= 720) {
      count = 0;
      cameraAngleY = 0.0f;
    }
  }

  // --- Cleanup ---
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

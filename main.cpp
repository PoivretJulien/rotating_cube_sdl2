#include "MathTypes.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm> // For std::max, std::min
#include <cmath>

#include <cstddef>
#include <iostream>
#include <print>
#include <string>
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
  /**
      (swap z and y)
  */ 
  constexpr inline Point3D to_opengl(){
    return {x,z,y};
  } 
  inline void scale(float factor){
    x*=factor;
    y*=factor;
    z*=factor;
  }

  Point3D& operator * (float const& factor){
    x*=factor; 
    y*=factor; 
    z*=factor; 
    return *this;
  }
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
/**
 * @brief Renders text using a pre-loaded font pointer. (Optimized)
 */
void renderText(SDL_Renderer *renderer, TTF_Font *font, const std::string &text,
                int x, int y) {
  // Check if the font pointer is valid
  if (!font)
    return;

  SDL_Color color = {255, 255, 255, 255}; // white

  // Render text to a surface
  SDL_Surface *surface = TTF_RenderText_Blended(font, text.c_str(), color);
  if (!surface)
    return; // Handle rendering failure

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    SDL_FreeSurface(surface);
    return;
  }

  SDL_Rect dst;
  dst.x = x;
  dst.y = y;
  dst.w = surface->w;
  dst.h = surface->h;

  // Cleanup temporary resources immediately
  SDL_FreeSurface(surface);
  SDL_RenderCopy(renderer, texture, nullptr, &dst);
  SDL_DestroyTexture(texture);
}

// Defines how far from the line a pixel center can be to still contribute to
// AA. This determines the thickness of the anti-aliased edge. 1.0 is a good
// starting point.
const float AA_RADIUS = 1.3f;

/**
 * @brief Draws an anti-aliased line segment using distance weighting and exponential falloff.
 * 
 * This function iterates over every pixel within the bounding box of the specified line 
 * segment (expanded by AA_RADIUS). For each pixel, it calculates the perpendicular 
 * distance to the true mathematical path of the line. Pixels closer to the line 
 * receive a higher opacity value (alpha), creating a smooth anti-aliased edge.
 * 
 * The alpha weighting uses an exponential decay model (e^(-3*d^2/R^2)) for a natural, 
 * gradual fade effect near the line center.
 * 
 * @param renderer Pointer to the SDL rendering context used for drawing primitives.
 * @param x1 Starting X coordinate of the line segment.
 * @param y1 Starting Y coordinate of the line segment.
 * @param x2 Ending X coordinate of the line segment.
 * @param y2 Ending Y coordinate of the line segment.
 * @param color The base RGB and alpha (0-255) color for the line.
 * 
 * @pre Requires SDL_BLENDMODE_BLEND to be set on the renderer before calling.
 * 
 * @note For lines with zero or near-zero length, a single point is drawn instead.
 * @note Performance complexity: O(W*H), where W and H are dimensions of the bounding box + 2*AA_RADIUS.
 */
void SDL_RenderAALine(SDL_Renderer* renderer,
                      int x1, int y1, int x2, int y2,
                      const SDL_Color& color)
{
    float dx = float(x2 - x1);
    float dy = float(y2 - y1);
    float len2 = dx*dx + dy*dy;

    // Degenerate line → draw a point
    if (len2 < 1.0f) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        SDL_RenderDrawPoint(renderer, x1, y1);
        return;
    }

    float invLen = 1.0f / std::sqrt(len2);

    // Expand bounding box by AA radius
    int minX = std::min(x1, x2) - int(AA_RADIUS);
    int maxX = std::max(x1, x2) + int(AA_RADIUS);
    int minY = std::min(y1, y2) - int(AA_RADIUS);
    int maxY = std::max(y1, y2) + int(AA_RADIUS);

    // Pre-set RGB once (alpha changes per pixel)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {

            float px = x + 0.5f;
            float py = y + 0.5f;

            // Project pixel onto the segment using t = dot / |D|²
            float t = ((px - x1) * dx + (py - y1) * dy) / len2;
            t = std::clamp(t, 0.0f, 1.0f);

            // Closest point on the segment
            float cx = x1 + dx * t;
            float cy = y1 + dy * t;

            // Perpendicular distance (no pow, no sqrt)
            float dist = std::hypot(px - cx, py - cy);

            if (dist < AA_RADIUS) {
                //float alpha = 1.0f - (dist / AA_RADIUS);
                float alpha = std::exp(-3.0f * (dist*dist) / (AA_RADIUS*AA_RADIUS));

                uint8_t a = uint8_t(alpha * color.a);

                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, a);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
}


void drawCubeEdges(SDL_Renderer *renderer, const std::vector<Point3D> &vertices,
                   const Matrix4x4 &viewMatrix,
                   const Matrix4x4 &projectionMatrix) {

  std::vector<SDLPoint> screenPoints;

  // Model transformation M = Identity since the cube is stationary in world
  // space. Calculate Total Transform T = P * V * I = P * V once per frame.
  Matrix4x4 totalTransform = multiply_unrolled(projectionMatrix, viewMatrix);

  for (size_t i = 0; i < vertices.size(); ++i) {
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
                                                        /*
                                                        for (const auto &edge : edge_indices) {
                                                          int start_idx = edge[0];
                                                          int end_idx = edge[1];
                                                      
                                                          // Draw line between the projected points
                                                          SDL_RenderDrawLine(renderer, screenPoints[start_idx].x,
                                                          screenPoints[start_idx].y, screenPoints[end_idx].x,
                                                          screenPoints[end_idx].y);
                                                        }
                                                        */
  SDL_Color line_color = {255, 255, 255, 255}; // Define the desired color once
  for (const auto &edge : edge_indices) {
    int start_idx = edge[0];
    int end_idx = edge[1];
    SDL_RenderAALine(renderer, screenPoints[start_idx].x,
                     screenPoints[start_idx].y, screenPoints[end_idx].x,
                     screenPoints[end_idx].y,
                     line_color); // <-- Use the new function
  }
}

Vector3 circular_orbit(float radius, float angle_radians, float height)
{
    Vector3 pos;
     pos.x = radius * cos(angle_radians);
     pos.z = radius * sin(angle_radians);
     pos.y = height;
    return pos;
}

Vector3 circular_orbit_transversal(float radius, float angle_radians)
{
     Vector3 pos;
     pos.z = radius * cos(angle_radians);
     pos.y = radius * sin(angle_radians);
     pos.x = 100;   
    return pos;
}

int main(int argc, char *argv[]) {
  // --- SDL Initialization ---
  TTF_Init();
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL could not initialize! SDL Error: " << SDL_GetError()
              << std::endl;
    return 1;
  }

  // --- Font Setup (Pre-load font once for performance) ---
  TTF_Font *font =
      TTF_OpenFont("../0xProtoNerdFont-Bold.ttf", 12); // Load at a default size
  if (!font) {
    std::cerr
        << "Warning: Could not load font! Text rendering will be disabled."
        << std::endl;
  }

  SDL_Window *window =
      SDL_CreateWindow("Animated Camera Cube (App3)", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (!window) {
    std::cerr << "Window could not be created! SDL Error: " << SDL_GetError()
              << std::endl;
    TTF_Quit(); // Clean up TTF resources before exiting main
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::cerr << "Renderer could not be created! SDL Error: " << SDL_GetError()
              << std::endl;
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

 // ( world system z up )
    std::vector<Point3D> my_geometry{
    {1,-1,0},
    {1,1,0},
    {-1,1,0},
    {-1,-1,0},
    {.01,-.01,2.5},
    {.01,.01,2.5},
    {-.01,.01,2.5},
    {-.01,-.01,2.5},
  };

  for(auto& vertex:my_geometry){
    vertex = vertex.to_opengl()*(CUBE_SIZE*0.25);
  }

/*
float tip =100;
// --- Cube Setup (Centered, Stationary) ---
std::vector<Point3D> cubeVertices = {
  {-CUBE_SIZE / 2, -CUBE_SIZE / 2, CUBE_SIZE / 2},  // 0: Back bottom left
  {CUBE_SIZE / 2, -CUBE_SIZE / 2, CUBE_SIZE / 2},   // 1: Back bottom right
  {CUBE_SIZE / tip, CUBE_SIZE / tip, CUBE_SIZE / tip},    // 2: Back top right
  {-CUBE_SIZE / tip, CUBE_SIZE / tip, CUBE_SIZE / tip},   // 3: Back top left
  {-CUBE_SIZE / 2, -CUBE_SIZE / 2, -CUBE_SIZE / 2}, // 4: Front bottom left
  {CUBE_SIZE / 2, -CUBE_SIZE / 2, -CUBE_SIZE / 2},  // 5: Front bottom right
  {CUBE_SIZE / tip, CUBE_SIZE / tip, -CUBE_SIZE / tip},   // 6: Front top right
  {-CUBE_SIZE / tip, CUBE_SIZE / tip, -CUBE_SIZE / tip}   // 7: Front top left
};
*/

  // --- Camera Control Variables ---
  float angle = 0.0f; // Yaw (rotation around Y-axis)


  // incremental counter.
  size_t count = 0;

  // --- Main Loop ---
  bool quit = false;
  SDL_Event e;
 
  auto m = Matrix4x4::indentity();
  // translation vector.
  m.m[12] = 2;
  m.m[13] = 2;
  m.m[14] = 2;

  Vector3 v{1, 1, 1};
  Vector3 result = transform_vertex(m, v);
  auto txt = m.to_string();
  std::println("Simple vertex translation:");
  std::println("{0}", m.to_string_column_major());
  std::println("{0}", result.to_string());

  // Projection Setup (Remains constant)
  Matrix4x4 projectionMatrix = createPerspectiveProjectionMatrix(
        35.0f, (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 500.0f);
  bool mode = true;
  while (!quit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        quit = true;
      }
      // Allow camera movement on mouse drag
      if (e.type == SDL_MOUSEMOTION && e.button.button == SDL_BUTTON_LEFT) {
        // Simple increment for demonstration, ideally needs state tracking
        angle += 0.001f;
      }
      // Allow manual reset or exit on key press
      if (e.type == SDL_KEYDOWN &&
          (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)) {
        quit = true;
      }
    }

    // 1. Camera Logic: Calculate view basis vectors and View Matrix
    Vector3 origin, target;
    // origin = {250,-200,500};
    if(mode)
      origin = circular_orbit(200, angle, angle*130);
    else
      origin = circular_orbit_transversal(400, angle);
    target = {0.0f, 0.0f, 0.0f};    // Focus point (at the origin of the world)

    // Build the View Matrix
    // Matrix4x4 viewMatrix = createViewMatrix(origin, target);
    Vector3 world_up{0,1,0};
    Matrix4x4 viewMatrix = look_at_view_quat(origin, target ,world_up);
    //Matrix4x4  viewMatrix = BuildViewMatrixFromEuler(0, -0.5,0, Vector3{0,400,700});
    //Matrix4x4  viewMatrix = BuildViewMatrixFromEuler(3.14/13, -3.14/3.2,0, Vector3{-100,400,200});
    
    // 2. Drawing Steps
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black screen
    SDL_RenderClear(renderer);

    // Parse view matrix on screen
    auto txt = viewMatrix.to_string_column_major();
    auto a = txt.substr(0, 41);
    auto b = txt.substr(42, 41);
    auto c = txt.substr(84, 41);
    auto d = txt.substr(126, 41);


    renderText(renderer, font, "View Matrix: (Column Major OpenGl style)", 50,
               26);
    renderText(renderer, font, "     R         U         F         T", 50, 38);
    renderText(renderer, font, a, 50, 50);
    renderText(renderer, font, b, 50, 62);
    renderText(renderer, font, c, 50, 74);
    renderText(renderer, font, d, 50, 86);
    renderText(renderer, font, std::format("rotation: {0:5.1f} deg",(angle*360)/(M_PI*2)), 50, 98);
    if(mode)
      renderText(renderer, font, "animation mode :'spiral up'", 320, 500);
    else
      renderText(renderer, font, "animation mode :'target lock'", 320, 500);
      

    //drawCubeEdges(renderer, cubeVertices, viewMatrix, projectionMatrix); 
    drawCubeEdges(renderer, my_geometry, viewMatrix, projectionMatrix);

    // Update the screen with rendering commands
    SDL_RenderPresent(renderer);

    // Control frame rate (approximate)
    SDL_Delay(16); // ~60 FPS

    // Small animation.
    angle += (2*M_PI / 720.0f);
    count++;
    if (count >= 360) {
      count = 0;
      angle = 0.0f;
      mode = !mode;
    }
  }

  // --- Cleanup ---
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  TTF_CloseFont(font); // Clean up the pre-loaded font resource
  TTF_Quit();
  SDL_Quit();

  return 0;
}
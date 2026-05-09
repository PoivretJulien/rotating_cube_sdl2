// MathTypes.h - Contains common math structures and declarations for App3 3D
// rendering.
#ifndef MATH_TYPES_H
#define MATH_TYPES_H

#include <cmath>
#include <format>
#include <iostream> // Kept for compatibility if debugging, but ideally should be minimized.
#include <string>
// Define PI if not available (common in some environments)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Math Structures ---

/**
 * @brief Vector structure used throughout the math library.
 */
struct Vector3 {
  float x, y, z;
  inline std::string to_string() {
    return std::format("({0},{1},{2})", x, y, z);
  }
};

/**
 * @brief 4x4 Matrix stored in column-major order (m[row + col*4])
 *        Assuming standard OpenGL/DirectX indexing convention for consistency
 * with rendering logic.
 */
struct Matrix4x4 {
  // m[0] m[4] m[8] m[12] -> X components (Row 0)
  // m[1] m[5] m[9] m[13] -> Y components (Row 1)
  // m[2] m[6] m[10] m[14] -> Z components (Row 2)
  // m[3] m[7] m[11] m[15] -> W components (Row 3)
  float m[16];

  Matrix4x4() :m{
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1,
  }{}

  Matrix4x4(int) :m{
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
  }{}

  /**
   * @brief Converts the 4x4 matrix into a readable string representation.
   *
   * Formats the matrix row by row, which is useful for debugging or outputting
   * to a system like an SDL framebuffer console logging.
   * Assumes column-major storage internally but outputs in row-major order
   * (R0C0, R0C1...).
   *
   * @return std::string The string representation of the matrix.
   */
  inline std::string to_string() {
    return std::format("|{0:^9.3f},{1:^9.3f},{2:^9.3f},{3:^9.3f}|\n"
                       "|{4:^9.3f},{5:^9.3f},{6:^9.3f},{7:^9.3f}|\n"
                       "|{8:^9.3f},{9:^9.3f},{10:^9.3f},{11:^9.3f}|\n"
                       "|{12:^9.3f},{13:^9.3f},{14:^9.3f},{15:^9.3f}|\n",
                       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8],
                       m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
  }

  inline std::string to_string_column_major() {
    return std::format("|{0:^9.3f},{1:^9.3f},{2:^9.3f},{3:^9.3f}|\n"
                       "|{4:^9.3f},{5:^9.3f},{6:^9.3f},{7:^9.3f}|\n"
                       "|{8:^9.3f},{9:^9.3f},{10:^9.3f},{11:^9.3f}|\n"
                       "|{12:^9.3f},{13:^9.3f},{14:^9.3f},{15:^9.3f}|\n",
                       m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2],
                       m[6], m[10], m[14], m[3], m[7], m[11], m[15]);
  }
};

// --- Linear Algebra Function Declarations ---

/**
 * @brief Multiplies two matrices: Result = A * B (A is on the left).
 */
Matrix4x4 multiply(const Matrix4x4 &A, const Matrix4x4 &B);

/**
 * @brief Multiplies two matrices: Result = A * B (A is on the left).
 */
Matrix4x4 multiply_unrolled(const Matrix4x4 &A, const Matrix4x4 &B);

/**
 * @brief Creates a Perspective Projection Matrix (P).
 */
Matrix4x4 createPerspectiveProjectionMatrix(float fovY_deg, float aspectRatio,
                                            float nearZ, float farZ);

Vector3 transform_vertex(const Matrix4x4 &M, const Vector3 &v);

Matrix4x4 look_at_view(const Vector3& eye,
                     const Vector3& target,
                     const Vector3& world_up = {0,1,0});

Matrix4x4 look_at_view_quat(const Vector3& eye,
                     const Vector3& target, Vector3& world_up);

inline Vector3 subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline float normalize(Vector3& v) {
    float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length > 1e-6f) { // Avoid division by zero or near-zero lengths
        v.x /= length;
        v.y /= length;
        v.z /= length;
    }
    return length;
}

inline float dot_product(const Vector3& a, const Vector3& b) {
    return 
        a.x*b.x+a.y*b.y+a.z*b.z
    ;
}



inline Vector3 cross_product(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
#endif // MATH_TYPES_H
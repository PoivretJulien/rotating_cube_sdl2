// MathTypes.h - Contains common math structures and declarations for App3 3D
// rendering.
#ifndef MATH_TYPES_H
#define MATH_TYPES_H

#include <cmath>
#include <cstdio>
#include <format>
#include <string>

// Define PI if not available (common in some environments)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Math Structures ---

/**
 * @brief Vector structure used throughout the math library.
 */
struct Vec3 {
  float x, y, z;
  inline Vec3 to_opengl() const{
    return {x,z,y};
  }
  inline std::string to_string() const {
    return std::format("({0},{1},{2})", x, y, z);
  }
  Vec3 operator*(float factor){
       return {x*factor,y*factor,z*factor};
  } 
  Vec3& operator+=(Vec3 const& oth){
       x+=oth.x;
       y+=oth.y;
       z+=oth.z;
       return *this;
  } 
  Vec3 operator+(const Vec3& oth) const{
       return {x+oth.x,y+oth.y,z+oth.z};
  } 
};

inline Vec3 subtract(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 cross_product(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float normalize(Vec3& v) {
    float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length > 1e-6f) { // Avoid division by zero or near-zero lengths
        v.x /= length;
        v.y /= length;
        v.z /= length;
    }
    return length;
}

inline float dot_product(const Vec3& a, const Vec3& b) {
    return 
        a.x*b.x+a.y*b.y+a.z*b.z
    ;
}
inline float length(const Vec3& v){
    return 
        std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}


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

  inline static Matrix4x4 identity(){
    return {
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1,
  };
  }

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

  /**
   * @brief Formats the matrix into a pre-allocated buffer (zero heap alloc).
   *        Outputs in column-major order (R0C0, R1C0, ... R3C3, R0C1, ...)
   */
  inline void to_string_column_major(char* buf, std::size_t size) const {
    auto result = std::format_to_n(buf, size,
      "|{0:^9.3f},{1:^9.3f},{2:^9.3f},{3:^9.3f}|\n"
      "|{4:^9.3f},{5:^9.3f},{6:^9.3f},{7:^9.3f}|\n"
      "|{8:^9.3f},{9:^9.3f},{10:^9.3f},{11:^9.3f}|\n"
      "|{12:^9.3f},{13:^9.3f},{14:^9.3f},{15:^9.3f}|\n",
      m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13],
      m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]);
    if (result.size < size)
      *result.out = '\0';
  }

  inline std::string to_string_column_major() const {
    char buf[256];
    to_string_column_major(buf, sizeof(buf));
    return buf;
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

Vec3 transform_vertex(const Matrix4x4 &M, const Vec3 &v);


Matrix4x4 look_at_view(const Vec3& eye,
                     const Vec3& target,
                     const Vec3& world_up = {0,1,0});

Matrix4x4 BuildViewMatrixFromEuler(float yaw, float pitch, float roll, const Vec3& pos);

/**
 *   How to use quaternions:
 *   -  w = cos(θ/2)
 *   -  (x, y, z) = axis * sin(θ/2)
 *   Where:
 *   -  θ is the rotation angle
 *   -  axis is a unit vector describing the direction of rotation
 *
 *   So if you rotate 90° around Y :
 *   -  θ = 90° → θ/2 = 45°
 *   -  w = cos(45°) = 0.707
 *   -  x = 0
 *   -  y = sin(45°) = 0.707
 *   -  z = 0
*/
struct Quaternion {
    float w, x, y, z;
};

inline Quaternion quat_identity() {
    return {1, 0, 0, 0};
}

/**
 * @brief Hamilton product: composes rotation 'a' followed by rotation 'b'.
 *
 * @details This is the quaternion equivalent of matrix multiplication.
 *          The result represents applying rotation 'a' first, then rotation 'b'.
 *
 * @note This operation is **non-commutative**: a * b ≠ b * a in general.
 *       - Left-multiplying (q * other) applies 'q' in the local (object) frame.
 *       - Right-multiplying (other * q) applies 'q' in the global (world) frame.
 *       - For Euler angles: yaw * orientation * pitch applies yaw in world space
 *         and pitch in local space (standard FPS/CAD camera convention).
 *
 * @param a First rotation quaternion (left side)
 * @param b Second rotation quaternion (right side)
 * @return Quaternion The composed rotation (a followed by b)
 */
inline Quaternion operator*(const Quaternion& a, const Quaternion& b) {
    return {
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
    };
}

inline Quaternion quat_from_axis_angle(const Vec3& axis, float angle) {
    float s = sinf(angle * 0.5f);
    return {
        cosf(angle * 0.5f),
        axis.x * s,
        axis.y * s,
        axis.z * s
    };
}

inline Vec3 rotate(const Quaternion& q, const Vec3& v) {
    Vec3 t = {
        2.0f * (q.y * v.z - q.z * v.y),
        2.0f * (q.z * v.x - q.x * v.z),
        2.0f * (q.x * v.y - q.y * v.x)
    };

    return {
        v.x + q.w * t.x + (q.y * t.z - q.z * t.y),
        v.y + q.w * t.y + (q.z * t.x - q.x * t.z),
        v.z + q.w * t.z + (q.x * t.y - q.y * t.x)
    };
}

inline void normalize(Quaternion& q) {
    float len = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    q.w /= len;
    q.x /= len;
    q.y /= len;
    q.z /= len;
}

struct Camera {
    Vec3 position;
    Quaternion orientation;
    float distance;
};

inline void camera_rotate(Camera& cam, float yaw, float pitch) {
    Quaternion qYaw   = quat_from_axis_angle({0,1,0}, yaw);
    Quaternion qPitch = quat_from_axis_angle({1,0,0}, pitch);

    // yaw in world space, pitch in local space
    cam.orientation = qYaw * cam.orientation * qPitch;
    normalize(cam.orientation);
}

inline void camera_zoom(Camera& cam, float delta) {
    cam.distance = std::max(0.1f, cam.distance + delta);
}

inline Vec3 camera_forward(const Camera& cam) {
    return rotate(cam.orientation, {0,0,-1});
}

inline Vec3 camera_right(const Camera& cam) {
    return rotate(cam.orientation, {1,0,0});
}

inline Vec3 camera_up(const Camera& cam) {
    return rotate(cam.orientation, {0,1,0});
}

inline void camera_move(Camera& cam, const Vec3& localMove, float speed) {
    cam.position += camera_right(cam)   * localMove.x * speed;
    cam.position += camera_up(cam)      * localMove.y * speed;
    cam.position += camera_forward(cam) * localMove.z * speed;
}


inline void camera_init_orbit(Camera& cam, const Vec3& target, float distance) {
    cam.distance    = distance;
    cam.orientation = quat_identity();

    // initial offset: camera sits behind the target
    Vec3 offset = {0, 0, distance};
    cam.position = target + offset;
}

inline void camera_init_fps(Camera& cam, const Vec3& pos, float yaw, float pitch) {
    cam.position = pos;

    Quaternion qYaw   = quat_from_axis_angle({0,1,0}, yaw);
    Quaternion qPitch = quat_from_axis_angle({1,0,0}, pitch);

    cam.orientation = qYaw * qPitch;
    normalize(cam.orientation);
}

inline void camera_orbit(Camera& cam, const Vec3& pivot, float yaw, float pitch) {
    Quaternion qYaw   = quat_from_axis_angle({0,1,0}, yaw);
    Quaternion qPitch = quat_from_axis_angle({1,0,0}, pitch);

    cam.orientation = qYaw * cam.orientation * qPitch;
    normalize(cam.orientation);

    // Orbit offset = rotated vector pointing backward by distance
    Vec3 offset = rotate(cam.orientation, {0, 0, cam.distance});
    cam.position = pivot + offset;
}

inline Matrix4x4 camera_view_matrix(const Camera& cam) {
    // Extract basis from quaternion
    Vec3 r = rotate(cam.orientation, {1,0,0});
    Vec3 u = rotate(cam.orientation, {0,1,0});
    Vec3 f = rotate(cam.orientation, {0,0,-1}); // forward is -Z

    Matrix4x4 M;
    M.m[0]  = r.x;   M.m[4]  = r.y;   M.m[8]  = r.z;   M.m[12] = -dot_product(r, cam.position);
    M.m[1]  = u.x;   M.m[5]  = u.y;   M.m[9]  = u.z;   M.m[13] = -dot_product(u, cam.position);
    M.m[2]  = -f.x;   M.m[6]  = -f.y;   M.m[10] = -f.z;   M.m[14] = dot_product(f, cam.position);
    M.m[3]  = 0;     M.m[7]  = 0;     M.m[11] = 0;     M.m[15] = 1;

    return M;
}

// produce sphericall coordinates for origin points.
Vec3 circular_orbit(float radius, float angle_radians, float height);
// produce sphericall coordinates for origin points the circle path is tansversal.
Vec3 circular_orbit_transversal(float radius, float angle_radians);

#endif // MATH_TYPES_H
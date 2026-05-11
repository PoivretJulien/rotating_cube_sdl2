#include "MathTypes.h"
#include <algorithm> // For std::max, std::min
#include <cmath>
#include <string>   // Needed for to_string()

// --- Function Implementations ---

Matrix4x4 multiply(const Matrix4x4& A, const Matrix4x4& B) {
    Matrix4x4 C;

    // Zero out result
    for (int i = 0; i < 16; ++i)
        C.m[i] = 0.0f;

    // Standard column-major matrix multiplication
    for (int j = 0; j < 4; ++j) {         // Column of C
        for (int i = 0; i < 4; ++i) {     // Row of C
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) { // Dot product
                sum += A.m[i + k*4] * B.m[k + j*4];
            }
            C.m[i + j*4] = sum;
        }
    }

    return C;
}

Matrix4x4 multiply_unrolled(const Matrix4x4& A, const Matrix4x4& B)
{
    Matrix4x4 C;

    // Column 0
    C.m[0]  = A.m[0]*B.m[0]  + A.m[4]*B.m[1]  + A.m[8]*B.m[2]   + A.m[12]*B.m[3];
    C.m[1]  = A.m[1]*B.m[0]  + A.m[5]*B.m[1]  + A.m[9]*B.m[2]   + A.m[13]*B.m[3];
    C.m[2]  = A.m[2]*B.m[0]  + A.m[6]*B.m[1]  + A.m[10]*B.m[2]  + A.m[14]*B.m[3];
    C.m[3]  = A.m[3]*B.m[0]  + A.m[7]*B.m[1]  + A.m[11]*B.m[2]  + A.m[15]*B.m[3];

    // Column 1
    C.m[4]  = A.m[0]*B.m[4]  + A.m[4]*B.m[5]  + A.m[8]*B.m[6]   + A.m[12]*B.m[7];
    C.m[5]  = A.m[1]*B.m[4]  + A.m[5]*B.m[5]  + A.m[9]*B.m[6]   + A.m[13]*B.m[7];
    C.m[6]  = A.m[2]*B.m[4]  + A.m[6]*B.m[5]  + A.m[10]*B.m[6]  + A.m[14]*B.m[7];
    C.m[7]  = A.m[3]*B.m[4]  + A.m[7]*B.m[5]  + A.m[11]*B.m[6]  + A.m[15]*B.m[7];

    // Column 2
    C.m[8]  = A.m[0]*B.m[8]  + A.m[4]*B.m[9]  + A.m[8]*B.m[10]  + A.m[12]*B.m[11];
    C.m[9]  = A.m[1]*B.m[8]  + A.m[5]*B.m[9]  + A.m[9]*B.m[10]  + A.m[13]*B.m[11];
    C.m[10] = A.m[2]*B.m[8]  + A.m[6]*B.m[9]  + A.m[10]*B.m[10] + A.m[14]*B.m[11];
    C.m[11] = A.m[3]*B.m[8]  + A.m[7]*B.m[9]  + A.m[11]*B.m[10] + A.m[15]*B.m[11];

    // Column 3
    C.m[12] = A.m[0]*B.m[12] + A.m[4]*B.m[13] + A.m[8]*B.m[14]  + A.m[12]*B.m[15];
    C.m[13] = A.m[1]*B.m[12] + A.m[5]*B.m[13] + A.m[9]*B.m[14]  + A.m[13]*B.m[15];
    C.m[14] = A.m[2]*B.m[12] + A.m[6]*B.m[13] + A.m[10]*B.m[14] + A.m[14]*B.m[15];
    C.m[15] = A.m[3]*B.m[12] + A.m[7]*B.m[13] + A.m[11]*B.m[14] + A.m[15]*B.m[15];

    return C;
}

Matrix4x4 BuildViewMatrixFromEuler(float yaw, float pitch, float roll, const Vector3& pos)
{
    Matrix4x4 M;

    // --- Build quaternion from yaw (Y), pitch (X), roll (Z) ---
    float cy = cosf(yaw   * 0.5f);
    float sy = sinf(yaw   * 0.5f);
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);
    float cr = cosf(roll  * 0.5f);
    float sr = sinf(roll  * 0.5f);

    // Quaternion multiplication order: q = yaw * pitch * roll
    // (Y * X * Z) — standard for FPS/CAD cameras
    float qw = cy*cp*cr + sy*sp*sr;
    float qx = cy*sp*cr + sy*cp*sr;
    float qy = sy*cp*cr - cy*sp*sr;
    float qz = cy*cp*sr - sy*sp*cr;

    // --- Extract basis vectors from quaternion ---
    // Right (+X)
    Vector3 right(
        1 - 2*(qy*qy + qz*qz),
        2*(qx*qy + qw*qz),
        2*(qx*qz - qw*qy)
    );

    // Up (+Y)
    Vector3 up(
        2*(qx*qy - qw*qz),
        1 - 2*(qx*qx + qz*qz),
        2*(qy*qz + qw*qx)
    );

    // Forward (-Z in OpenGL)
    Vector3 forward(
        2*(qx*qz + qw*qy),
        2*(qy*qz - qw*qx),
        1 - 2*(qx*qx + qy*qy)
    );

    // --- Build OpenGL view matrix (column-major) ---
    M.m[0] = right.x;
    M.m[1] = up.x;
    M.m[2] = -forward.x;
    M.m[3] = 0.0f;

    M.m[4] = right.y;
    M.m[5] = up.y;
    M.m[6] = -forward.y;
    M.m[7] = 0.0f;

    M.m[8]  = right.z;
    M.m[9]  = up.z;
    M.m[10] = -forward.z;
    M.m[11] = 0.0f;

    // Translation = -Rᵀ * pos
    M.m[12] = -(right.x * pos.x + right.y * pos.y + right.z * pos.z);
    M.m[13] = -(up.x    * pos.x + up.y    * pos.y + up.z    * pos.z);
    M.m[14] =  (forward.x * pos.x + forward.y * pos.y + forward.z * pos.z);
    M.m[15] = 1.0f;

    return M;
}

Matrix4x4 createPerspectiveProjectionMatrix(float fovY_deg, float aspectRatio, float nearZ, float farZ) {
    Matrix4x4 P {0};// zeroed init.

    float fovY = fovY_deg * (M_PI / 180.0f);
    float f = 1.0f / std::tan(fovY * 0.5f);

    float nf = 1.0f / (nearZ - farZ);

    // Column-major layout: m[row + col*4]

    // Column 0
    P.m[0] = f / aspectRatio;

    // Column 1
    P.m[5] = f;

    // Column 2
    P.m[10] = (farZ + nearZ) * nf;
    P.m[14] = (2.0f * farZ * nearZ) * nf;

    // Vulkan / DirectX perspective matrix formula.
    // DirectX/Vulkan z is from 0 to 1 (in opengl z is from -1 to 1)
    // replace the two previews lines by this:
    // P.m[10] = farZ / (farZ - nearZ);
    // P.m[14] = -(farZ * nearZ) / (farZ - nearZ);

    // Column 3
    P.m[11] = -1.0f;

    return P;
}

/**
 * @brief Multiplies a 3D vertex (treated as homogeneous coordinates) by a 4x4 matrix.
 * 
 * Performs R = M * V_homo and divides the result by W for perspective division.
 * Assumes column-major storage for Matrix4x4.
 *
 * @param M The transformation matrix.
 * @param v The input 3D vertex.
 * @return Vector3 The transformed 3D vertex.
 */
Vector3 transform_vertex(const Matrix4x4& M, const Vector3& v) {
    // Treat V as homogeneous coordinate (v.x, v.y, v.z, 1.0f)
    float x = v.x;
    float y = v.y;
    float z = v.z;
    float w_in = 1.0f;

    // Calculate the transformed coordinates (R = M * V_homo)
    // Components are calculated based on column-major storage: m[row + col*4]
    
    // R[0]: X component
    float newX = M.m[0] * x + M.m[4] * y + M.m[8] * z + M.m[12] * w_in;

    // R[1]: Y component
    float newY = M.m[1] * x + M.m[5] * y + M.m[9] * z + M.m[13] * w_in;

    // R[2]: Z component
    float newZ = M.m[2] * x + M.m[6] * y + M.m[10] * z + M.m[14] * w_in;

    // W component (for perspective division)
    float newW = M.m[3] * x + M.m[7] * y + M.m[11] * z + M.m[15] * w_in;

    if (std::abs(newW) > 1e-6f) {
        // Perform perspective division
        return {newX / newW, newY / newW, newZ / newW};
    } else {
        // Degenerate case or affine transform where w should be 1.0
        // Return the raw XYZ if W is near zero to prevent division by zero/large errors.
        return {newX, newY, newZ};
    }
}


Matrix4x4 look_at_view(const Vector3& eye,
                     const Vector3& target,
                     const Vector3& world_up)
{
    // Forward (camera looks toward -Z in view space)
    Vector3 f = subtract(target, eye);
    normalize(f);

    // Choose a stable up
    Vector3 up = world_up;

    if (fabs(dot_product(f, up)) > 0.99999f)
        up = {1,0,1};

    // Right = up × forward
    Vector3 r = cross_product(up, f);
    normalize(r);

    // Recompute true up = forward × right
    up = cross_product(f, r);

    // Column-major view matrix
    Matrix4x4 M;

    M.m[0]  = r.x;   M.m[4]  = r.y;   M.m[8]  = r.z;   M.m[12] = -dot_product(r, eye);
    M.m[1]  = up.x;  M.m[5]  = up.y;  M.m[9]  = up.z;  M.m[13] = -dot_product(up, eye);
    M.m[2]  = f.x;   M.m[6]  = f.y;   M.m[10] = f.z;   M.m[14] = -dot_product(f, eye);
    M.m[3]  = 0;     M.m[7]  = 0;     M.m[11] = 0;     M.m[15] = 1;

    return M;
}

Vector3 circular_orbit(float radius, float angle_radians, float height) {
  Vector3 pos;
  pos.x = radius * cos(angle_radians);
  pos.z = radius * sin(angle_radians);
  pos.y = height;
  return pos;
}

Vector3 circular_orbit_transversal(float radius, float angle_radians) {
  Vector3 pos;
  pos.z = radius * cos(angle_radians);
  pos.y = radius * sin(angle_radians);
  pos.x = 100;
  return pos;
}
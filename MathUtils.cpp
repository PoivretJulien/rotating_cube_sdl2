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
    if (fabs(dot_product(f, up)) > 0.999f)
        up = {1,0,0};

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

Matrix4x4 look_at_view_quat(const Vector3& eye,
                     const Vector3& target, Vector3& world_up)
{
    // 1. Compute forward
    Vector3 f = subtract(target, eye);
    normalize(f);

    // 2. Stable world-up
    if (fabs(dot_product(f, world_up)) > 0.999f)
        world_up  = {1, 0, 0};

    // 3. Build orthonormal basis
    Vector3 r = cross_product(world_up, f);
    normalize(r);

    Vector3 u = cross_product(f, r);
    normalize(u);

    // 4. Convert basis → quaternion (internal only)
    float m00 = r.x, m01 = u.x, m02 = f.x;
    float m10 = r.y, m11 = u.y, m12 = f.y;
    float m20 = r.z, m21 = u.z, m22 = f.z;

    float trace = m00 + m11 + m22;
    float qw, qx, qy, qz;

    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f;
        qw = 0.25f * s;
        qx = (m21 - m12) / s;
        qy = (m02 - m20) / s;
        qz = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        float s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
        qw = (m21 - m12) / s;
        qx = 0.25f * s;
        qy = (m01 + m10) / s;
        qz = (m02 + m20) / s;
    } else if (m11 > m22) {
        float s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
        qw = (m02 - m20) / s;
        qx = (m01 + m10) / s;
        qy = 0.25f * s;
        qz = (m12 + m21) / s;
    } else {
        float s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
        qw = (m10 - m01) / s;
        qx = (m02 + m20) / s;
        qy = (m12 + m21) / s;
        qz = 0.25f * s;
    }

    // 5. Extract basis from quaternion (stabilizes roll)
    auto rotate = [&](const Vector3& v) {
        // q * v * q^-1
        Vector3 t = {
            2.0f * (qy * v.z - qz * v.y),
            2.0f * (qz * v.x - qx * v.z),
            2.0f * (qx * v.y - qy * v.x)
        };

        return Vector3{
            v.x + qw * t.x + (qy * t.z - qz * t.y),
            v.y + qw * t.y + (qz * t.x - qx * t.z),
            v.z + qw * t.z + (qx * t.y - qy * t.x)
        };
    };

    r = rotate({1,0,0});
    u = rotate({0,1,0});
    f = rotate({0,0,1});

    // 6. Build column-major view matrix
    Matrix4x4 M;

    M.m[0]  = r.x;   M.m[4]  = r.y;   M.m[8]  = r.z;   M.m[12] = -dot_product(r, eye);
    M.m[1]  = u.x;   M.m[5]  = u.y;   M.m[9]  = u.z;   M.m[13] = -dot_product(u, eye);
    M.m[2]  = f.x;   M.m[6]  = f.y;   M.m[10] = f.z;   M.m[14] = -dot_product(f, eye);
    M.m[3]  = 0;     M.m[7]  = 0;     M.m[11] = 0;     M.m[15] = 1;

    return M;
}

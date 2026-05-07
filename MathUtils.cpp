#include "MathTypes.h"
#include <algorithm> // For std::max, std::min

// --- Function Implementations ---

Vector3 subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

float normalize(Vector3& v) {
    float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length > 1e-6f) { // Avoid division by zero or near-zero lengths
        v.x /= length;
        v.y /= length;
        v.z /= length;
    }
    return length;
}

Vector3 cross_product(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

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
    Matrix4x4 P;

    // Zero out the matrix
    for (int i = 0; i < 16; ++i)
        P.m[i] = 0.0f;

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

void camera_look_at(const Vector3& origin, const Vector3& target, 
                     Vector3& right, Vector3& up, Vector3& forward) {
    
    // Calculate Z-axis (Forward vector: from Origin to Target)
    forward = subtract(target, origin);
    normalize(forward);

    // Determine the "up" world direction (assuming Y is up in our scene)
    Vector3 temp_world_up = {0.0f, 1.0f, 0.0f}; 

    // Calculate X-axis (Right vector): Cross product of World Up and Forward
    right = cross_product(temp_world_up, forward);
    normalize(right);

    // Calculate Y-axis (Actual Up vector): Must be perpendicular to both Right and Forward
    up = cross_product(forward, right);
    normalize(up);
}

Matrix4x4 createViewMatrix(const Vector3& origin, const Vector3& target) {
    Vector3 right, up, forward;
    camera_look_at(origin, target, right, up, forward);

    Matrix4x4 view;

    // Zero out
    for (int i = 0; i < 16; ++i)
        view.m[i] = 0.0f;

    // Orientation (column-major)
    view.m[0]  = right.x;
    view.m[1]  = right.y;
    view.m[2]  = right.z;

    view.m[4]  = up.x;
    view.m[5]  = up.y;
    view.m[6]  = up.z;

    view.m[8]  = forward.x;
    view.m[9]  = forward.y;
    view.m[10] = forward.z;

    // Translation (last column)
    view.m[12] = - (right.x   * origin.x + right.y   * origin.y + right.z   * origin.z);
    view.m[13] = - (up.x      * origin.x + up.y      * origin.y + up.z      * origin.z);
    view.m[14] = - (forward.x * origin.x + forward.y * origin.y + forward.z * origin.z);

    // Homogeneous bottom-right
    view.m[15] = 1.0f;

    return view;
}

Matrix4x4 createViewMatrix_old(const Vector3& origin, const Vector3& target) {
    Vector3 right, up, forward;
    camera_look_at(origin, target, right, up, forward);

    // The View Matrix is the inverse transformation: T * R 
    Matrix4x4 view = Matrix4x4(); // Starts as Identity (defined in constructor)
    for(int i=0; i<16; ++i) view.m[i] = 0.0f; // Zero out identity components first

    // Column-major assignment for View Matrix components:
    view.m[0]  = right.x;   view.m[4] = right.y;   view.m[8] = right.z;
    view.m[1]  = up.x;     view.m[5] = up.y;     view.m[9] = up.z;
    view.m[2]  = forward.x; view.m[6] = forward.y; view.m[10] = forward.z;

    // Translation components (The negative dot products):
    // T_x = -(R * O)
    view.m[12] = -(right.x * origin.x + up.x * origin.y + forward.x * origin.z);
    // T_y = -(U * O)
    view.m[13] = -(right.y * origin.x + up.y * origin.y + forward.y * origin.z);
    // T_z = -(F * O)
    view.m[14] = -(right.z * origin.x + up.z * origin.y + forward.z * origin.z);

    return view;
}

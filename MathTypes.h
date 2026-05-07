// MathTypes.h - Contains common math structures and declarations for App3 3D rendering.
#ifndef MATH_TYPES_H
#define MATH_TYPES_H

#include <cmath>
#include <iostream> // Kept for compatibility if debugging, but ideally should be minimized.

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
};

/**
 * @brief 4x4 Matrix stored in column-major order (m[row + col*4])
 *        Assuming standard OpenGL/DirectX indexing convention for consistency with rendering logic.
 */
struct Matrix4x4 {
    // m[0] m[4] m[8] m[12] -> X components (Row 0)
    // m[1] m[5] m[9] m[13] -> Y components (Row 1)
    // m[2] m[6] m[10] m[14] -> Z components (Row 2)
    // m[3] m[7] m[11] m[15] -> W components (Row 3)
    float m[16]; 

    Matrix4x4() {
    for (int i = 0; i < 16; ++i)
        m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }

    inline std::string to_string(){
         return std::format(
            "Matrix id: {16}\n"
            "|{0:^9.3f},{1:^9.3f},{2:^9.3f},{3:^9.3f}|\n"
            "|{4:^9.3f},{5:^9.3f},{6:^9.3f},{7:^9.3f}|\n"
            "|{8:^9.3f},{9:^9.3f},{10:^9.3f},{11:^9.3f}|\n"
            "|{12:^9.3f},{13:^9.3f},{14:^9.3f},{15:^9.3f}|\n"
            ,
            m[0],m[1],m[2],m[3],
            m[4],m[5],m[6],m[7],
            m[8],m[9],m[10],m[11],
            m[12],m[13],m[14],m[15]
            ,id
         );
   }
};

// --- Linear Algebra Function Declarations ---

/**
 * @brief Helper function for vector subtraction (A - B).
 */
Vector3 subtract(const Vector3& a, const Vector3& b);

/**
 * @brief Helper function for normalization (calculating unit vector) and returns original length.
 */
float normalize(Vector3& v);

/**
 * @brief Calculates the cross product of two vectors (A x B).
 */
Vector3 cross_product(const Vector3& a, const Vector3& b);

/**
 * @brief Multiplies two matrices: Result = A * B (A is on the left).
 */
Matrix4x4 multiply(const Matrix4x4& A, const Matrix4x4& B);

/**
 * @brief Multiplies two matrices: Result = A * B (A is on the left).
 */
Matrix4x4 multiply_unrolled(const Matrix4x4& A, const Matrix4x4& B);

/**
 * @brief Creates a Perspective Projection Matrix (P).
 */
Matrix4x4 createPerspectiveProjectionMatrix(float fovY_deg, float aspectRatio, float nearZ, float farZ);


/**
 * @brief Calculates the orthonormal basis vectors [right, up, forward]. (Camera LookAt implementation)
 */
void camera_look_at(const Vector3& origin, const Vector3& target, 
                     Vector3& right, Vector3& up, Vector3& forward);

/**
 * @brief Builds a View Matrix that transforms coordinates from World Space to Camera View Space.
 */
Matrix4x4 createViewMatrix(const Vector3& origin, const Vector3& target);


#endif // MATH_TYPES_H

cmake_minimum_required(VERSION 3.10)
project(rotating_cube)

# if not installed install with -> "sudo apt install libsdl2-dev"
# Find SDL2 package
find_package(SDL2 REQUIRED)
# Include directories for SDL2
include_directories(${SDL2_INCLUDE_DIRS})

# Define the math utility source
set(MATH_SOURCES MathUtils.cpp)

# Target 1: The original version
add_executable(${PROJECT_NAME} main.cpp ${MATH_SOURCES})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
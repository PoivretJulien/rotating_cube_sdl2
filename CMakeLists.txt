cmake_minimum_required(VERSION 3.16)
project(rotating_cube)
set(CMAKE_CXX_STANDARD 23)

# SDL2 + SDL2_ttf via pkg-config (works on all Linux distros)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2TTF REQUIRED SDL2_ttf)

add_executable(${PROJECT_NAME}
    main.cpp
    MathUtils.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2TTF_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2TTF_LIBRARIES}
)

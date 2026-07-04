cmake_minimum_required(VERSION 3.18)
project(TwiniCudaGolf LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)

# Find SDL2
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Add source files
set(SOURCES
    src/main.cpp
    src/level_gen.cu
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
)

# CUDA architecture (Adjust based on your GPU)
set_target_properties(${PROJECT_NAME} PROPERTIES CUDA_ARCHITECTURES "75;80;86")
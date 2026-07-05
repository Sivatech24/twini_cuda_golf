cmake_minimum_required(VERSION 3.18)
project(TwiniCudaGolf LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)

# 1. Point to your exact layout inside C:/sdl
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH 
        "C:/sdl/SDL2-2.30.8"               # Your new layout path
        "C:/sdl/SDL2_ttf-2.24.0"
        "C:/sdl/SDL2_image-2.8.8" # Your updated development package path
    )
endif()

# 2. Find the packages
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_image REQUIRED) 

# 3. Add your source files
set(SOURCES
    src/main.cpp
    src/level_gen.cu
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# 4. Link the libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    SDL2_ttf::SDL2_ttf
    SDL2_image::SDL2_image
)

# Set CUDA Architecture (75=Turing, 80=Ampere, 86=Ada)
set_target_properties(${PROJECT_NAME} PROPERTIES CUDA_ARCHITECTURES "75;80;86")

# 5. Automatically copy Assets and DLLs to the build folder so the game runs
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # Copy 'res' folder
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
    
    # Copy DLLs out of the /lib/x64 directories directly to where the execution happens
    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
        "C:/sdl/SDL2-2.30.8/lib/x64/SDL2.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
        "C:/sdl/SDL2_ttf-2.24.0/lib/x64/SDL2_ttf.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
        "C:/sdl/SDL2_image-devel-2.8.8-VC/lib/x64/SDL2_image.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>/
)
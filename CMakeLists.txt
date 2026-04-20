cmake_minimum_required(VERSION 3.20)
project(SearchAssetsImGui)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)

include(FetchContent)

# Fetch GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# Fetch ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

# ViGEmClient – Xbox 360 virtual controller driver (Windows only)
if(WIN32)
    FetchContent_Declare(
        ViGEmClient
        GIT_REPOSITORY https://github.com/nefarius/ViGEmClient.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    set(ViGEmClient_DLL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ViGEmClient)
endif()

# Create ImGui library
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# Main executable
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")
add_executable(SearchAssetsV2 ${SOURCES})

target_include_directories(SearchAssetsV2 PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(SearchAssetsV2 PRIVATE imgui glfw OpenGL::GL)

# Keep as console application for better compatibility
if(WIN32)
    set_property(TARGET SearchAssetsV2 PROPERTY WIN32_EXECUTABLE FALSE)

    # Link ViGEmClient and its dependencies
    target_link_libraries(SearchAssetsV2 PRIVATE ViGEmClient::ViGEmClient setupapi)

    # Ensure windows macros are consistently defined
    target_compile_definitions(SearchAssetsV2 PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()
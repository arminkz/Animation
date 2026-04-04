cmake_minimum_required(VERSION 3.10)
project(VulkanEngine)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (WIN32)
    set(VULKAN_SDK $ENV{VULKAN_SDK})
    if (NOT VULKAN_SDK)
        message(FATAL_ERROR "Vulkan SDK not found! Make sure it's installed.")
    endif()
    set(CMAKE_INCLUDE_PATH ${VULKAN_SDK}/Include)
    set(CMAKE_LIBRARY_PATH ${VULKAN_SDK}/Lib)
endif()

find_package(Vulkan REQUIRED)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# Add dependencies
add_subdirectory(external/spdlog)
add_subdirectory(external/glm)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# ImGui sources
set(IMGUI_SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/backends/imgui_impl_sdl3.cpp
    external/imgui/backends/imgui_impl_vulkan.cpp
)

# Libraries
set(LIBRARIES
    Vulkan::Vulkan
    spdlog::spdlog
    glm::glm
    SDL3::SDL3
)

# Handle .cpp files
file(GLOB SOURCES
    src/*.cpp
    src/core/*.cpp
    src/geometry/*.cpp
    src/gui/*.cpp
    src/scene/*.cpp
    src/scene/camera/*.cpp
    src/scene/content/*.cpp
    src/scene/content/flag/*.cpp
    src/scene/content/table/*.cpp
    src/scene/content/jelly/*.cpp
    src/scene/content/boid/*.cpp
    src/scene/physics/*.cpp
    src/vulkan/*.cpp
    src/vulkan/resources/*.cpp
)
list(APPEND SOURCES ${IMGUI_SOURCES})

# Add resources.rc file to the sources if on Windows
if (WIN32)
    list(APPEND SOURCES resources.rc)
endif()

# Handle .h files
set(INCLUDES ${INCLUDES} src)
set(INCLUDES ${INCLUDES} external/stb)
set(INCLUDES ${INCLUDES} external/tinyobjloader)
set(INCLUDES ${INCLUDES} external/imgui)
set(INCLUDES ${INCLUDES} external/imgui/backends)

# Set asset directory definition
add_compile_definitions("ASSET_DIR=${CMAKE_SOURCE_DIR}/assets/")

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})
message(STATUS "I hate myself so I use cmake!")
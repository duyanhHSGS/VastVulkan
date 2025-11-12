cmake_minimum_required(VERSION 3.25)
project(VastVulkan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add cpp stuff (replace later)
file(GLOB SOURCES "src/*.cpp")
add_executable(my_project ${SOURCES})

# Include folder
target_include_directories(my_project PRIVATE include)

# Vulkan stuff
find_package(Vulkan REQUIRED)
target_include_directories(my_project PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(my_project PRIVATE ${Vulkan_LIBRARIES})

# Compile flags
target_compile_features(my_project PRIVATE cxx_std_23)
target_compile_options(my_project PRIVATE -Wall -Wextra -O3 -march=native -flto)

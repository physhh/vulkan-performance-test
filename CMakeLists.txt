cmake_minimum_required(VERSION 3.20)

project(vulkan-performance-test)

include("${CMAKE_BINARY_DIR}/conan_paths.cmake")
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(vulkan-memory-allocator REQUIRED)
find_package(shaderc REQUIRED)

add_executable(shaderc IMPORTED GLOBAL)
set_target_properties(shaderc PROPERTIES IMPORTED_LOCATION "${CONAN_SHADERC_ROOT}/bin/glslc")

file(GLOB_RECURSE sources CONFIGURE_DEPENDS
  "src/*.hpp"
  "src/*.cpp"
)

add_executable(memory-test 
  ${sources}
)
target_include_directories(memory-test PUBLIC "src")
target_link_libraries(memory-test
  PUBLIC 
    Vulkan::Vulkan 
    vulkan-memory-allocator::vulkan-memory-allocator
)

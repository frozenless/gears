cmake_minimum_required(VERSION 3.14)

project(Gears VERSION 0.0.1 LANGUAGES CXX)

add_subdirectory(external)
add_executable(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE Lamp)

target_sources(${PROJECT_NAME}
PRIVATE internal/gears.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES
RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/temp"
PDB_OUTPUT_DIRECTORY     "${CMAKE_SOURCE_DIR}/temp")
cmake_minimum_required(VERSION 3.31)
project(OrgEngineV2)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_compile_definitions(UNICODE)

# Set the output directory to App/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/App)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/App)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/App)

# Add only the Core subdirectory
add_subdirectory(3rdParty/fmt)
add_subdirectory(Source/Core)

# Define the main executable target
add_executable(OrgEngineV2 Source/main.cpp)

# Link the Core library
target_link_libraries(OrgEngineV2 PRIVATE Core fmt::fmt)

# Set output folder for the executable
set_target_properties(OrgEngineV2 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

if (MSVC)
    set(MAP_FILE "${CMAKE_BINARY_DIR}/App/OrgEngineV2.map")
    target_link_options(OrgEngineV2 PRIVATE "/MAP:${MAP_FILE}" /MAPINFO:EXPORTS)
endif()


# Include directories so headers are found cleanly
target_include_directories(OrgEngineV2 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/Platform
)

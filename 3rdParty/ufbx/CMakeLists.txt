# ufbx/CMakeLists.txt

# Create a static library for ufbx
add_library(ufbx STATIC ufbx.c)

set_target_properties(ufbx PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)

# Public include path
target_include_directories(ufbx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
    target_sources(ufbx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ufbxi.natvis)
endif()

# ufbx/CMakeLists.txt
project(ufbx LANGUAGES C CXX)

# Create a static library for ufbx
add_library(ufbx OBJECT ufbx.c ufbx.h)

target_compile_features(ufbx PRIVATE c_std_99 cxx_std_11)

# Public include path
target_include_directories(ufbx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if (MSVC)
    target_sources(ufbx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ufbxi.natvis)
endif()

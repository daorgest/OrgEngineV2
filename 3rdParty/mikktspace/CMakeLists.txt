project(mikktspace LANGUAGES C)

add_library(mikktspace STATIC mikktspace.c)

target_include_directories(mikktspace PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mikktspace PUBLIC c_std_99)
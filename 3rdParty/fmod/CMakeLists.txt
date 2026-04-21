project(FMOD LANGUAGES C CXX)

add_library(fmod_audio INTERFACE)
add_library(FMOD::FMOD ALIAS fmod_audio)

set(FMOD_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(fmod_audio INTERFACE
        "${FMOD_API_DIR}/core/inc"
        "${FMOD_API_DIR}/studio/inc"
)

target_link_directories(fmod_audio INTERFACE
        "${FMOD_API_DIR}/core/lib/x64"
        "${FMOD_API_DIR}/studio/lib/x64"
)

target_link_libraries(fmod_audio INTERFACE
        fmod_vc
        fmodstudio_vc
)

function(fmod_copy_dlls target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/core/lib/x64/fmod.dll"
            $<TARGET_FILE_DIR:${target_name}>

            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/studio/lib/x64/fmodstudio.dll"
            $<TARGET_FILE_DIR:${target_name}>
    )
endfunction()
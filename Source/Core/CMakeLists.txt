project(Core)

set(CORE_LIB_TYPE STATIC)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CORE_LIB_TYPE SHARED)
    add_compile_definitions(ORGAPI_DLL_EXPORT)
endif()

add_library(Core ${CORE_LIB_TYPE}
        PrimTypes.h
        Logger.h
        Platform/PlatformWindows.cpp
        Platform/Platform.h
        Renderer/RenderInterface.h
)

target_include_directories(Core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Platform
        ${CMAKE_CURRENT_SOURCE_DIR}/Renderer
)
target_link_libraries(Core PRIVATE fmt::fmt)

target_compile_definitions(Core PRIVATE
        $<$<CONFIG:Debug>:ORGAPI_DLL_EXPORT>
)


# Copy DLL to App folder in Debug (for shared builds)
if(CORE_LIB_TYPE STREQUAL "SHARED")
    add_custom_command(TARGET Core POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Core>
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>
            COMMENT "Copying Core.dll to App/$<CONFIG>"
    )
endif()
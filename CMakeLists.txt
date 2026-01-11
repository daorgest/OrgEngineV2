cmake_minimum_required(VERSION 4.0)

project(OrgEngineV2 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(MSVC)
    add_compile_options(/std:c++latest)
    # Required to make sure MSVC reports the correct __cplusplus version
    add_compile_options(/Zc:__cplusplus)
    # Good for engine dev: treat warnings as errors to keep code clean
    add_compile_options(/W4)
else()
    set(CMAKE_CXX_STANDARD 26)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(3RDPARTY_DIR  ${CMAKE_SOURCE_DIR}/3rdParty)
set(CORE_DIR ${CMAKE_SOURCE_DIR}/Source/Core)
set(ENGINE_DIR ${CMAKE_SOURCE_DIR}/Source/Engine)
set(GAME_DIR ${CMAKE_SOURCE_DIR}/Source/Game)
set(TOOLS_DIR ${CMAKE_SOURCE_DIR}/Source/Core/Tools)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

add_compile_definitions(
        UNICODE
        VK_NO_PROTOTYPES
        VOLK_STATIC_DEFINES
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
        EDITORUI
        IMGUI_IMPL_VULKAN_USE_VOLK
        _CRT_SECURE_NO_WARNINGS
        VULKAN_DEBUG_MODE
#        ENABLE_GPU_TIMING
        ENGINE_ENABLE_LOGGING # Soon
        ENGINE_PLATFORM_WIN32
        ENGINE_COMMIT_HASH="${GIT_HASH}"
#        TRACY_ENABLE
#        TRACY_CALLSTACK=10
)

set(ENGINE_PLATFORM "WIN32" CACHE STRING "Platform backend")
set_property(CACHE ENGINE_PLATFORM PROPERTY STRINGS WIN32 SDL)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/App)

foreach (cfg Debug RelWithDebInfo Release MinSizeRel)
    string(TOUPPER "${cfg}" CFGU)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFGU} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endforeach()

file(GLOB_RECURSE SLANG_ALL CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/Source/Game/Shaders/*.slang"
)

set(SLANG_GRAPHICS "${SLANG_ALL}")
list(FILTER SLANG_GRAPHICS EXCLUDE REGEX "(_comp|\\.comp)\\.slang$")

set(SLANG_COMP "${SLANG_ALL}")
list(FILTER SLANG_COMP INCLUDE REGEX "(_comp|\\.comp)\\.slang$")


foreach(shader ${SLANG_GRAPHICS})
    get_filename_component(name "${shader}" NAME_WE)
    set(out "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/${name}.spv")

    add_custom_command(
            OUTPUT "${out}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders"
            COMMAND "${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe"
            -o "${out}"
            -target spirv -profile sm_6_6
            -entry vertexMain -entry fragmentMain
            -emit-spirv-directly -fvk-use-scalar-layout
            "${shader}"
            DEPENDS "${shader}" "${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe"
            VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${out}")
endforeach()

foreach (shader ${SLANG_COMP})
    get_filename_component(name "${shader}" NAME_WE)
    set(out "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/${name}.spv")

    add_custom_command(
            OUTPUT "${out}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders"
            COMMAND "${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe"
            -o "${out}"
            -target spirv -profile sm_6_6
            -entry compMain
            -emit-spirv-directly -fvk-use-scalar-layout
            "${shader}"
            DEPENDS "${shader}" "${CMAKE_SOURCE_DIR}/3rdParty/slang/bin/slangc.exe"
            VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${out}")
endforeach()

find_package(Vulkan REQUIRED)

add_custom_target(Shaders ALL DEPENDS ${COMPILED_SHADERS})

set(SRC_FILES
        ${CMAKE_SOURCE_DIR}/Source/main.cpp
        ${TOOLS_DIR}/Arena.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanInit.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanDevice.cpp
        ${CORE_DIR}/Renderer/Vulkan/VmaUsage.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanSwapchain.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanTexture.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanCommands.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanCommandBuffer.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanPipeline.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanShader.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanBuffer.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanDescriptors.cpp
#        ${CORE_DIR}/Renderer/Vulkan/VulkanRenderPass.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanShaderBuffer.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanQueryPool.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanMesh.cpp
        ${ENGINE_DIR}/TextureLoader.cpp
        ${ENGINE_DIR}/MeshLoader.cpp
        ${CORE_DIR}/Math/MikkWrapper.cpp
        ${CORE_DIR}/Input/InputSys.cpp
        ${CORE_DIR}/Input/InputSysGameInput.cpp
        ${ENGINE_DIR}/Camera.cpp
        ${ENGINE_DIR}/EditorUi.cpp
        ${ENGINE_DIR}/FPSCamera.cpp
        ${GAME_DIR}/Application.cpp
        ${GAME_DIR}/DebugRenderer.cpp
        ${GAME_DIR}/SceneRenderer.cpp
        ${GAME_DIR}/SkyboxManager.cpp
        ${TOOLS_DIR}/MemStuff.cpp
)

if (ENGINE_PLATFORM STREQUAL "SDL")
    list(APPEND SRC_FILES ${CORE_DIR}/Platform/PlatformSDL.cpp Source/Core/Input/InputSystemSDL.cpp)
else ()
    list(APPEND SRC_FILES ${CORE_DIR}/Platform/PlatformWindows.cpp)
endif ()

set(HDR_FILES
        ${CORE_DIR}/Renderer/RendererTypes.h
        ${ENGINE_DIR}/MeshData.h
        ${ENGINE_DIR}/MeshStats.h
        ${ENGINE_DIR}/TextureLoader.h
        ${ENGINE_DIR}/MeshLoader.h
        ${ENGINE_DIR}/AABB.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanDevice.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanSwapchain.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanTexture.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanConvert.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanCommands.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanCommandBuffer.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanPipeline.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanShader.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanBuffer.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanCheck.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanDescriptors.h
#        ${CORE_DIR}/Renderer/Vulkan/VulkanRenderPass.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanDebugUtils.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanShaderBuffer.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanQueryPool.h
        ${CORE_DIR}/Renderer/Vulkan/VulkanMesh.h
        ${CORE_DIR}/Math/MikkWrapper.h
        ${CORE_DIR}/Input/InputSys.h
        ${CORE_DIR}/Input/InputSysGameInput.h
        ${ENGINE_DIR}/Camera.h
        ${ENGINE_DIR}/EditorUi.h
        ${ENGINE_DIR}/FPSCamera.h
        ${GAME_DIR}/Application.h
        ${GAME_DIR}/DebugRenderer.h
        ${GAME_DIR}/SceneRenderer.h
        ${GAME_DIR}/SkyboxManager.h
        ${CORE_DIR}/Tools/FileManager.h
        ${ENGINE_DIR}/FollowTarget.h
        ${ENGINE_DIR}/ShaderParams.h
        ${TOOLS_DIR}/Timer.h
        ${TOOLS_DIR}/UUID.h
)

set(IMGUI_FILES
        ${3RDPARTY_DIR}/imgui/imgui.cpp
        ${3RDPARTY_DIR}/imgui/imgui_demo.cpp
        ${3RDPARTY_DIR}/imgui/imgui_draw.cpp
        ${3RDPARTY_DIR}/imgui/imgui_tables.cpp
        ${3RDPARTY_DIR}/imgui/imgui_widgets.cpp
        ${3RDPARTY_DIR}/imgui/backends/imgui_impl_vulkan.cpp
        ${3RDPARTY_DIR}/imgui/imgui.h
        ${3RDPARTY_DIR}/imgui/imgui_internal.h
        ${3RDPARTY_DIR}/imgui/backends/imgui_impl_vulkan.h
)

if (ENGINE_PLATFORM STREQUAL "SDL")
    list(APPEND IMGUI_FILES
            ${3RDPARTY_DIR}/imgui/backends/imgui_impl_sdl3.cpp
            ${3RDPARTY_DIR}/imgui/backends/imgui_impl_sdl3.h
    )
else ()
    list(APPEND IMGUI_FILES
            ${3RDPARTY_DIR}/imgui/backends/imgui_impl_win32.cpp
            ${3RDPARTY_DIR}/imgui/backends/imgui_impl_win32.h
    )
endif ()

set(3RDPARTY_HEADERS
        ${3RDPARTY_DIR}/stb/stb_image.h
        ${3RDPARTY_DIR}/tinyobjloader/tiny_obj_loader.h
        ${3RDPARTY_DIR}/imoguizmo/imoguizmo.hpp
)

list(APPEND SRC_FILES ${IMGUI_FILES})
list(APPEND HDR_FILES ${3RDPARTY_HEADERS})

# NATVIS for Visual Studio debugger / OR ANY DEBUGGER THAT SUPPORTS IT
set(NATVIS_FILES ${CMAKE_SOURCE_DIR}/OrgEngineV2.natvis
        Source/Core/Tools/Logger.cpp
        Source/Engine/ShaderConstants.h
        Source/Engine/DefaultTextures.h)

add_executable(OrgEngineV2 ${SRC_FILES} ${HDR_FILES} ${NATVIS_FILES})

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SRC_FILES} ${HDR_FILES})

target_include_directories(OrgEngineV2 PRIVATE
        ${CORE_DIR}
        ${CORE_DIR}/Platform
        ${CORE_DIR}/Renderer
        ${CORE_DIR}/Renderer/Vulkan
        ${CORE_DIR}/Math
        ${ENGINE_DIR}
        ${GAME_DIR}
        ${3RDPARTY_DIR}/stb
        ${3RDPARTY_DIR}/tinyobjloader
        ${3RDPARTY_DIR}/imgui
        ${3RDPARTY_DIR}/imgui/backends
        ${3RDPARTY_DIR}/imoguizmo
        ${3RDPARTY_DIR}/fmt/include
        ${3RDPARTY_DIR}/volk
        ${3RDPARTY_DIR}/vma/include
        ${3RDPARTY_DIR}/meshoptimizer/src
        ${3RDPARTY_DIR}/mikktspace
        ${3RDPARTY_DIR}/glm
        ${3RDPARTY_DIR}/ufbx
        ${3RDPARTY_DIR}/tracy/public
        ${3RDPARTY_DIR}/slang/include
        ${3RDPARTY_DIR}/gameinput/include
        ${Vulkan_INCLUDE_DIRS}
)

add_subdirectory(3rdParty/fmt)
add_subdirectory(3rdParty/volk)
add_subdirectory(3rdParty/vma)
add_subdirectory(3rdParty/meshoptimizer)
add_subdirectory(3rdParty/mikktspace)
add_subdirectory(3rdParty/glm)
add_subdirectory(3rdParty/ufbx)

option(TRACY_ENABLE "" ON)
option(TRACY_ON_DEMAND "" ON)
option(TRACY_CALLSTACK "" 10)
add_subdirectory(3rdParty/tracy)

target_link_directories(OrgEngineV2 PRIVATE
#        ${3RDPARTY_DIR}/slang/lib
        ${3RDPARTY_DIR}/gameinput/lib
)

if (ENGINE_PLATFORM STREQUAL "SDL")
    add_subdirectory(3rdParty/sdl EXCLUDE_FROM_ALL)
    target_link_libraries(OrgEngineV2 PRIVATE SDL3::SDL3)
endif ()

target_precompile_headers(OrgEngineV2 PRIVATE
        ${CMAKE_SOURCE_DIR}/Source/PrimTypes.h
)

target_link_libraries(OrgEngineV2 PRIVATE
        fmt::fmt
        volk
        GPUOpen::VulkanMemoryAllocator
#        slang
        meshoptimizer
        mikktspace
        glm
        GameInput.lib
        ufbx
        Tracy::TracyClient
)

set_target_properties(OrgEngineV2 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_directory_properties(PROPERTIES VS_STARTUP_PROJECT OrgEngineV2)
set_target_properties(OrgEngineV2 PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

set(ASSET_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
set(ASSET_DST "${CMAKE_CURRENT_BINARY_DIR}/App/Assets")

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSET_SRC}"
        "${ASSET_DST}"
        COMMENT "Copying engine assets to build directory..."
)


if (MSVC)
    target_compile_options(OrgEngineV2 PRIVATE
            $<$<CONFIG:Debug>:/Zi /permissive->
            $<$<CONFIG:RelWithDebInfo>:/Zi>
            /MP /Zf
    )
    target_link_options(OrgEngineV2 PRIVATE
            $<$<CONFIG:Debug>:/DEBUG:FASTLINK /INCREMENTAL>
            $<$<CONFIG:RelWithDebInfo>:/DEBUG /INCREMENTAL>
            "/MAP:${CMAKE_BINARY_DIR}/App/OrgEngineV2.map" /MAPINFO:EXPORTS
    )
else()
    target_compile_options(OrgEngineV2 PRIVATE
            $<$<CONFIG:Debug>:-O0 -g>
            $<$<CONFIG:RelWithDebInfo>:-O2 -g>
    )
endif()

if (MSVC)
    target_compile_options(OrgEngineV2 PRIVATE /W4)
else()
    target_compile_options(OrgEngineV2 PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /RTC1 /MDd")

if (ENGINE_PLATFORM STREQUAL "WIN32")
    set_target_properties(OrgEngineV2 PROPERTIES
            WIN32_EXECUTABLE $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:TRUE>
    )
else ()
    set_target_properties(OrgEngineV2 PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})
add_dependencies(OrgEngineV2 CompileShaders)

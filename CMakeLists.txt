cmake_minimum_required(VERSION 4.0)

project(OrgEngineV2 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/App)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# --- Compiler Configuration ---
if(MSVC)
    add_compile_options(/std:c++latest /Zc:__cplusplus /W4 /MP /Zf)
else()
    set(CMAKE_CXX_STANDARD 26)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Path Definitions ---
set(3RDPARTY_DIR  ${CMAKE_SOURCE_DIR}/3rdParty)
set(CORE_DIR      ${CMAKE_SOURCE_DIR}/Source/Core)
set(ENGINE_DIR    ${CMAKE_SOURCE_DIR}/Source/Engine)
set(GAME_DIR      ${CMAKE_SOURCE_DIR}/Source/Game)
set(TOOLS_DIR     ${CMAKE_SOURCE_DIR}/Source/Core/Tools)

# --- Versioning ---
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# --- Platform Selection ---
set(ENGINE_PLATFORM "WIN32" CACHE STRING "Platform backend")
set_property(CACHE ENGINE_PLATFORM PROPERTY STRINGS WIN32 SDL)

# --- Source Lists ---
set(SRC_FILES
        Source/main.cpp
        ${TOOLS_DIR}/Arena.cpp
        ${TOOLS_DIR}/MemStuff.cpp
        ${TOOLS_DIR}/Logger.cpp
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
        ${CORE_DIR}/Renderer/Vulkan/VulkanShaderBuffer.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanQueryPool.cpp
        ${CORE_DIR}/Renderer/Vulkan/VulkanMesh.cpp
        ${CORE_DIR}/Math/MikkWrapper.cpp
        ${CORE_DIR}/Input/InputSys.cpp
        ${ENGINE_DIR}/TextureLoader.cpp
        ${ENGINE_DIR}/MeshLoader.cpp
        ${ENGINE_DIR}/Camera.cpp
        ${ENGINE_DIR}/EditorUi.cpp
        ${ENGINE_DIR}/FPSCamera.cpp
        ${GAME_DIR}/Application.cpp
        ${GAME_DIR}/DebugRenderer.cpp
        ${GAME_DIR}/SceneRenderer.cpp
        ${GAME_DIR}/SkyboxManager.cpp
)

set(IMGUI_FILES
        ${3RDPARTY_DIR}/imgui/imgui.cpp
        ${3RDPARTY_DIR}/imgui/imgui_demo.cpp
        ${3RDPARTY_DIR}/imgui/imgui_draw.cpp
        ${3RDPARTY_DIR}/imgui/imgui_tables.cpp
        ${3RDPARTY_DIR}/imgui/imgui_widgets.cpp
        ${3RDPARTY_DIR}/imgui/backends/imgui_impl_vulkan.cpp
)

# --- Shader Compilation Logic ---
file(GLOB_RECURSE SLANG_ALL CONFIGURE_DEPENDS "${GAME_DIR}/Shaders/*.slang")

foreach(shader ${SLANG_ALL})
    get_filename_component(name "${shader}" NAME_WE)
    set(out "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/${name}.spv")

    # Define entry points as a list
    set(ENTRY_POINTS "")
    if(shader MATCHES "(_comp|\\.comp)\\.slang$")
        list(APPEND ENTRY_POINTS -entry compMain)
    else()
        list(APPEND ENTRY_POINTS -entry vertexMain -entry fragmentMain)
    endif()

    add_custom_command(
            OUTPUT "${out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders"
            COMMAND "${3RDPARTY_DIR}/slang/bin/slangc.exe"
            "${shader}"
            -o "${out}"
            -target spirv
            -profile sm_6_6
            ${ENTRY_POINTS}
            -emit-spirv-directly
            -fvk-use-scalar-layout
            DEPENDS "${shader}" "${3RDPARTY_DIR}/slang/bin/slangc.exe"
            VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${out}")
endforeach()
add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})

# --- Executable and Dependencies ---
add_executable(OrgEngineV2 ${SRC_FILES} ${IMGUI_FILES})
add_dependencies(OrgEngineV2 CompileShaders)

# --- 3rd Party Subdirectories ---
add_subdirectory(${3RDPARTY_DIR}/fmt)
add_subdirectory(${3RDPARTY_DIR}/volk)
add_subdirectory(${3RDPARTY_DIR}/vma)
add_subdirectory(${3RDPARTY_DIR}/meshoptimizer)
add_subdirectory(${3RDPARTY_DIR}/mikktspace)
add_subdirectory(${3RDPARTY_DIR}/glm)
add_subdirectory(${3RDPARTY_DIR}/ufbx)
option(TRACY_ENABLE "" OFF)
add_subdirectory(${3RDPARTY_DIR}/tracy)

# --- Core Target Configuration ---
target_compile_definitions(OrgEngineV2 PRIVATE
        UNICODE
        VK_NO_PROTOTYPES
        VOLK_STATIC_DEFINES
        NOMINMAX
        EDITORUI
        IMGUI_IMPL_VULKAN_USE_VOLK
        _CRT_SECURE_NO_WARNINGS
        ENABLE_GPU_TIMING
        ENGINE_COMMIT_HASH="${GIT_HASH}"
)

target_include_directories(OrgEngineV2 PRIVATE
        ${CORE_DIR} ${CORE_DIR}/Platform
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
)

target_link_libraries(OrgEngineV2 PRIVATE
        fmt::fmt
        volk
        GPUOpen::VulkanMemoryAllocator
        meshoptimizer
        mikktspace
        glm
        ufbx
        Tracy::TracyClient
)

# --- Platform Specific Block ---
if (ENGINE_PLATFORM STREQUAL "SDL")
    target_compile_definitions(OrgEngineV2 PRIVATE ENGINE_PLATFORM_SDL)
    target_sources(OrgEngineV2 PRIVATE
            ${CORE_DIR}/Platform/PlatformSDL.cpp
            ${CORE_DIR}/Input/InputSystemSDL.cpp
            ${3RDPARTY_DIR}/imgui/backends/imgui_impl_sdl3.cpp
    )
    add_subdirectory(${3RDPARTY_DIR}/sdl EXCLUDE_FROM_ALL)
    target_link_libraries(OrgEngineV2 PRIVATE SDL3::SDL3)
else()
    target_compile_definitions(OrgEngineV2 PRIVATE ENGINE_PLATFORM_WIN32 VK_USE_PLATFORM_WIN32_KHR)
    target_sources(OrgEngineV2 PRIVATE
            ${CORE_DIR}/Platform/PlatformWindows.cpp
            ${CORE_DIR}/Input/InputSysGameInput.cpp
            ${3RDPARTY_DIR}/imgui/backends/imgui_impl_win32.cpp
    )
    target_include_directories(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/gameinput/include)
    target_link_directories(OrgEngineV2 PRIVATE ${3RDPARTY_DIR}/gameinput/lib)
    target_link_libraries(OrgEngineV2 PRIVATE GameInput.lib)

    set_target_properties(OrgEngineV2 PROPERTIES
            WIN32_EXECUTABLE $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:TRUE>
    )
endif()

# --- Post-Build Assets ---
add_custom_command(
        TARGET OrgEngineV2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets"
        COMMENT "Syncing assets..."
)

# --- Debug/Release Specifics ---
if(MSVC)
    target_link_options(OrgEngineV2 PRIVATE
            $<$<CONFIG:Debug>:/DEBUG:FASTLINK /INCREMENTAL>
            $<$<CONFIG:RelWithDebInfo>:/DEBUG /INCREMENTAL>
    )
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /RTC1 /MDd")
endif()

target_precompile_headers(OrgEngineV2 PRIVATE ${CMAKE_SOURCE_DIR}/Source/PrimTypes.h)

if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(OrgEngineV2 PRIVATE
            VULKAN_DEBUG_MODE
            ENGINE_ENABLE_LOGGING
    )
endif()